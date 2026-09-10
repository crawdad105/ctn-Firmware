
#include <3ds.h>
#include <stdio.h> // for vsprintf
#include <string.h> // for memset

#include <sys/socket.h>     // For socket functions
#include <netinet/in.h>     // For sockaddr_in and htons
#include <arpa/inet.h>      // For inet_pton function
#include "minisoc.h"

#include "ctn/file.h"
#include "ctn/ctn.h"
#include "ctn/network.h"
#include "ctn/memory_.h"
#include "ctn/newUI.h"
#include "ctn/threading.h"

// msg_buffer must be 32 or 16 in length
void Get_Error(int error_msg, char* msg_buffer, bool buffer_size_16) {
    // size 32 copy
    if (!buffer_size_16){
        switch (error_msg) { 
            case SUCCESS: strcpy(msg_buffer, "Operation Success\0"); break;
            case NULL_RETURN: strcpy(msg_buffer, "Null Operation Result\0"); break;
            case FAILED: strcpy(msg_buffer, "Operation Failed\0"); break;
        
            case FAILED_INIT_SOCKET: strcpy(msg_buffer, "Failed to Initialize Socket\0"); break;
            case FAILED_CREATE_SOCKET: strcpy(msg_buffer, "Failed to Create Socket\0"); break;
            case FAILED_CONNECT_SOCKET: strcpy(msg_buffer, "Failed to Connect Socket\0"); break;
            case ALREADY_CONNECTED_TO_SERVER: strcpy(msg_buffer, "Socket Already Connected\0"); break;
            case ALREADY_DISCONNECTED_FROM_SERVER: strcpy(msg_buffer, "Socket Already Disconnected\0"); break;

            case CLIENT_NOT_CONNECTED: strcpy(msg_buffer, "Socket Not Connected to Server\0"); break;
            case FAILED_TO_SEND_DATA: strcpy(msg_buffer, "Failed to Send Data to Server\0"); break;
            case FAILED_TO_RECEIVE_DATA: strcpy(msg_buffer, "Failed to Receive Server Data\0"); break;
        
            case FAILED_MEMORY_ALLOCATION: strcpy(msg_buffer, "Failed to Allocate Memory\0"); break;
        
            default: strcpy(msg_buffer, "Unknown Error\0"); break;
        }
    } 
    // size 16 copy
    else {
        switch (error_msg) {
            case SUCCESS: strcpy(msg_buffer, "Op. Success\0"); break;
            case NULL_RETURN: strcpy(msg_buffer, "Null Op. Res\0"); break;
            case FAILED: strcpy(msg_buffer, "Op. Fail\0"); break;
        
            case FAILED_INIT_SOCKET: strcpy(msg_buffer, "Soc Init Fail\0"); break;
            case FAILED_CREATE_SOCKET: strcpy(msg_buffer, "Soc Create Fail\0"); break;
            case FAILED_CONNECT_SOCKET: strcpy(msg_buffer, "Soc Conn. Fail\0"); break;
            case ALREADY_CONNECTED_TO_SERVER: strcpy(msg_buffer, "Soc Alr Discon.\0"); break;
            case ALREADY_DISCONNECTED_FROM_SERVER: strcpy(msg_buffer, "Soc Alr NConn.\0"); break;
        
            case CLIENT_NOT_CONNECTED: strcpy(msg_buffer, "Soc Not Conn.\0"); break;
            case FAILED_TO_SEND_DATA: strcpy(msg_buffer, "Send Fail\0"); break;
            case FAILED_TO_RECEIVE_DATA: strcpy(msg_buffer, "Receive Fail\0"); break;
        
            case FAILED_MEMORY_ALLOCATION: strcpy(msg_buffer, "Alloc Fail\0"); break;
        
            default: strcpy(msg_buffer, "Unknown Error\0"); break;
        }
    }
}

static int client_socket = -1;
static struct sockaddr_in server_addr;
static bool connectedToServer = false;
u64 sendDataTimer = 0;
int connectNetworkError = SUCCESS;
int disconnectNetworkError = SUCCESS;

