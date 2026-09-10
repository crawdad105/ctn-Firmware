// needs to have underscore otherwise he original memory.c file gets confused
#include <3ds.h>
#include <csvc.h>
#include "ctn/memory_.h"
#include "ctn/process.h"

#include <string.h> // memset and strcpy
#include <stdio.h> // for sprintf

// GDB uses "svc #0xFF" for both but they use 2 different values for each one so im using a single value for both (also because i dont know how find if a specific section of code is ARM or THUMB)
// this should be "svc #0xFF" in THUMB and "svc #0xDFFF" in ARM
u8 breakpointValueBuffer[4] = { 0xFF, 0xDF, 0x00, 0xEF };

// buffer to use for reading a single page, should be fine to use whenever
u8 dataBuffer[PAGE_SIZE] = {0};
ScanData Scan = {0};

// a heap may be better then this allocation method

// the top screen buffer ends at 0xD91A000 so i guess thats where we can start
// there should also be room after 0x0D026000, after the bottom screen buffer
static u32 mallocAddr = 0xD91A000;
#define MAX_REGIONS 32
// an attempt at reusing memory, its not ideal but better then naively using what came after the previous section
static memRegion regions[MAX_REGIONS] = { 0 };
static u32 regionCount = 0;
Result mem_alloc(memRegion *mem, u32 size, bool executable){
    size = (size + 0xFFF) & ~0xFFF;
    bool newRegion = true;
    u32 sizeDifference = 0xFFFFFFFF;
    u32 address = mallocAddr;
    u32 rIndex = regionCount;
    for (u32 i = 0; i < regionCount; i++) {
        if (regions[i].used == false && regions[i].size >= size){
            u32 diff = size - regions[i].size;
            // find best match
            if (diff < sizeDifference){
                // check if memory is free, assuming mem_alloc is called every time, this should not be necessary, but incase Luma3DS or a plugin sets memory to be writable then this checks it
                MemSectionInfo info = queryMemInfo(CUR_PROCESS_HANDLE, regions[i].address);
                if (info.failed || !(info.state & MEMSTATE_FREE) || info.size < size){ // if fail or not free or size is too small we cant use this memory
                    regions[i].used = true;
                    continue;
                }
                newRegion = false;
                rIndex = i;
                address = regions[i].address;
                sizeDifference = diff;
                // if found best match, quit
                if (diff == 0) break; 
            }
        }
    }
    
    Result res = svcControlMemoryEx(&(mem->address), address, 0, size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
    if (R_SUCCEEDED(res)){
        mem->size = size;
        mem->used = true;
        
        if (executable){
            // pseudo handles not aloud so we need to get a real one
            u32 pid = 0;
            if (R_SUCCEEDED(svcGetProcessId(&pid, CUR_PROCESS_HANDLE))){
                Handle h = 0;
                if (R_SUCCEEDED(svcOpenProcess(&h, pid))){
                    svcControlProcessMemory(h, mem->address, 0, mem->size, MEMOP_PROT, MEMPERM_READ | MEMPERM_WRITE | MEMPERM_EXECUTE); // set executable
                }
                svcCloseHandle(h);
            }
        }
        // if room runs out we dont really care we just lose the ability to reuse memory
        if (regionCount < MAX_REGIONS){
            memcpy(&regions[rIndex], mem, sizeof(memRegion));
            if (newRegion){
                mallocAddr += size;
                regionCount++;
            }
        }
        return true;
    } else return res;
}
Result mem_free(memRegion *mem){
    u32 tmp;
    Result res = svcControlMemory(&tmp, mem->address, 0, mem->size, MEMOP_FREE, 0);
    if (R_SUCCEEDED(res)){
        for (u32 i = 0; i < MAX_REGIONS; i++) {
            if (mem->address == regions[i].address){
                regions[i].used = false;
            }
        }
    }
    return res;
}

// returns true if memory was read otherwise false if the memory is free or fails to read
bool ReadMemory(Process *proc, u32 address, u8 *buffer, u32 bufferSize){
    MemInfo memInfo;
    PageInfo pageInfo;
    svcQueryDebugProcessMemory(&memInfo, &pageInfo, proc->debugHandle, address);
    if (address + bufferSize > memInfo.base_addr + memInfo.size) return false; // check if reading past section
    if (memInfo.perm & MEMPERM_READ || memInfo.perm & MEMPERM_WRITE || memInfo.perm & MEMPERM_EXECUTE){
        Result res = svcReadProcessMemory(buffer, proc->debugHandle, address, bufferSize);
        if (R_FAILED(res)){ // if failed change permissions then try again
            u32 originalPerms = memInfo.perm;
            u32 newPerms = MEMPERM_READ | MEMPERM_WRITE | memInfo.perm; // keep original permissions so if resetting permissions fails we only add read and writability
            if (R_FAILED(svcControlProcessMemory(proc->handle, memInfo.base_addr, memInfo.base_addr, memInfo.size, MEMOP_PROT, newPerms))) return false;
            res = svcReadProcessMemory(buffer, proc->debugHandle, address, bufferSize); // read entire page into buffer
            // reset permissions regardless of read state
            if (R_FAILED(svcControlProcessMemory(proc->handle, memInfo.base_addr, memInfo.base_addr, memInfo.size, MEMOP_PROT, originalPerms))){
                // not sure what happens here, i guess just continue 
            }
            if (R_FAILED(res)) return false; // no memory :(
            // else; it will return true
        }
    } else return false; // free memory wont have any permissions so this means the section is probably not going to be readable
    return true;
}
bool WriteMemory(Process *proc, u32 address, u8 *buffer, u32 bufferSize){
    MemInfo memInfo;
    PageInfo pageInfo;
    svcQueryDebugProcessMemory(&memInfo, &pageInfo, proc->debugHandle, address);
    if (address + bufferSize >= memInfo.base_addr + memInfo.size) return false; // check if reading past section
    if (memInfo.perm & MEMPERM_READ || memInfo.perm & MEMPERM_WRITE || memInfo.perm & MEMPERM_EXECUTE){
        Result res = svcWriteProcessMemory(proc->debugHandle, buffer, address, bufferSize);
        if (R_FAILED(res)){ // if failed change permissions then try again
            u32 originalPerms = memInfo.perm;
            u32 newPerms = MEMPERM_READ | MEMPERM_WRITE | memInfo.perm; // keep original permissions so if resetting permissions fails we only add read and writability
            if (R_FAILED(svcControlProcessMemory(proc->handle, memInfo.base_addr, memInfo.base_addr, memInfo.size, MEMOP_PROT, newPerms))) return false;
            res = svcWriteProcessMemory(proc->debugHandle, buffer, address, bufferSize); // read entire page into buffer
            // reset permissions regardless of read state
            if (R_FAILED(svcControlProcessMemory(proc->handle, memInfo.base_addr, memInfo.base_addr, memInfo.size, MEMOP_PROT, originalPerms))){
                // not sure what happens here, i guess just continue 
            }
            if (R_FAILED(res)) return false; // no memory :(
            // else; it will return true
        }
    } else return false; // free memory wont have any permissions so this means the section is probably not going to be writable
    svcFlushProcessDataCache(proc->handle, address, bufferSize);
    return true;
}

u32 SearchToNum(SearchTerm search){
    return search.gap | (search.wildcard << 8) | (search.searchByte << 16);
}

bool HexStringToNumLimit(char *string, u32 length, u32 *value){
    (*value) = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = string[i];
        if (c >= '0' && c <= '9'){
            (*value) <<= 4; // shift first so it can add the value to the start and if there is no value it will just 0 which will still be 0
            (*value) += (c - '0');
        } else if (c >= 'a' && c <= 'f'){
            (*value) <<= 4;
            (*value) += (c - 'a') + 10;
        } else if (c >= 'A' && c <= 'F'){
            (*value) <<= 4;
            (*value) += (c - 'A') + 10;
        } else return false;
    }
    return true;
}
bool HexStringToNum(char *string, u32 *value){
    u32 len = strlen(string);
    if (len >= 9) return false;
    return HexStringToNumLimit(string, len, value);
}
void CreatSearchTermList(char *searchString, SearchTerm *search, u32 maxSearchTerms, u32 *resultSearchTermCount){
    
    
    u32 len = strlen(searchString);
    u32 searchIndex = 0;
    u8 curByte = 0;
    bool bytePart2 = false;
    bool inString = false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = searchString[i];
        SearchTerm s = { 0 };
        s.gap = false;
        s.wildcard = false;
        s.searchByte = false;
        if (inString && c != '\"'){
            s.searchByte = c;
            search[searchIndex++] = s;
        } else {
            if (c == '?'){
                s.wildcard = true;
                search[searchIndex++] = s;
            } else if (c == '*'){
                s.gap = true;
                search[searchIndex++] = s;
            } else if (c == ' '){
                continue;
            } else if (c == '\"'){
                inString = !inString;
                continue;
            } else if (c >= '0' && c <= '9'){
                curByte += (c - '0');
                if (bytePart2){
                    s.searchByte = curByte;
                    curByte = 0;
                    search[searchIndex++] = s;
                } else {
                    curByte <<= 4; // shift 4
                }
                bytePart2 = !bytePart2;
            } else if (c >= 'a' && c <= 'f'){
                curByte += (c - 'a') + 10;
                if (bytePart2){
                    s.searchByte = curByte;
                    curByte = 0;
                    search[searchIndex++] = s;
                } else {
                    curByte <<= 4; // shift 4
                }
                bytePart2 = !bytePart2;
            } else if (c >= 'A' && c <= 'F'){
                curByte += (c - 'A') + 10;
                if (bytePart2){
                    s.searchByte = curByte;
                    curByte = 0;
                    search[searchIndex++] = s;
                } else {
                    curByte <<= 4; // shift 4
                }
                bytePart2 = !bytePart2;
            }
        }
        if (resultSearchTermCount != NULL) *resultSearchTermCount = searchIndex;
        if (searchIndex >= maxSearchTerms) return;
    }

}

