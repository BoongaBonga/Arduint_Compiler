#include <Windows.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serial.h"

// Handle for input handler thread
bool keepRunning = true;
HANDLE handleInputThread = INVALID_HANDLE_VALUE;

// Handle for comm thread
HANDLE commHandle = INVALID_HANDLE_VALUE;

// Counter for message Id's
uint32_t packetIdCounter = 0;

DWORD WINAPI inputThread(LPVOID arg) {
  char inputBuffer[256];

  // Initialise an array that will store all the commands
  uint8_t argList[120];

  while (keepRunning) {
    // Read in a command line
    if (fgets(inputBuffer, sizeof(inputBuffer), stdin) == NULL) {
      continue;
    }

    // Remove potential spaces and newlines
    if (/*inputBuffer[0] == '\n' ||*/ inputBuffer[0] == '\r') {
      continue;
    }

    // Check quitting
    if (inputBuffer[0] == 'q' || inputBuffer[0] == 'Q') {
      keepRunning = false;
      break;
    }

    // Start making a command packet
    CommPacket packet;

    // Set packet header & ID
    packet.packetHeader = SYNC;
    packet.id = packetIdCounter++;
    printf("id counter: %u, ", packet.id);

    // Populate the array
    int i = 0;
    uint8_t argCount = 0;
    bool validArgs = true;

    char *token = strtok(inputBuffer, " ,\r\n");

    while (token != NULL) {
      // Each byte must contain exactly two hex digits.
      if (strlen(token) != 2 || !isxdigit((unsigned char)token[0]) ||
          !isxdigit((unsigned char)token[1])) {
        printf("Error: enter bytes like C8 01 9B.\n");
        validArgs = false;
        break;
      }

      // Prevent writing beyond argList.
      if (argCount >= sizeof(argList)) {
        printf("Error: too many bytes (maximum %zu).\n", sizeof(argList));
        validArgs = false;
        break;
      }

      char *endptr;
      unsigned long value = strtoul(token, &endptr, 16);

      if (*endptr != '\0' || value > UINT8_MAX) {
        printf("Error: invalid hex byte.\n");
        validArgs = false;
        break;
      }

      argList[argCount] = (uint8_t)value;
      argCount++;

      token = strtok(NULL, " ,\r\n");
    }

    if (!validArgs || argCount == 0) {
      continue;
    }

    packet.argCount = argCount;
    printf("packet argCount: %u, ", argCount);
    packet.args = argList;

    sendCommPacket(commHandle, &packet);
  }

  return 0;
}

// Safely close serial comms upon interrupt
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

    ExitProcess(0);
  }
  return TRUE;
}

int main() {
  // Make an input handling thread
  handleInputThread = CreateThread(NULL, 0, inputThread, NULL, 0, NULL);
  SetConsoleCtrlHandler(CtrlHandler, TRUE);

  printf("Program running. Enter 'q' to quit.\n");

  // Open COM port
  commHandle = openHandle();
  if (commHandle == INVALID_HANDLE_VALUE) return 100;

  int result = configureDCB(commHandle);
  if (result != 0) return result;

  printf("\nSerial initialized on PC. Waiting for Arduino reboot...\n");
  Sleep(2000); // Wait 2 seconds for Arduino bootloader to hand off to your code
  // Wait until 0xAA is received
  while (keepRunning) {
    int16_t input = commGetChar(commHandle);
    if (input == 0xAA) break;
    if (input == GETCHAR_COM_ERROR) return -1;
  }
  printf("Ready! Enter your hex code:\n\n");

  while (keepRunning) {
    int16_t received = commGetChar(commHandle);

    if (received == GETCHAR_NO_DATA_RECEIVED) {
      continue;
    } else if (received == GETCHAR_COM_ERROR) {
      printf("\nError while reading COM\n");
      break;
    }

    printf("Received: %X\n", (uint8_t)received);
  }

  WaitForSingleObject(handleInputThread, INFINITE);
  CloseHandle(handleInputThread);

  printf("Exiting...");
  CloseHandle(commHandle);
  return 0;
}