int connectToServer(char ip[16], u16 port){
    if (connectedToServer) return ALREADY_CONNECTED_TO_SERVER;
    if (R_FAILED(miniSocInit())) return FAILED_INIT_SOCKET;

    client_socket = socSocket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) return FAILED_CREATE_SOCKET; // Failed to create socket

    int send_buffer_size = 0x1000 * 2;
    socSetsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size));
    int recv_buffer_size = 0x1000 * 2;
    socSetsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (socConnect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        disconnectFromServer();
        return FAILED_CONNECT_SOCKET;
    }

    connectedToServer = true;
    return SUCCESS;
}
int disconnectFromServer() {
    if (!connectedToServer) return ALREADY_DISCONNECTED_FROM_SERVER;
    connectedToServer = false;
    struct linger linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;
    socSetsockopt(client_socket, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));
    socClose(client_socket);
    miniSocExit();
    return SUCCESS;
}
int sendDataToServer(char* send_buffer, u32 buffer_size, u64* sendTimeMs) {
    if (!connectedToServer) return CLIENT_NOT_CONNECTED;

    u64 startTime = svcGetSystemTick();
    if (socSend(client_socket, send_buffer, buffer_size, 0) == -1) return FAILED_TO_SEND_DATA;
    
    if (sendTimeMs != NULL) *sendTimeMs = (svcGetSystemTick() - startTime) / TICKS_TO_MS;
    return SUCCESS;
}
int getDataFromServer(char* get_buffer, u32 buffer_size) {
    if (!connectedToServer) return CLIENT_NOT_CONNECTED;
    u32 total = 0;
    while (total < buffer_size) {
        int received = socRecv(client_socket, get_buffer + total, buffer_size - total, 0);
        if (received < 0) return FAILED_TO_RECEIVE_DATA;
        if (received == 0) return SERVER_DISCONNECTED; // server closed connection
        total += received;
    }
    return SUCCESS;
}


#define MAX_STRING 512
#define BUFFER_SIZE PAGE_SIZE
#define STACK_SIZE PAGE_SIZE
// thread stack
static u8 CTR_ALIGN(8) networkThreadStack[STACK_SIZE];
static ThreadData networkThread;
bool shouldRunNetworkThread = true;
bool networkThreadStarted = false;
u64 threadUpdateCounter = 0;
NetworkReadData readData;
NetworkHeader readHeader;
NetworkHeader sendHeader;
static char networkBuffer[BUFFER_SIZE];
static char networkTempString[MAX_STRING]; // ends at 0x00, 0x01
static char tempNetString[MAX_STRING];
static char tempBuffer[PAGE_SIZE - MAX_STRING - MAX_STRING]; // fills remaining page
typedef struct {
    char ip[16];
    u32 port;
} NetworkAddress;
NetworkAddress netAddr = { 0 };

extern bool ReadMemoryToBuffer(Process* proc, u32 address, char* buffer); // from proc_view.c