// searchLength is the length of the intended search term length NOT the full count of search terms in the allocated space
// search start is rounded to page base of the start address, and ends at the page end of the end address
u64 SearchMemory(u32 start, u32 end, u32 debugHandle, SearchTerm *search, u32 searchLength, u32 *resultBuffer, u32 maxResults, u32 *foundResultCount, u32 *resultLengthBuffer, u64 maxSearchTime){ 

    u64 startTime = svcGetSystemTick();

    u32 resultCount = 0;
    memset(resultBuffer, 0, maxResults * sizeof(u32));

    static u8 buffer[PAGE_SIZE];

    u32 resultLength = 0;
    u32 searchIndex = 0;
    SearchTerm curSearch = search[searchIndex];

    u32 addr = PAGE_ROUND(start);
    u32 startOffset = start & 0xFFF; // start with remainder
    u32 foundStartAddr = start; // set to start this would normally be (addr + startOffset)

    bool readFail = false;
    Result readRes = svcReadProcessMemory(buffer, debugHandle, addr, PAGE_SIZE);
    if (R_FAILED(readRes)) readFail = true;
    else readFail = false;

    while(true){ // will either leave if max results are met or end of address space is reached
        if (readFail){
            readFail = false;
        } else {
            for (u32 i = startOffset; i < PAGE_SIZE; i++) {
                bool found = false;
                bool skip = false;
                u8 b = buffer[i];
                if (curSearch.gap){
                    if (searchIndex > 0 && searchIndex < searchLength && i < PAGE_SIZE - 1){  // only do gap if there is a previous and next search results
                        SearchTerm prevSearch = search[searchIndex - 1];
                        SearchTerm nextSearch = search[searchIndex + 1];
                        // check first incase X*X
                        if (buffer[i + 1] == nextSearch.searchByte){ // check if next byte is for next search (this will still have the next char be checked)
                            found = true;
                        } else if (b == prevSearch.searchByte){ // check if current byte was for previous search, technically this may not be wanted by makes more sense having it
                            found = false;
                        } else {
                            skip = true;
                        }
                    }
                } else if (curSearch.wildcard || b == curSearch.searchByte){
                    found = true;
                }
                if (skip){
                    resultLength++;
                } else {
                    if (found){
                        searchIndex++;
                        resultLength++;
                        if (searchIndex >= searchLength){
                            resultBuffer[resultCount] = foundStartAddr;
                            resultLengthBuffer[resultCount] = resultLength;
                            resultCount++;
                            if (resultCount >= maxResults) break; // return if max results found
                            found = false; // set to false so normal end search code runs
                        }
                    }
                    if (!found) {
                        foundStartAddr = addr + i + 1; // +1 because this is for the next byte
                        resultLength = 0;
                        searchIndex = 0;
                    }
                }
                curSearch = search[searchIndex];
            }
        }
        startOffset = 0;
        addr += PAGE_SIZE;
        if (maxSearchTime != 0 && (svcGetSystemTick() - startTime) > maxSearchTime) break;
        if (addr >= end || resultCount >= maxResults) break;
        if (addr >= end) break;
        Result readRes = svcReadProcessMemory(buffer, debugHandle, addr, PAGE_SIZE);
        if (R_FAILED(readRes)) readFail = true;
        else readFail = false;
    }

    *foundResultCount = resultCount;
    return svcGetSystemTick() - startTime;

}

