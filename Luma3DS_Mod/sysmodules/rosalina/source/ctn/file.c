
#include <3ds.h>
#include <3ds/services/fs.h> // file stuff

#include <stdio.h> // for sprintf
#include <string.h> // for memset

#include "ctn/file.h"
#include "ctn/newUI.h"
#include "ctn/ctn.h"

// from base_menu.c
extern void DrawDefaultUpperScreenStuff();
extern void DefaultWindowStart();
extern void DefaultWindowEnd();

bool nextDir(char *dir, char *add, u32 maxLength){
    u32 len = strlen(dir);
    if (len + strlen(add) + 1 >= maxLength) return false;
    if (strlen(add) == 0) return true;
    if (dir[len - 1] != '/') dir[len] = '/'; // only add / if it doesn't end with one already
    else len--;
    memcpy(&dir[len + 1], add, strlen(add));
    dir[len + strlen(add) + 1] = 0;
    return true;
}
void downDir(char *dir){
    for (int i = strlen(dir) - 1; i >= 0; i--) {
        if (dir[i] == '/') {
            dir[i] = 0;
            break;
        }
    }
    if (strlen(dir) == 0) { dir[0] = '/'; dir[1] = 0; } // unsafe, this assume dir is at elast 2 elements long
}

#define MAX_DIRECTORY_ENTRIES 16
static FS_DirectoryEntry directoryEntries[MAX_DIRECTORY_ENTRIES];

static FS_Archive cachedArchive = 0;
static bool isArchiveCached = false;

void setCacheArchive(FS_Archive archive){ // uses cached handle
    if (isArchiveCached) return;
    cachedArchive = archive;
    isArchiveCached = true;
}
void closeCachedArchive(){ // uses cached handle
    FSUSER_CloseArchive(cachedArchive);
    isArchiveCached = false;
}

bool openArchive(FS_Archive *archive){ // uses cached handle
    Result openRes = FSUSER_OpenArchive(archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(openRes)) return false;
    return true;
}
bool closeArchive(FS_Archive archive){ // uses cached handle
    Result closeRes = FSUSER_CloseArchive(archive);
    if (R_FAILED(closeRes)) return false;
    if (isArchiveCached && cachedArchive == archive) isArchiveCached = false;
    return true;
}

bool openDirectory(FS_Archive archive, Handle *dirHandle, char *path){
    Result openDirRes = FSUSER_OpenDirectory(dirHandle, archive, fsMakePath(PATH_ASCII, path));
    if (R_FAILED(openDirRes)) return false;
    return true;
}
bool closeDirectory(Handle dirHandle){
    if (dirHandle == 0) return true;
    if (R_FAILED(FSDIR_Close(dirHandle))) return false;
    return true;
}

