#ifndef ARDUINT_SERIAL_C
#define ARDUINT_SERIAL_C

#include <Windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "errorHandling.h"
#include "serial.h"

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
    defaultError("Error opening Arduino COM port: %s", errorMessage);
  }
  exit(OPENCOMPORT_ERROR);
}

HANDLE openHandle(char *port) {
  if (strlen(port) < strlen("\\\\.\\COM4")) {
    fprintf(stderr, "Invalid COM port '%s'.\n", port);
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
    handleSerialOpenError(commHandle);
    return NULL;
  } else
    printf("Opening serial port succesfull\n\n");

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
    printf("\nError in GetCommState()");
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
    printf("\nError in SetCommState()");
    return SETCOMMSTATE_ERROR;
  }
#ifdef STATUS_PRINTING
  printf("\nSetCommState() success");
#endif

  // Clear out transmit & receive buffers
  if (!PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
    printf("\nError in purging Comm buffers");
    return PURGECOMMBUF_ERROR;
  }
#ifdef STATUS_PRINTING
  else
    printf("\nPurgeComm() success");
#endif

  // Configure timeouts
  if (!setCommTimeoutConfiguration(hComm)) {
    printf("Error setting COM timeouts");
    return GETCHAR_SETTIMEOUT_ERROR;
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
    printf("\nError in Writing Byte to Serial Port");
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

  printf("checksum: %X, ", checkSum);

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
    printf("\nError in Writing Bytes to Serial Port");
    free(charArr);
    return false;
  } else {
    free(charArr);
    return true;
  }
  // FlushFileBuffers(hComm);
}

#endif