inline static bool getBit(u8* buffer, u32 bitIndex) {
    return (buffer[bitIndex >> 3] & (1 << (bitIndex & 7))) > 0 ? true : false;
}
inline static bool setBit(u8* buffer, u32 bitIndex, bool value) { 
    buffer[bitIndex >> 3] &= ~(1 << (bitIndex & 7)); // remove
    if (value) buffer[bitIndex >> 3] |= (1 << (bitIndex & 7)); // set if value is true
    return value; // return value
}
void ScanMemoryReset(){
    // all chunks possible by default
    memset(Scan.memoryScanFullChunk, 0xFF, 4096);
    memset(Scan.memoryScanMinChunk1, 0xFF, 4096);
    memset(Scan.memoryScanMinChunk3, 0xFF, 4096);
    memset(Scan.memoryScanAddresses, 0, sizeof(Scan.memoryScanAddresses));
    Scan.memoryScanPass = 0;
}
u64 ScanMemory(u32 start, u32 end, u32 debugHandle, u8 check, u32 *testResult, u32 level){
    if (level == 0){
        ScanMemoryReset();
        return 0;
    }
    if (level > 3) return 0; // to high level
    if ((end - start) >= 0x8000000) return 0; // not sure what error to put
    u64 startTime = svcGetSystemTick();
    u8 *possibleChunks_1 = Scan.memoryScanFullChunk;
    u32 chunkBitIndex_1 = 0;

    u8 *possibleChunks_2 = Scan.memoryScanMinChunk1;
    u32 chunkBitIndex_2 = 0;

    u8 *possibleChunks_3 = Scan.memoryScanMinChunk3;
    u32 chunkBitIndex_3 = 0;
    
    memset(Scan.memoryScanAddresses, 0, sizeof(Scan.memoryScanAddresses));
    Scan.memoryScanAddressIndex = 0;

    u32 addr = PAGE_ROUND(start); // round down
    u32 possibleChunkCount = 0;
    for (; addr < end; addr += PAGE_SIZE) {
        if (R_FAILED(svcReadProcessMemory(Scan.memoryScanBuffer, debugHandle, addr, PAGE_SIZE))) continue;  // if read failed, continue to next iteration
        bool isValid = getBit(possibleChunks_1, chunkBitIndex_1);
        if (isValid){ // check if chunk is valid (all chunks valid by default so everything is set on the first pass)
            if (level == 1){ // level 1
                setBit(possibleChunks_1, chunkBitIndex_1, false); // remove bit because if it ends up being valid it will be set regardless (we already know the chunk was valid)
                for (u32 j = 0; j < CHUNK_SIZE_1; j++) {
                    u8 b = Scan.memoryScanBuffer[j++];
                    if (b != check) continue;
                    setBit(possibleChunks_1, chunkBitIndex_1, true); // set chunk bit (this will only occur if both the previous chunk check was valid and this one is too)
                    possibleChunkCount++; // not sure if this will end up being used
                    if (Scan.memoryScanAddressIndex < MAX_SCAN_ADDR - 1) Scan.memoryScanAddresses[Scan.memoryScanAddressIndex++] = PAGE_ROUND(start) + (chunkBitIndex_1 * CHUNK_SIZE_1);
                    break; // break because we already know this chunk has the value
                }
            } else { // level > 1
                for (u32 k = 0; k < 16; k++) { // chunk decrees multiplier
                    isValid = getBit(possibleChunks_2, chunkBitIndex_2); // check if "new" chunk is valid (all chunks valid by default so everything is set on the first pass)
                    if (isValid){
                        if (level == 2){ // level 2
                            setBit(possibleChunks_2, chunkBitIndex_2, false); // remove bits
                            for (u32 j = 0; j < CHUNK_SIZE_2; j++) {
                                u8 b = Scan.memoryScanBuffer[(k * CHUNK_SIZE_2) + j];
                                if (b != check) continue;
                                setBit(possibleChunks_2, chunkBitIndex_2, true);
                                possibleChunkCount++; // not sure if this will end up being used
                                if (Scan.memoryScanAddressIndex < MAX_SCAN_ADDR - 1) Scan.memoryScanAddresses[Scan.memoryScanAddressIndex++] = PAGE_ROUND(start) + (chunkBitIndex_1 * CHUNK_SIZE_1) + (k * CHUNK_SIZE_2);
                                break; // break because we already know this chunk has the value
                            }
                        } else {
                            for (u32 k2 = 0; k2 < 16; k2++) { // chunk decrees multiplier
                                isValid = getBit(possibleChunks_3, chunkBitIndex_3); // check if "new" chunk is valid (all chunks valid by default so everything is set on the first pass)
                                if (isValid){
                                    if (level == 3){ // level 3
                                        setBit(possibleChunks_3, chunkBitIndex_3, false); // remove bits
                                        for (u32 j = 0; j < CHUNK_SIZE_3; j++) {
                                            u8 b = Scan.memoryScanBuffer[(k * CHUNK_SIZE_2) + (k2 * CHUNK_SIZE_3) + j];
                                            if (b != check) continue;
                                            setBit(possibleChunks_3, chunkBitIndex_3, true);
                                            possibleChunkCount++; // not sure if this will end up being used
                                            if (Scan.memoryScanAddressIndex < MAX_SCAN_ADDR - 1) Scan.memoryScanAddresses[Scan.memoryScanAddressIndex++] = PAGE_ROUND(start) + (chunkBitIndex_1 * CHUNK_SIZE_1) + (k * CHUNK_SIZE_2) + (k2 * CHUNK_SIZE_3);
                                            break; // break because we already know this chunk has the value
                                        }
                                    } else {
                                        // do nothing for levels more then 3
                                    } // end of level select 3
                                } // end of chunk code 3
                                chunkBitIndex_3++;
                            } // second nested loop
                        } // end of level select 2
                    } // end of chunk code 2
                    chunkBitIndex_2++;
                } // first nested loop
            } // end of level select 1
        } // end of chunk code 1
        chunkBitIndex_1++;
    }
    Scan.memoryScanPass++;
    if (Scan.memoryScanPass >= MAX_SCAN_RESULTS){
        Scan.memoryScanPass = MAX_SCAN_RESULTS - 1;
    }
    *testResult = possibleChunkCount;
    return svcGetSystemTick() - startTime; // return here if first pass because we dont have enough information for the next check (technically we could so maybe this should be removed)
}