void networkThreadMain(void *param){
    // thread start
    NetworkAddress *addr = (NetworkAddress*)param;

    connectNetworkError = connectToServer(addr->ip, addr->port);

    memset((void*)&readData, 0, sizeof(NetworkReadData));
    memset((void*)&readHeader, 0, sizeof(NetworkHeader));
    memset((void*)&sendHeader, 0, sizeof(NetworkHeader));

    if (connectNetworkError >= 0){
        while(shouldRunNetworkThread){ // thread loop
            
            struct pollfd pfd;
            pfd.fd = client_socket;
            pfd.events = POLLIN;
            pfd.revents = 0;
            int pollres = socPoll(&pfd, 1, 10);
            if (pollres > 0 && (pfd.revents & POLLIN)){
                // this reads header first then reads the rest rather then reading the entire thing at once
                u32 readLen = 16;
                if (!readData.isReading){ // read header
                    connectNetworkError = getDataFromServer((char*)&readHeader, readLen);
                    if (connectNetworkError == FAILED_TO_RECEIVE_DATA){
                        // error, failed to read
                    }
                    if (readHeader.FF != 0xFF){
                        // error, reading header from not header
                    }
                    readData.isReading = true;
                    readData.currentBytes = 0;
                    readData.sectionCount = 0;
                    readData.targetBytes = readHeader.size;
                } else { // read non-header data
                    readLen = (readData.targetBytes - readData.currentBytes) < BUFFER_SIZE ? (readData.targetBytes - readData.currentBytes) : BUFFER_SIZE;
                    memset(networkBuffer, 0, BUFFER_SIZE);
                    connectNetworkError = getDataFromServer(networkBuffer, readLen);
                }
                readData.currentBytes += readLen;
                readData.sectionCount++;
                if (readData.currentBytes >= readData.targetBytes){ // check if complete reading data
                    readData.isReading = false;
                    
                }
                
                sendHeader.FF = 0xFF;
                sendHeader.type = readHeader.type;
                sendHeader.response = 1;
                sendHeader._reserved = 0;
                sendHeader.size = 0; //sizeof(NetworkHeader); set to 0 for clarity
                sendHeader.num2 = 0;
                sendHeader.num3 = 0;

                /*
                // debug send data to server
                sendHeader.type = STRING;
                sendHeader.resultError = 1;
                char temp[8] = {0};
                memcpy(temp, networkBuffer + readLen - 8, 8);
                memset(tempBuffer, 0, BUFFER_SIZE);
                sprintf(tempBuffer, "Read (%llu/%llu), %ld, %02X%02X%02X%02X%02X%02X%02X%02X (%d)", readData.currentBytes, readData.targetBytes, readData.sectionCount,
                    temp[0],
                    temp[1],
                    temp[2],
                    temp[3],
                    temp[4],
                    temp[5],
                    temp[6],
                    temp[7], connectNetworkError
                );
                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                sendHeader.FF = 0xFF;
                sendHeader.type = readHeader.type;
                sendHeader.response = 1;
                sendHeader._reserved = 0;
                sendHeader.size = 0; //sizeof(NetworkHeader); set to 0 for clarity
                sendHeader.num2 = 0;
                sendHeader.num3 = 0;
                */

                static FS_Archive archive = 0;
                static Handle file = 0;
                static u64 fileIndex = 0;
                // not including the 16 byte header, this is calculated if header is sent
                switch (readHeader.type) {
                    case TEST:{
                        sendHeader.num2 = (u32)(threadUpdateCounter & 0xFFFFFFFF);
                        sendHeader.size = sizeof(NetworkHeader);
                        connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                    } break;
                    case REQUEST_MEMORY:{
                        if (!ReadMemory(&targetProcess, readHeader.memoryAddress, (u8*)networkBuffer, PAGE_SIZE))
                            memset(networkBuffer, 0, BUFFER_SIZE); // clear memory if failed
                        sendHeader.size = sizeof(NetworkHeader) + PAGE_SIZE;
                        connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                        connectNetworkError = sendDataToServer(networkBuffer, PAGE_SIZE, &sendDataTimer);
                    } break;
                    case REQUEST_MEMORY_SELF:{
                        memset(networkBuffer, 0, BUFFER_SIZE);
                        u32 addr = readHeader.memoryAddress;

                        sendHeader.resultError = 0;
                        u32 curProcPid = 0;
                        if (R_FAILED(svcGetProcessId(&curProcPid, CUR_PROCESS_HANDLE))) {
                            sendHeader.resultError = 1;
                        } else {
                            Handle proc = 0;
                            if (R_FAILED(svcOpenProcess(&proc, curProcPid))){
                                sendHeader.resultError = 2;
                            } else {
                                MemInfo mInfo;
                                PageInfo pInfo;
                                if (R_FAILED(svcQueryMemory(&mInfo, &pInfo, addr))){
                                    sendHeader.resultError = 3;
                                } else {
                                    if (mInfo.perm & MEMPERM_READ){
                                        memcpy(networkBuffer, (void*)addr, PAGE_SIZE); // could read into non read memory
                                    } else{
                                        sendHeader.resultError = 4;
                                    }
                                }
                                svcCloseHandle(proc);
                            }
                        }
                        if (sendHeader.resultError != 0){
                            sendHeader.size = sizeof(NetworkHeader);
                            connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                        } else {
                            sendHeader.size = sizeof(NetworkHeader) + PAGE_SIZE;
                            connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                            connectNetworkError = sendDataToServer(networkBuffer, PAGE_SIZE, &sendDataTimer);
                        }
                    } break;
                    case REQUEST_DIRECTORY:{
                        u32 strSkip = 0;
                        if (readData.sectionCount == 1){ // first section (minimum packet is 40 bytes)
                            memset(networkTempString, 0, MAX_STRING);
                            strSkip = 16;
                        }
                        u32 strStart = strlen(networkTempString);
                        if (networkTempString[strStart + 1] != 1){ // if value after 0 is not 1 continue with string
                            for (u32 i = strSkip; i < readLen; i++) {
                                char c = networkBuffer[i];
                                if (strStart + i + 2 >= MAX_STRING){
                                    networkTempString[MAX_STRING - 2] = 0;
                                    networkTempString[MAX_STRING - 1] = 1;
                                    break; // string too long
                                }
                                networkTempString[strStart + i] = c;
                                if (c == 0){
                                    networkTempString[strStart + i + 1] = 1;
                                    break;
                                }
                            }
                        }
                        strStart = strlen(networkTempString);
                        if (networkTempString[strStart + 1] == 1){ // if string terminates properly, continue
                            static DirectoryData entries[32];
                            u32 count = 0;
                            if (enumerateFiles(networkTempString, entries, 32, &count, 0)){
                                memset(networkBuffer, 0, BUFFER_SIZE);
                                u32 length = 0;
                                for (u32 i = 0; i < count; i++) {
                                    u32 len = strlen(entries[i].name);
                                    length += len + 8 + 1;
                                }

                                sendHeader.size = sizeof(NetworkHeader) + length;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);

                                u32 index = 0;
                                for (u32 i = 0; i < count; i++) {
                                    u32 len = strlen(entries[i].name);
                                    if (index + len + 8 >= PAGE_SIZE){
                                        connectNetworkError = sendDataToServer(networkBuffer, index, &sendDataTimer); // send current data
                                        i--; // go back one element
                                        memset(networkBuffer, 0, BUFFER_SIZE); // clear data
                                        index = 0; // reset index
                                        continue;
                                    }
                                    u64 num = entries[i].fileSize;
                                    memcpy(networkBuffer + index, &num, 8);
                                    memcpy(networkBuffer + index + 8, entries[i].name, len);
                                    index += len + 8 + 1;
                                    if (entries[i].attributes & FS_ATTRIBUTE_DIRECTORY)
                                        networkBuffer[index - 1] = 1;
                                }
                                connectNetworkError = sendDataToServer(networkBuffer, index, &sendDataTimer); // send remaining data
                            } else {
                                sendHeader.type = STRING; // return string to give raw error
                                sendHeader.resultError = 1;
                                char *text = "Bad Path";
                                sendHeader.size = sizeof(NetworkHeader) + strlen(text) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(text, strlen(text) + 1, &sendDataTimer);
                            }
                        }
                    } break;
                    case UPLOAD_FILE:{
                        u32 strSkip = 0;
                        if (readData.sectionCount == 1){ // first section (minimum packet is 40 bytes)
                            memset(networkTempString, 0, MAX_STRING);
                            memset(tempNetString, 0, MAX_STRING);
                            strSkip = 16;
                        }
                        u32 strStart = strlen(networkTempString);
                        if (networkTempString[strStart + 1] != 1){ // if value after 0 is not 1 continue with string
                            for (u32 i = strSkip; i < readLen; i++) {
                                char c = networkBuffer[i];
                                if (strStart + i + 2 >= MAX_STRING){
                                    networkTempString[MAX_STRING - 2] = 0;
                                    networkTempString[MAX_STRING - 1] = 1;
                                    break; // string too long
                                }
                                networkTempString[strStart + i] = c;
                                if (c == 0){
                                    networkTempString[strStart + i + 1] = 1;
                                    break;
                                }
                            }
                        }
                        strStart = strlen(networkTempString);
                        if (networkTempString[strStart + 1] == 1){ // if string terminates properly, continue
                            fileIndex = 0;
                            if (!openArchive(&archive)){
                                sendHeader.type = STRING;
                                sendHeader.resultError = 1;
                                sprintf(tempBuffer, "Failed to open archive");
                                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                                break;                                
                            }
                            if (!openFile(archive, &file, networkTempString)){
                                sendHeader.type = STRING;
                                sendHeader.resultError = 1;
                                sprintf(tempBuffer, "Failed to open file");
                                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                                break;
                            }
                            u64 fileSize = 0;
                            if (!getFileSize(file, &fileSize)){
                                sendHeader.type = STRING;
                                sendHeader.resultError = 1;
                                sprintf(tempBuffer, "Failed to get file size");
                                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                                break;
                            }

                            sendHeader.type = readHeader.type;
                            sendHeader.resultError = 0;
                            sendHeader.size = sizeof(NetworkHeader) + fileSize;
                            connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                            
                            // used to send data after, we cant stop half way
                            bool failed = false;
                            while(fileIndex < fileSize){
                                u64 size = (fileSize - fileIndex) < BUFFER_SIZE ? (fileSize - fileIndex) : BUFFER_SIZE;
                                u32 read = 0;
                                if (!readFile(file, fileIndex, networkBuffer, (u32)size, &read)){
                                    memset(networkBuffer, 0, BUFFER_SIZE);
                                    failed = true;
                                }
                                connectNetworkError = sendDataToServer(networkBuffer, read, &sendDataTimer);
                                fileIndex += read;
                            }
                            
                            if (failed){
                                sendHeader.type = STRING;
                                sendHeader.resultError = 0;
                                sprintf(tempBuffer, "Done transfer failed, invalid data was sent");
                                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                            }

                            if (!readData.isReading){
                                closeFile(file);
                                closeArchive(archive);
                                sendHeader.type = STRING;
                                sendHeader.resultError = 0;
                                sprintf(tempBuffer, "Done sending transfer, %ld bytes", (u32)fileIndex);
                                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                            }
                        }
                    } break;
                    case DOWNLOAD_FILE:{
                        u32 strSkip = 0;
                        if (readData.sectionCount == 1){ // first section (minimum packet is 40 bytes)
                            memset(networkTempString, 0, MAX_STRING);
                            memset(tempNetString, 0, MAX_STRING);
                            strSkip = 16;
                        }
                        u32 strStart = strlen(networkTempString);
                        if (networkTempString[strStart + 1] != 1){ // if value after 0 is not 1 continue with string
                            for (u32 i = strSkip; i < readLen; i++) {
                                char c = networkBuffer[i];
                                if (strStart + i + 2 >= MAX_STRING){
                                    networkTempString[MAX_STRING - 2] = 0;
                                    networkTempString[MAX_STRING - 1] = 1;
                                    break; // string too long
                                }
                                networkTempString[strStart + i] = c;
                                if (c == 0){
                                    networkTempString[strStart + i + 1] = 1;
                                    break;
                                }
                            }
                        }
                        strStart = strlen(networkTempString);
                        if (networkTempString[strStart + 1] == 1){ // if string terminates properly, continue
                            u32 bufferSkip = 0;
                            if (strlen(tempNetString) == 0){ // start read
                                bufferSkip += strlen(networkTempString) + 1; // string length + null
                                openArchive(&archive);
                                if (getTempFile(networkTempString, tempNetString)){
                                    openCreateFile(archive, &file, tempNetString);
                                    fileIndex = 0;
                                }
                            }
                            if (!fileExists(tempNetString)){
                                sendHeader.type = STRING;
                                sendHeader.resultError = 1;
                                sprintf(tempBuffer, "Missing file, discarding %ld bytes", readLen);
                                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                            } else {
                                u32 read = 0;
                                if (!writeFile(file, fileIndex, networkBuffer + bufferSkip, readLen - bufferSkip, &read)){ // using +bufferSkip to skip string if applicable
                                    sendHeader.type = STRING;
                                    sendHeader.resultError = 1;
                                    sprintf(tempBuffer, "Failed file write %ld, %ld", (u32)fileIndex, read);
                                    sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                    connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                    connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                                }
                                fileIndex += read;
                            }
                            if (!readData.isReading){ // end read file
                                closeFile(file);
                                if (!moveFile(archive, tempNetString, networkTempString, true)){
                                    sendHeader.type = STRING;
                                    sendHeader.resultError = 0;
                                    sprintf(tempBuffer, "Failed to move file");
                                    sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                    connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                    connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);                                    
                                }
                                closeArchive(archive);
                                sendHeader.type = STRING;
                                sendHeader.resultError = 0;
                                sprintf(tempBuffer, "Done file transfer, %ld bytes", (u32)fileIndex);
                                sendHeader.size = sizeof(NetworkHeader) + strlen(tempBuffer) + 1;
                                connectNetworkError = sendDataToServer((char*)&sendHeader, 16, &sendDataTimer);
                                connectNetworkError = sendDataToServer(tempBuffer, strlen(tempBuffer) + 1, &sendDataTimer);
                            }
                        }
                    } break;
                    case STRING:{

                    } break;
                }

            }

            threadUpdateCounter++;

            if (pollres <= 0){ // wait 1 second if poll fails, otherwise dont wait since we may want to read immediately after
                svcSleepThread(1000ULL * TICKS_TO_MS); // 1 second
            }
        }
    }

    disconnectNetworkError = disconnectFromServer();

    // thread end
    networkThreadStarted = false;
}
bool StartNetworkThread(char* ip, u32 port){
    memset(netAddr.ip, 0, 16);
    memcpy(netAddr.ip, ip, 16);
    netAddr.ip[15] = 0;
    netAddr.port = port;
    void* param = (void*)&netAddr;
    networkThreadStarted = true;
    shouldRunNetworkThread = true;
    if (!Thread_CreateThread(&networkThread, networkThreadMain, param, networkThreadStack, STACK_SIZE, 52, 1)){ // 1 = CORE_SYSTEM, same as the rosalina thread
        networkThreadStarted = false;
        shouldRunNetworkThread = false;
        return false;
    }
    return true;
}
bool StopNetworkThread(){
    shouldRunNetworkThread = false;
    return !networkThreadStarted; // will probably always return false
}