static char tempPath[512];
bool getTempFile(char *file, char *tmpFile){ // uses cached handle
    if (strlen(file) >= 512 - 6){ // minus "tmpXY\0"
        return false; // name too long
    }
    // check if not cached then open it so calling fileExists() isn't too expensive
    FS_Archive archive = 0;
    bool openedArchive = false;
    if (!isArchiveCached){
        openArchive(&archive);
        setCacheArchive(archive);
        openedArchive = true;
    }
    u32 tmpNum = 0;
    while (true) {
        sprintf(tempPath, "%stmp%02lX", file, tmpNum);
        bool fileExist = fileExists(tempPath);
        if (!fileExist){
            memcpy(tmpFile, tempPath, 512);
            if (openedArchive) closeCachedArchive();
            return true;
        }
        tmpNum++;
        if (tmpNum > 0xFF){
            if (openedArchive) closeCachedArchive();
            return false; // temp num too high, technically not an issue but why do you have so many temp files
        }
    }
}
bool fileExists(char *file){ // uses cached handle 
    FS_Archive archive;
    if (isArchiveCached) archive = cachedArchive;
    if (isArchiveCached || openArchive(&archive)){
        Handle fileHandle = 0;
        bool res = openFile(archive, &fileHandle, file);
        closeFile(fileHandle);
        if (!isArchiveCached) closeArchive(archive);
        return res;
    }
    return false;
}
bool directoryExists(char *path){ // uses cached handle 
    FS_Archive archive;
    if (isArchiveCached) archive = cachedArchive;
    if (isArchiveCached || openArchive(&archive)){
        Handle baseDirHandle = 0;
        bool opened = openDirectory(archive, &baseDirHandle, path);
        closeDirectory(baseDirHandle);
        if (!isArchiveCached) closeArchive(archive);
        return opened;
    }
    return false;
}
bool enumerateFiles(char *path, DirectoryData *entries, u32 count, u32 *read, u32 skip){  // uses cached handle
    if (read != NULL) (*read) = 0;
    bool pass = false;
    FS_Archive archive = 0;
    if (isArchiveCached) archive = cachedArchive;
    if (isArchiveCached || openArchive(&archive)){
        Handle baseDirHandle = 0;
        if (openDirectory(archive, &baseDirHandle, path)){
            memset(directoryEntries, 0, sizeof(directoryEntries)); // clear entires
            u32 totalRead = 0;
            u32 readCount = 0;
            u32 loopCount = 64;
            pass = true;
            while(loopCount--){ // max 64 incase somthing fails and consinies without is wanting it
                Result readRes = FSDIR_Read(baseDirHandle, &readCount, MAX_DIRECTORY_ENTRIES, directoryEntries);
                if (R_FAILED(readRes)){
                    pass = false;
                    continue;
                }
                for (u32 i = totalRead, j = 0; i < (totalRead + readCount); i++, j++) {
                    if (i < skip) continue;
                    if (read != NULL) (*read)++;
                    if (i >= count) break;
                    u32 i2 = i - skip;
                    entries[i2].attributes = directoryEntries[j].attributes;
                    entries[i2].fileSize = directoryEntries[j].fileSize;
                    entries[i2].reserved = directoryEntries[j].reserved;
                    memcpy(entries[i2].shortExt, directoryEntries[j].shortExt, 4);
                    memcpy(entries[i2].shortName, directoryEntries[j].shortName, 10);
                    entries[i2].valid = directoryEntries[j].valid;
                    memset(entries[i2].name, 0, 262);
                    utf16_to_utf8((uint8_t*)entries[i2].name, (uint16_t*)directoryEntries[j].name, 262);
                }
                totalRead += readCount;
                if (totalRead >= count || readCount < MAX_DIRECTORY_ENTRIES){
                    pass = true;
                    break;
                }
            }
            closeDirectory(baseDirHandle);
        } else pass = false;
        if (!isArchiveCached) closeArchive(archive);
    } else pass = false;
    return pass;
}
bool directoryCreate(char *path){ // uses cached handle
    FS_Archive archive;
    if (isArchiveCached){
        archive = cachedArchive;
    } else {
        if (!openArchive(&archive)) return false;
    }
    Result res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, path), 0);
    if (!isArchiveCached) closeArchive(archive);
    if (res == (Result)0xC82044BE) return true; // apparity result 0xC82044BE means it already exist according to rosalina
    if (R_FAILED(res)) return false;
    return true;
}
bool ensureDirectory(char *path){ // uses cached handle
    if (!directoryExists(path))
        if (!directoryCreate(path))
            return false;
    return true;
}

