#pragma once

#include <3ds.h>

// see FS_DirectoryEntry
typedef struct {
	char name[262]; // 256 + 6
	char shortName[10];
	char shortExt[4];
	u8 valid;
	u8 reserved;
	u32 attributes;
	u64 fileSize;
} DirectoryData;

void setCacheArchive(FS_Archive archive);
void closeCachedArchive();

bool openArchive(FS_Archive *archive);
bool closeArchive(FS_Archive archive);

bool openDirectory(FS_Archive archive, Handle *dirHandle, char *path);
bool closeDirectory(Handle dirHandle);

bool getTempFile(char *file, char *tmpFile);
bool fileExists(char *file);
bool directoryExists(char *path);
bool enumerateFiles(char *path, DirectoryData *entries, u32 count, u32 *read, u32 skip);
bool directoryCreate(char *path);
bool ensureDirectory(char *path);

bool openFile(FS_Archive archive, Handle *fileHandle, char *path);
bool openCreateFile(FS_Archive archive, Handle *fileHandle, char *path);
bool writeFile(Handle fileHandle, u64 offset, void* buffer, u32 size, u32 *writtenBytes);
bool readFile(Handle fileHandle, u64 offset, void* buffer, u32 size, u32 *readBytes);
bool closeFile(Handle fileHandle);
bool getFileSize(Handle fileHandle, u64 *size);
bool moveFile(FS_Archive archive, char *oldPath, char *newPath, bool override);