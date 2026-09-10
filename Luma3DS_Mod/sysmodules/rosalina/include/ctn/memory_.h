// needs to have underscore otherwise he original memory.c file gets confused
#pragma once

#include <3ds.h>
#include "ctn/process.h"

// i think this is the correct size, its worked so far and is the size of all of the sections ive found
#define PAGE_SIZE 0x1000
// rounds an address down to the page address, 0x123456 would become 0x123000
#define PAGE_ROUND(addr) (addr >> 12 << 12)
// gets the page remainder of the address, 0x123456 would become 0x000456
#define PAGE_REMAIN(addr) (addr & 0xFFF)

#define TEXT_SECTION_ADDR 0x10005
#define RODATA_SECTION_ADDR 0x10006
#define DATA_SECTION_ADDR 0x10007

#define TEXT_SECTION_SIZE 0x10002
#define RODATA_SECTION_SIZE 0x10003
#define DATA_SECTION_SIZE 0x10004

typedef struct SearchTerm{
    bool gap;
    bool wildcard;
    u8 searchByte;
} SearchTerm;

#define MAX_SCAN_RESULTS 32
// used to store chunk data
// each bit represents a chunk that could contain a search value, 
// anything larger then 0x8000000 will not work, this is a ridiculous size but terraria's heap (version 1024) is close at 0x6000000 bytes which requires 24576 chunks
#define MAX_SCAN_ADDR 1024
typedef struct ScanData {
    u8 memoryScanFullChunk[4096]; // 32768 total chunk room 
    u8 memoryScanMinChunk1[4096]; // 32768 total chunk room (2048 normal size chunks)
    u8 memoryScanMinChunk3[4096]; // 32768 total chunk room (128 normal size chunks)
    u32 memoryScanAddresses[MAX_SCAN_ADDR]; // 1024 addresses
    u32 memoryScanAddressIndex;
    u32 memoryScanPass;
    u8 memoryScanBuffer[PAGE_SIZE];
    u32 scanResults[MAX_SCAN_RESULTS];
} ScanData;
#define CHUNK_SIZE_1 PAGE_SIZE // 0x1000 (4096)
#define CHUNK_SIZE_2 (PAGE_SIZE / 16) // 0x100 (256)
#define CHUNK_SIZE_3 ((PAGE_SIZE / 16) / 16) // 0x10 (16)

extern u8 dataBuffer[PAGE_SIZE];
extern ScanData Scan;

// not to be confused with "MemRegion"
typedef struct memRegion{
    u32 address;
    u32 size;
    // used to determine if the memory can be overriden, dont change this, let mem_free() do it
    bool used;
} memRegion;

extern u8 breakpointValueBuffer[4];

Result mem_alloc(memRegion *mem, u32 size, bool executable);
Result mem_free(memRegion *mem);

bool ReadMemory(Process *proc, u32 address, u8 *buffer, u32 bufferSize);
bool WriteMemory(Process *proc, u32 address, u8 *buffer, u32 bufferSize);

u32 SearchToNum(SearchTerm search);
bool HexStringToNumLimit(char *string, u32 length, u32 *value);
bool HexStringToNum(char *string, u32 *value);
void CreatSearchTermList(char *searchString, SearchTerm *search, u32 maxSearchTerms, u32 *resultSearchTermCount);
u64 SearchMemory(u32 start, u32 end, u32 debugHandle, SearchTerm *search, u32 searchLength, u32 *resultBuffer, u32 maxResults, u32 *foundResultCount, u32 *resultLengthBuffer, u64 maxSearchTime);
void ScanMemoryReset();
u64 ScanMemory(u32 start, u32 end, u32 debugHandle, u8 check, u32 *testResult, u32 level);