bool openFile(FS_Archive archive, Handle *fileHandle, char *path){
    Result res;
    res = FSUSER_OpenFile(fileHandle, archive, fsMakePath(PATH_ASCII, path), FS_OPEN_WRITE, 0);
    if (R_FAILED(res)) return false;
    return true;
}
bool openCreateFile(FS_Archive archive, Handle *fileHandle, char *path){
    Result res;
    res = FSUSER_OpenFile(fileHandle, archive, fsMakePath(PATH_ASCII, path), FS_OPEN_CREATE | FS_OPEN_WRITE, 0);
    if (R_FAILED(res)) return false;
    return true;
}
bool writeFile(Handle fileHandle, u64 offset, void* buffer, u32 size, u32 *writtenBytes){
    u32 totalWritten = 0;
    u32 written = 0;
    u32 left = size;
    u64 pos = offset;
    void *buf = buffer;
    bool failed = false;
    while (true) {
        Result res = FSFILE_Write(fileHandle, &written, pos, buf, left, 0);
        if (R_FAILED(res)) { failed = true; break; }
        totalWritten += written;
        pos += written;
        buf += written;
        if (left < written) break; // error: wrote more bytes then we should have
        left -= written;
        if (left == 0) break;
    }
    (*writtenBytes) = totalWritten;
    return failed ? false : true;
}
bool readFile(Handle fileHandle, u64 offset, void* buffer, u32 size, u32 *readBytes){
    u32 totalRead = 0;
    u32 written = 0;
    u32 left = size;
    u64 pos = offset;
    void *buf = buffer;
    bool failed = false;
    while (true) {
        Result res = FSFILE_Read(fileHandle, &written, pos, buf, left);
        if (R_FAILED(res)) { failed = true; break; }
        totalRead += written;
        pos += written;
        buf += written;
        if (left < written) break; // error: read more bytes then we should have
        left -= written;
        if (left == 0) break;
    }
    (*readBytes) = totalRead;
    return failed ? false : true;
}
bool closeFile(Handle fileHandle){
    if (fileHandle == 0) return true;
    if (R_FAILED(FSFILE_Close(fileHandle))) return false;
    return true;
}
bool getFileSize(Handle fileHandle, u64 *size){
    if (R_FAILED(FSFILE_GetSize(fileHandle, size))) return false;
    return true;
}
bool moveFile(FS_Archive archive, char *oldPath_or_tempFile, char *newPath, bool override){ // WARNING: deletes old file and temporary file, if override is false it wont remove the original file if it exists, make sure files handles are closed
    if (override){
        FSUSER_DeleteFile(archive, fsMakePath(PATH_ASCII, newPath)); // could fail if file does not exist
    }
    if (R_FAILED(FSUSER_RenameFile(archive, fsMakePath(PATH_ASCII, oldPath_or_tempFile), archive, fsMakePath(PATH_ASCII, newPath)))) return false;
    FSUSER_DeleteFile(archive, fsMakePath(PATH_ASCII, oldPath_or_tempFile)); // remove old file
    return true;
}

