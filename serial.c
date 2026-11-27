#ifndef ARDUINT_SERIAL_C
#define ARDUINT_SERIAL_C

#include <Windows.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "cmdLineFlags.h"
#include "collection.h"
#include "common.h"
#include "debug.h"
#include "errorHandling.h"
#include "serial.h"

#define COMM_BUFFER_SIZE 1024
#define ARDUINO_BUFFER_SIZE 256
#define COMM_BUFFER_MAX_TIME 5                              // Milliseconds
#define COMM_BUFFER_OUTPUT_FREQ 1000 / COMM_BUFFER_MAX_TIME // /s

typedef struct {
  pthread_t thread;
  bool isRunning;
} ThreadWorker;

volatile bool keepRunning = true;
uint32_t packetIdCounter = 0;
HANDLE commHandle = INVALID_HANDLE_VALUE;

ThreadWorker commHandler;
RingBuffer commBuff;
static pthread_mutex_t commBuffMutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t unix_time_ms_windows(void) {
  FILETIME ft;
  GetSystemTimePreciseAsFileTime(&ft); /* Windows 8+ / Server 2012+ */

  uint64_t ticks_100ns =
      ((uint64_t)ft.dwHighDateTime << 32) | (uint64_t)ft.dwLowDateTime;

  const uint64_t EPOCH_DIFF_100NS = 116444736000000000ULL;
  if (ticks_100ns < EPOCH_DIFF_100NS) {
    return 0;
  }

  return (ticks_100ns - EPOCH_DIFF_100NS) / 10000ULL;
}

static void handleSerialOpenError(HANDLE hComm) {
  DWORD win32ErrorCode;   // Variable to store error code
  CHAR errorMessage[256]; // Character array to store error messages
  if (hComm == INVALID_HANDLE_VALUE) {
    win32ErrorCode = GetLastError();

    // Turn error code into messages
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
        win32ErrorCode, // error codeto be converted ,integer value like 2
        0,              // Language ID (0 = system default)
        errorMessage,   // buffer where the error message will be stored
        sizeof(errorMessage), NULL);
    defaultError("Error opening Arduino COM port: %s\n", errorMessage);
  }
  exit(OPENCOMPORT_ERROR);
}

HANDLE openHandle(char *port) {
  if (strlen(port) < strlen("\\\\.\\COM4")) {
    fprintf(stderr, "(0-0:0-0) %s[Error]: Invalid COM port '%s'.%s\n", COL_RED,
            port, AC_NORMAL);
    exit(2);
  }
  // Create handle with ANSI (8-bit strings)
  HANDLE commHandle =
      CreateFileA(port, // If COM is higher than 9 you must use "\\\\.\\COM10"
                  GENERIC_READ | GENERIC_WRITE,
                  0,             // No sharing
                  NULL,          // No security
                  OPEN_EXISTING, // Open existing port only
                  0,             // Non-overlapped I/O
                  NULL);         // Null for COM devices

  if (commHandle == INVALID_HANDLE_VALUE) {
    handleSerialOpenError(commHandle); // auto-exits program
  } else
    printf("%sOpening serial port succesfull%s\n", COL_GREEN, AC_NORMAL);

  return commHandle;
}

/// @brief Configures timeouts for a single file read
/// @param hComm handle of the file(COM port)
static BOOL setCommTimeoutConfiguration(HANDLE hComm) {
  COMMTIMEOUTS timeouts = {0};       // Zero out the COMMTIMEOUTS structure
  timeouts.ReadIntervalTimeout = 20; // Max time between bytes (ms)
  timeouts.ReadTotalTimeoutMultiplier = 1;  // Per-byte timeout
  timeouts.ReadTotalTimeoutConstant = 50;   // Constant timeout (ms)
  timeouts.WriteTotalTimeoutMultiplier = 1; // Per-byte timeout
  timeouts.WriteTotalTimeoutConstant = 50;  // Constant timeout (ms)
  return SetCommTimeouts(hComm, &timeouts); // finally set the timeouts
}

