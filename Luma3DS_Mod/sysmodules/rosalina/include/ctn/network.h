#pragma once

#include <3ds.h>

enum ERROR_MSG {
    SUCCESS = 1, // not an error
    NULL_RETURN = 0, // not an error but not a success
    FAILED = -1, // failed but not specified

    FAILED_INIT_SOCKET = -100,      // failed to initialize socket
    FAILED_CREATE_SOCKET = -101,    // failed to create a socket
    FAILED_CONNECT_SOCKET = -102,   // failed to connect to server
    ALREADY_CONNECTED_TO_SERVER = -103,
    ALREADY_DISCONNECTED_FROM_SERVER = -104,

    CLIENT_NOT_CONNECTED = -200,    // not connect to server
    FAILED_TO_SEND_DATA = -201,     // failed to send data to server
    FAILED_TO_RECEIVE_DATA = -202,  // failed to get data from server
    SERVER_DISCONNECTED = -203,     // server closed connection

    FAILED_MEMORY_ALLOCATION = -300,
};

enum MSG_TYPE {
    TEST,
    REQUEST_MEMORY,
    REQUEST_MEMORY_SELF,
    REQUEST_DIRECTORY,
    /// <summary> Take file from 3ds </summary>
    UPLOAD_FILE,
    /// <summary> Give file to 3ds </summary>
    DOWNLOAD_FILE,
    STRING
};

typedef struct NetworkHeader {
    u8 FF;
    u8 type;
    u8 response;
    u8 _reserved;
    u32 size;
    union  {
        u32 num2;
        u32 memoryAddress;
        u32 resultError;
    };
    u32 num3;
} NetworkHeader;
typedef struct NetworkReadData {
    u64 targetBytes;
    u64 currentBytes;
    u32 sectionCount;
    bool isReading;
} NetworkReadData;

void Get_Error(int error_msg, char* msg_buffer, bool buffer_size_16);

extern u64 sendDataTimer;
extern int connectNetworkError;
extern int disconnectNetworkError;

int connectToServer(char ip[16], u16 port);
int disconnectFromServer();
int sendDataToServer(char* send_buffer, u32 buffer_size, u64* sendTimeMs);
int getDataFromServer(char* get_buffer, u32 buffer_size);

extern bool shouldRunNetworkThread;
extern bool networkThreadStarted;
extern u64 threadUpdateCounter;
extern NetworkHeader readHeader;

bool StartNetworkThread();
bool StopNetworkThread();