MenuResult ctn_Menu_FileMenu() {
    
    #define MAX_ENTRY 32

    static Button backButton;
    static ScrollZone fileScroll;

    static Button backDirButton;
    
    static Button entryButtons[MAX_ENTRY];

    MENU_ONCE_START{
        // renamed to exit to make it less confusing
        backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Exit", BASE_COLOUR, 0x000000);
        fileScroll = UI_CreateScrollZone(1 * PADDING, PADDING + 20 + PADDING + SPACING_Y + PADDING, BOTTOM_SCREEN_WIDTH - (2 * PADDING), BOTTOM_SCREEN_HEIGHT - (2 * 20) - (4 * PADDING) - SPACING_Y - PADDING);
        
        backDirButton = UI_CreateButton(BOTTOM_SCREEN_WIDTH - PADDING - 40, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 40, 20, "..", BASE_COLOUR, 0x000000);

        for (int i = 0; i < MAX_ENTRY; i++) { entryButtons[i] = UI_CreateButton(0, 0, 4 * SPACING_X + 6, SPACING_Y, "Open", BASE_COLOUR, 0x000000); entryButtons[i].textOffsetX = -2; }

    } MENU_ONCE_END;

    FS_Archive archive;
    bool archiveOpened = openArchive(&archive);
    setCacheArchive(archive);

    u32 entriesRead;
    static DirectoryData entries[MAX_ENTRY];

    static char currentDir[512];
    memset(currentDir, 0, sizeof(currentDir));
    currentDir[0] = '/';
    currentDir[1] = 0;
    u32 dirLevel = 0;
    bool dirtyDir = true;

    bool readSuccess = true;

    s32 selectedIndex = -1;

    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("File Menu");
        DefaultWindowStart();
        if (currentDir[1] == 0){ // this means we are at the bottom level
            MENU_BCLOSE;
        }

        if (dirtyDir){
            selectedIndex = -1;
            fileScroll.scrollX = 0;
            fileScroll.scrollY = 0;
            if (archiveOpened){
                memset(entries, 0, sizeof(entries));
                readSuccess = enumerateFiles(currentDir, entries, MAX_ENTRY, &entriesRead, 0);
            }
 
            dirtyDir = false;
        }

        u32 pressed = UI_GetPressedButtons();
        if (pressed & KEY_DDOWN){
            if (selectedIndex != -1){
                selectedIndex++;
                if (selectedIndex >= (s32)entriesRead) selectedIndex = (s32)entriesRead - 1;
            } else selectedIndex = 0;
        }
        if (pressed & KEY_DUP){
            if (selectedIndex != -1){
                selectedIndex--;
                if (selectedIndex < 0) selectedIndex = 0;
            } else selectedIndex = 0;
        }
        if (UI_IsCursorMode()) selectedIndex = -1;

        if (fileScroll.scrollY < 0){
            fileScroll.scrollY = 0;
        } else {
            s32 maxScroll = 8 + ((s32)entriesRead * SPACING_Y);
            if ((u32)maxScroll > fileScroll.h){
                if (fileScroll.scrollY > maxScroll - fileScroll.h){
                    fileScroll.scrollY = maxScroll - fileScroll.h;
                }
            } else {
                fileScroll.scrollY = 0;
            }
            if (selectedIndex != -1 && (pressed & KEY_DDOWN || pressed & KEY_DUP)){
                s32 pos = 8 + (selectedIndex * SPACING_Y);
                if (pos + SPACING_Y - fileScroll.scrollY > fileScroll.h){
                    fileScroll.scrollY = pos + SPACING_Y - fileScroll.h;
                } 
                if (pos - fileScroll.scrollY < 8){
                    fileScroll.scrollY = pos - SPACING_Y;
                }
            }
        }

        if (fileScroll.scrollX < 0){
            fileScroll.scrollX = 0;
        } else {
            u32 length = 0;
            for (u32 i = 0; i < entriesRead; i++) {
                u32 len = strlen(entries[i].name);
                if (len > length) length = len;
            }
            s32 maxScroll = 8 + ((s32)(length + 13) * SPACING_X); // +13 for size info
            if ((u32)maxScroll > fileScroll.w){
                if (fileScroll.scrollX > maxScroll - fileScroll.w){
                    fileScroll.scrollX = maxScroll - fileScroll.w;
                }
            } else {
                fileScroll.scrollX = 0;
            }
        }
                
        UI_DrawPanel(fileScroll.x, fileScroll.y, fileScroll.w, fileScroll.h, true);
        
        if (!archiveOpened){
            UI_DrawStringFormat(PADDING * 2, PADDING + 20 + PADDING, 0xFF0000, "Open Archive Fail");
        } else if (!readSuccess){
            UI_DrawStringFormat(PADDING * 2, PADDING + 20 + PADDING, 0xFF0000, "Read Fail");
            UI_DrawStringFormat(PADDING * 2, PADDING + 20 + PADDING + SPACING_Y, 0xFF0000, "> %s", currentDir);
        } else {
            UI_DrawStringFormat(PADDING * 2, PADDING + 20 + PADDING, 0x000000, "> %s", currentDir);

            s32 stringX = fileScroll.x + PADDING - fileScroll.scrollX;
            s32 stringY = fileScroll.y + PADDING - fileScroll.scrollY;
            
            UI_SetClippingPlaneZ(fileScroll);

            for (u32 i = 0; i < MAX_ENTRY; i++) {
                if (entries[i].attributes == 0 && entries[i].name[0] == 0) continue; // if no atrributes and no name its likely not a file
                bool isDir = entries[i].attributes & FS_ATTRIBUTE_DIRECTORY;
                bool isSel = selectedIndex == (s32)i;
                u32 colour = isSel ? 0x007F00 : (isDir ? 0x00007F : 0x000000); // mid-green, mid-blue, black
                entryButtons[i].x = stringX;
                entryButtons[i].y = stringY;
                if (isDir) MENU_DO_BUTTON_OR(entryButtons[i], (isSel && pressed & KEY_A)){
                    if (!nextDir(currentDir, entries[i].name, 512)){
                        UI_DisplayMessage("Directory too long");
                    } else {
                        dirtyDir  = true;
                        break;
                    };
                }
                UI_DrawStringFormatSized(0, isDir ? (stringX + entryButtons[i].w + 2) : stringX, stringY, colour, "%s (0x%08X)", entries[i].name, entries[i].fileSize);
                stringY += SPACING_Y;
            }

            UI_UpdateScrollZone(&fileScroll);
    
            UI_ClearClippingPlane();
            
            MENU_DO_BUTTON_OR(backDirButton, (pressed & KEY_B)){
                dirLevel--;
                downDir(currentDir);
                dirtyDir  = true;
            }
    
        }

        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        
        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
    } MENU_LOOP_END();

    closeArchive(archive); // not needed but just in case
    closeCachedArchive(archive);

    return MENU_RESULT_OK;

}