bool CheckIp(char* buffer, u32 size, u32* ip){
    u32 nums[4] = { 0 };
    (*ip) = 0;
    u32 numIndex = 0;
    bool wasNum = false;
    for (u32 i = 0; i < size; i++) {
        char c = buffer[i];
        if (c == 0) break;
        if (c >= '0' && c <= '9'){
            nums[numIndex] *= 10;
            nums[numIndex] += (c - '0');
            if (nums[numIndex] > 0xFF) return false; // too large
            wasNum = true;
        } else if (c == '.'){
            if (wasNum){
                numIndex++;
                if (numIndex >= 4) return false; // too many sections
                wasNum = false;
            } else return false; // back to back dot
        } else return false; // invalid char
    }
    (*ip) = (nums[3] << 0) | (nums[2] << 8) | (nums[1] << 16) | (nums[0] << 24);
    return true;
}
bool CheckPort(char* buffer, u32 size, u32* port){
    (*port) = 0;
    for (u32 i = 0; i < size; i++) {
        char c = buffer[i];
        if (c == 0) break;
        if (c >= '0' && c <= '9'){
            (*port) *= 10;
            (*port) += (c - '0');
            if ((*port) > 0xFFFF) return false; // too large
        } else return false; // invalid char
    }
    return true;
}