int configureDCB(HANDLE hComm) {
  DCB DCBStructParameter = {
      0}; // Zero out all the DCB struct members before calling GetCommState()

  DCBStructParameter.DCBlength =
      sizeof(DCBStructParameter); // Get the size of the DCB structure using
                                  // sizeof() update .DCBlength member with that
                                  // size required by GetCommState()
  // Get the current settings for the port hComm
  // and fill it in the DCB_Struct_Parameter struct
  if (!GetCommState(hComm, &DCBStructParameter)) {
    return GETCOMMSTATE_ERROR;
  }
#ifdef STATUS_PRINTING
  else
    printf("\nGetCommState() success");
#endif

  // Fill in the DCB with required settings
  DCBStructParameter.BaudRate = BAUDRATE;
  DCBStructParameter.ByteSize = 8; // Data word length
  DCBStructParameter.Parity = NOPARITY;
  DCBStructParameter.StopBits = ONESTOPBIT;
  DCBStructParameter.fDtrControl = DTR_CONTROL_ENABLE;
  DCBStructParameter.fRtsControl = RTS_CONTROL_ENABLE;

  if (!SetCommState(hComm, &DCBStructParameter)) {
    return SETCOMMSTATE_ERROR;
  }
#ifdef STATUS_PRINTING
  printf("\nSetCommState() success");
#endif

  // Clear out transmit & receive buffers
  if (!PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
    return PURGECOMMBUF_ERROR;
  }
#ifdef STATUS_PRINTING
  else
    printf("\nPurgeComm() success");
#endif

  // Configure timeouts
  if (!setCommTimeoutConfiguration(hComm)) {
    return SETTIMEOUT_ERROR;
  }
#ifdef STATUS_PRINTING
  else
    printf("\nSetTimeouts() success");
#endif

  return 0;
}

int16_t commGetChar(HANDLE hComm) {
  DWORD bytesRead;
  char receiveDataBuffer[2] = {0};

  // Return -1 in case of error, and -2 in case of no data
  if (!ReadFile(hComm, receiveDataBuffer, 1, &bytesRead, NULL))
    return GETCHAR_COM_ERROR;
  else if (bytesRead <= 0)
    return GETCHAR_NO_DATA_RECEIVED;

  return (int16_t)receiveDataBuffer[0] & 0xFF;
}

bool commSendChar(HANDLE hComm, char character) {
  DWORD bytesWritten = 0;

  if (!WriteFile(hComm, &character, sizeof(character), &bytesWritten, NULL)) {
    fprintf(stderr,
            "(0-0:0-0) %s[Error]: Error in Writing Byte to Serial Port%s\n",
            COL_RED, AC_NORMAL);
    return false;
  }

  FlushFileBuffers(hComm);
  return true;
}

bool sendCommPacket(HANDLE hComm, volatile CommPacket *packet) {
  uint8_t *charArr = malloc(packet->argCount + PACKET_NON_ARG_BYTES);
  uint16_t checkSum = 0;

  // Set Header
  charArr[0] = packet->packetHeader;
  checkSum += charArr[0];
  // Set ID
  charArr[1] = (uint8_t)(packet->id >> 24) & 0xFF;
  charArr[2] = (uint8_t)(packet->id >> 16) & 0xFF;
  charArr[3] = (uint8_t)(packet->id >> 8) & 0xFF;
  charArr[4] = (uint8_t)(packet->id) & 0xFF;
  checkSum += charArr[1];
  checkSum += charArr[2];
  checkSum += charArr[3];
  checkSum += charArr[4];
  // Set Arglen
  charArr[5] = packet->argCount;
  checkSum += charArr[5];
  // Set Args
  uint8_t *argPtr = &charArr[6];
  for (int i = 0; i < packet->argCount; i++) {
    uint8_t arg = packet->args[i];
    argPtr[i] = arg;
    checkSum += arg;
  }
  // Set Checksum
  charArr[packet->argCount + PACKET_NON_ARG_BYTES - 2] =
      (uint8_t)(checkSum >> 8);
  charArr[packet->argCount + PACKET_NON_ARG_BYTES - 1] = (uint8_t)checkSum;

#ifdef STATUS_PRINTING
  printf("Uploading: {\n");
  for (int i = 0; i < packet->argCount + PACKET_NON_ARG_BYTES; i++) {
    printf("%X, ", charArr[i]);
  }
  printf("\n}\n");
#endif

  // upload
  DWORD bytesWritten = 0;

  if (!WriteFile(hComm, charArr, packet->argCount + PACKET_NON_ARG_BYTES,
                 &bytesWritten, NULL)) {
    fprintf(stderr,
            "(0-0:0-0) %s[Error]: Error in Writing Bytes to Serial Port\n%s",
            COL_RED, AC_NORMAL);
    free(charArr);
    return false;
  } else {
    FlushFileBuffers(hComm);
    free(charArr);
    return true;
  }
}

