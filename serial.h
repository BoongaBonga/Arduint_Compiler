#ifndef ARDUINT_SERIAL_H
#define ARDUINT_SERIAL_H

#include <Windows.h>
#include <stdbool.h>
#include <stdint.h>

#define PORT "\\\\.\\COM4"
#define BAUDRATE 115200

#define OPENCOMPORT_ERROR 100
#define GETCOMMSTATE_ERROR 101
#define SETCOMMSTATE_ERROR 102
#define PURGECOMMBUF_ERROR 103
#define SETTIMEOUT_ERROR 104
#define READFILE_ERROR 105

#define GETCHAR_COM_ERROR -1
#define GETCHAR_NO_DATA_RECEIVED -2

typedef enum {
  PACKET_HEAD_SYNC = (uint8_t)200,
  PACKET_HEAD_ASYNC = (uint8_t)201
} PacketHeader;

#define PACKET_NON_ARG_BYTES                                                   \
  8 // The amount of bytes in the CommPacket that are not arguments
typedef struct {
  PacketHeader packetHeader; // Just a byte indicating a packet send has started
  uint32_t id;      // A unique ID for this instruction for debug purposes
  uint8_t argCount; // Length of the arguments
  uint8_t *args;    // Argument list (max 124 bytes)
} CommPacket;

typedef struct {
  HANDLE commHandle;
} CommController;

void startupCommController(char *port);
void exitCommController();
void pushSerialCommand(uint8_t op);
void pushSerialCommands(int amount, const uint8_t *opList);

/// @brief Open a new serial handle
/// @return HANDLE | NULL
HANDLE openHandle(char *port);

/// @brief Configures an opened Serial handle
/// @param hComm Serial port Handle
/// @return 0: success, anything else is an error and it will be autmatically
/// printed.
int configureDCB(HANDLE hComm);

/// @brief Tries reading the COM input
/// @param hComm handle on the COM port
/// @return The character read. GETCHAR_COM_ERROR if an error was encountered
/// and GETCHAR_NO_DATA_RECEIVED if no data was received
int16_t commGetChar(HANDLE hComm);

bool commSendChar(HANDLE hComm, char character);

/// @brief Sends a given packet over serial.
/// @param hComm handle to serial port
/// @param packet the packet to send
/// @return whether the sending was succesfull
bool sendCommPacket(HANDLE hComm, volatile CommPacket *packet);

#endif