extern void DrawDefaultUpperScreenStuff();
extern void DefaultWindowStart();
extern void DefaultWindowEnd();
extern Window* CreateWindow(char* title, u32 x, u32 y, u32 w, u32 h, void (*OnDraw)(Window *window), void (*OnUpdate)(Window *window));

static void netWinUpdate(Window *window){
    window->visible = networkThreadStarted;
    if (!networkThreadStarted)
        window->active = false;
}
static void netWinDraw(Window *window){
    if (!window->minimized){
        u32 top = window->y + 9 + PADDING;
        u32 left = window->x + PADDING;
        UI_DrawStringFormatSized(256, left, top, 0x000000, "n: %lld\nt: %lld\ncE: %d\ndE: %d\n"
            "%02X%02X%02X%02X\n%08X\n%08X\n%08X", 
            threadUpdateCounter, sendDataTimer, connectNetworkError, disconnectNetworkError,
            readHeader.FF, readHeader.type, readHeader.response, readHeader._reserved, readHeader.size, readHeader.num2, readHeader.num3
        );
    }
}
MenuResult ctn_Menu_Networking(){
    static Window *netWin = { 0 };

    static Button backButton = { 0 };
    static Button ipButton = { 0 };
    static Button portButton = { 0 };
    static Button connectButton = { 0 };
    static u32 ip = 0;
    static u32 port = 0;
    MENU_ONCE_START{
        backButton =    UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50,  20, "Back",    BASE_COLOUR, 0x000000);

        u32 x = PADDING * 2;
        u32 w = 4 + (20 * SPACING_X);
        u32 y = PADDING * 3 + 20 + SPACING_X;
        ipButton =      UI_CreateButton(x, y, w, 20, "IP:", BASE_COLOUR, 0x000000);
        netWin = CreateWindow("Net Info", ipButton.x + ipButton.w + PADDING, ipButton.y, 75, 13 + (SPACING_Y * 8) + 4, netWinDraw, netWinUpdate);
        portButton =    UI_CreateButton(x, ipButton.y + ipButton.h + PADDING, w, 20, "Port:",   BASE_COLOUR, 0x000000);
        connectButton = UI_CreateButton(x, portButton.y + portButton.h + PADDING, w, 20, "Connect", BASE_COLOUR, 0x000000);
    } MENU_ONCE_END;

    // "IP: 000.000.000.000"
    // "Post: 00000"
    char tempBuffer[128]; // 32 has issues, not sure why though
    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Networking");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;
        
        UI_DrawString(PADDING * 2, PADDING * 2 + 20, 0x00000000, "Used for connecting to custom debug server.");

        memset(tempBuffer, 0, sizeof(tempBuffer));
        sprintf(tempBuffer, "IP: %ld.%ld.%ld.%ld", (ip & 0xFF000000) >> 24, (ip & 0xFF0000) >> 16, (ip & 0xFF00) >> 8, (ip & 0xFF) >> 0);
        ipButton.str = tempBuffer;
        MENU_DO_BUTTON(ipButton) {
            memset(tempBuffer, 0, sizeof(tempBuffer));
            UI_GetUserInput("Input an IPv4 IP.", 32, tempBuffer);
            if (!CheckIp(tempBuffer, 32, &ip)){
                ip = 0;
                UI_DisplayMessageFormat("Invalid IP \"%s\"", tempBuffer);
            }
        }

        memset(tempBuffer, 0, sizeof(tempBuffer));
        sprintf(tempBuffer, "Port: %ld", port);
        portButton.str = tempBuffer;
        MENU_DO_BUTTON(portButton) {
            memset(tempBuffer, 0, sizeof(tempBuffer));
            UI_GetUserInput("Input a port.", 32, tempBuffer);
            if (!CheckPort(tempBuffer, 32, &port)){
                port = 0;
                UI_DisplayMessageFormat("Invalid port \"%s\"", tempBuffer);
            }
        }
        
        MENU_DO_BUTTON(connectButton) {
            if (networkThreadStarted){
                if (!StopNetworkThread()){
                    //UI_DisplayMessage("Failed to stop thread");
                }
            } else {
                memset(tempBuffer, 0, sizeof(tempBuffer));
                sprintf(tempBuffer, "%ld.%ld.%ld.%ld", (ip & 0xFF000000) >> 24, (ip & 0xFF0000) >> 16, (ip & 0xFF00) >> 8, (ip & 0xFF) >> 0);
                if (!StartNetworkThread(tempBuffer, port)){
                    //UI_DisplayMessage("Failed to start thread");
                } else {
                    netWin->active = true;
                }
            }
        }
        connectButton.str = networkThreadStarted ? "Disconnect" : "Connect";

        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
    } MENU_LOOP_END();
    
    return MENU_RESULT_OK;
}