static void initializeSerial() {
  commHandle = openHandle(cmdFlags.comPort);

  int result = configureDCB(commHandle);
  switch (result) {
  case 0: break;
  case GETCOMMSTATE_ERROR:
    defaultError("Error getting state of COM port.");
    exit(result);
  case SETCOMMSTATE_ERROR:
    defaultError("Error setting state of COM port.");
    exit(result);
  case PURGECOMMBUF_ERROR:
    defaultError("Error cleaning up COM port buffer.");
    exit(result);
  case SETTIMEOUT_ERROR:
    defaultError("Error setting timeouts of COM port.");
    exit(result);
  }

  printf("\n%sSerial initialized on PC. Waiting for Arduino reboot...%s\n",
         COL_YELLOW, AC_NORMAL);
  Sleep(2000); // Wait 2 seconds for Arduino bootloader

  // Wait until 0xAA is received
  while (keepRunning) {
    int16_t input = commGetChar(commHandle);
    if (input == 0xAA) break;
    if (input == GETCHAR_COM_ERROR) {
      defaultError("Error whilst trying to read serial input.");
      exit(READFILE_ERROR);
    }
    if (input == GETCHAR_NO_DATA_RECEIVED) continue;
    defaultError("(0-0:0-0) %s[Warning]: Received unexpected byte during "
                 "Arduino startup sequence: '%d'%s\n",
                 COL_YELLOW, input, AC_NORMAL);
  }

  printf("%sConnected to Arduino! Ready to execute program.%s\n\n", COL_GREEN,
         AC_NORMAL);
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
  if (fdwCtrlType == CTRL_C_EVENT) {
    keepRunning = false;
    printf("\n[Ctrl+C] Exiting...\n");

    if (commHandle != INVALID_HANDLE_VALUE) {
      // 1. Abort any ongoing read/write operations immediately
      PurgeComm(commHandle,
                PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
      // 2. Safely close the Windows kernel handle
      CloseHandle(commHandle);
      printf("\nCOM port closed cleanly.\n");
    }

    if (commHandler.isRunning) {
      pthread_cancel(commHandler.thread);
      pthread_join(commHandler.thread, NULL);
      printf("\nCOM process has exited.\n");
    }

    ExitProcess(0);
  }
  return TRUE;
}

void pushSerialCommand(uint8_t op) {
  while (true) {
    pthread_mutex_lock(&commBuffMutex);
    bool success = ringBuffPush(&commBuff, &op);
    pthread_mutex_unlock(&commBuffMutex);
    if (success) return;
    Sleep(1);
  }
}

void pushSerialCommands(int amount, const uint8_t *opList) {
  if (amount <= 0) return;
  if (amount > COMM_BUFFER_SIZE) return;
  if (opList == NULL) return;

  while (true) {
    pthread_mutex_lock(&commBuffMutex);
    bool success = ringBuffPushArr(&commBuff, amount, opList);
    pthread_mutex_unlock(&commBuffMutex);
    if (success) return;
    Sleep(1);
  }
}

void *bufferSerialPort(void *arg) {
  // Check if any serial commands have been pushed
  uint64_t lastPush = unix_time_ms_windows();

  int arduinoBuffCount = 0;
  uint8_t currentArduinoInstructions[ARDUINO_BUFFER_SIZE];

  while (true) {
    // Firstly, check if the Arduino has sent an instruction complete cmd

    pthread_mutex_lock(&commBuffMutex);
    // If the capacity is over halfway, OR the minimum time has elapsed, send
    // a COMMpacket
    if (unix_time_ms_windows() >= lastPush + COMM_BUFFER_MAX_TIME ||
        commBuff.count >= COMM_BUFFER_SIZE / 2) {
      CommPacket packet;
      packet.packetHeader = PACKET_HEAD_ASYNC;
      packet.id = packetIdCounter++;
      packet.argCount = commBuff.count;

      uint8_t argArr[commBuff.count];

      // Idk if this might fail someday
      ringBuffPopArr(&commBuff, commBuff.count, argArr);

      packet.args = argArr;

      sendCommPacket(commHandle, &packet);
      lastPush = unix_time_ms_windows();
    }
    pthread_mutex_unlock(&commBuffMutex);
    Sleep(1);
  }
  return NULL;
}

void startupCommController(char *port) {
  initializeSerial();
  commBuff = makeRingBuffer(sizeof(uint8_t), COMM_BUFFER_SIZE);

  pthread_create(&commHandler.thread, NULL, bufferSerialPort, NULL);

  SetConsoleCtrlHandler(CtrlHandler, TRUE);
}

void exitCommController() {
  CloseHandle(commHandle);

  pthread_cancel(commHandler.thread);
  pthread_join(commHandler.thread, NULL);
  printf("\nCOM process has exited.\n");
}

#endif