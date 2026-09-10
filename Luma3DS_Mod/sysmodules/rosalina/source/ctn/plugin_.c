#include <3ds.h>
#include <ctn/newUI.h>
#include <fmt.h> // from sprintf
#include "ctn/process.h"
#include "ctn/memory_.h"
#include "ctn/ctn.h"
#include "ctn/file.h"
#include "ctn/debug_.h"

#include "ctn/plugin_.h"

// from base_menu.c
extern void DrawDefaultUpperScreenStuff();
extern void DefaultWindowStart();
extern void DefaultWindowEnd();

//https://en.wikipedia.org/wiki/MurmurHash
// chosen because its simple and Terraria uses it.
u32 murmurHash3(char* str, u32 len, u32 seed){
    #define ROL(a, b) ((u32)((a) << (b)) | (u32)((((u64)(a)) << (b)) >> 32))
    #define C1 0xCC9E2D51
    #define C2 0x1B873593
    #define R1 15
    #define R2 13
    #define M 5
    #define N 0xE6546B64

    u32 hash = seed;
    s32 i = 0;
    while(1){
        if ((i + 4) >= (s32)len) break;
        u32 k = (u32)str[i] | ((u32)str[i + 1] << 8) | ((u32)str[i + 2] << 16) | ((u32)str[i + 3] << 24);
        hash = ROL(hash ^ (ROL(k * C1, R1) * C2), R2) * M + N;
        i += 4;    
    }
    for (u32 j = i; j < len; j++){
        u32 k = (u32)str[j];
        hash ^= ROL(k * C1, R1) * C2;
    }

    hash ^= len;
    hash = (hash ^ (hash >> 16)) * 0x85EBCA6B;
    hash = (hash ^ (hash >> 13)) * 0xC2B2AE35;
    hash = (hash ^ (hash >> 16));

    return hash;

    #undef N
    #undef M
    #undef R2
    #undef R1
    #undef C2
    #undef C1
    #undef ROL
}

///
/// front end plugin functions
///
#pragma region pluginFuncs

    u32 API_VERSION() { return 1; } // change this when api changes

    u32 DrawString(s32 posX, s32 posY, u32 textColour, char* string) { return UI_DrawString(posX, posY, textColour, string); }
    u32 DrawStringFormatSized(u32 bufferSize, s32 posX, s32 posY, u32 textColour, char* formatString, ...) {
        va_list arg; 
        va_start(arg, formatString);
        u32 ret = UI_DrawStringFormatSized_Args(bufferSize, posX, posY, textColour, formatString, arg);
        va_end(arg);
        return ret;
    }
    u32 DrawStringFormat(s32 posX, s32 posY, u32 textColour, char* formatString, ...){
        va_list arg; 
        va_start(arg, formatString);
        u32 ret = UI_DrawStringFormat_Args(posX, posY, textColour, formatString, arg);
        va_end(arg);
        return ret;
    }

    void DrawButton(Button *button) { UI_DrawButton(button); }
    bool UpdateButton(Button *button) { return UI_UpdateButton(button); }
    bool DoButton(Button *button) { return UI_DoButton(button); }

    bool UpdateScrollZone(ScrollZone *scroll) { return UI_UpdateScrollZone(scroll); }

    void DrawCheckBox(CheckBox *checkBox) { UI_DrawCheckBox(checkBox); }
    bool UpdateCheckBox(CheckBox *checkBox) { return UI_UpdateCheckBox(checkBox); }
    bool DoCheckBox(CheckBox *checkBox) { return UI_DoCheckBox(checkBox); }

    void DrawWindow(Window *window) { UI_DrawWindow(window); }
    bool UpdateWindow(Window *window) { return UI_UpdateWindow(window); }

    void DrawTabPanel(TabPanel *tabPanel) { UI_DrawTabPanel(tabPanel); }
    bool UpdateTabPanel(TabPanel *tabPanel) { return UI_UpdateTabPanel(tabPanel); }
    bool DoTabPanel(TabPanel *tabPanel) { return UI_DoTabPanel(tabPanel); }

    bool DrawPixel(s32 x, s32 y, u32 colour) { return UI_DrawPixel(x, y, colour); }
    void DrawButtonOutline(s32 x, s32 y, u32 w, u32 h, bool down) { UI_DrawButtonOutline(x, y, w, h, down); }
    void DrawPanel(s32 x, s32 y, u32 w, u32 h, bool down) { UI_DrawPanel(x, y, w, h, down); }
    void DrawRect(s32 x, s32 y, u32 w, u32 h, u32 colour) { UI_DrawRect(x, y, w, h, colour); }
    void FillRect(s32 x, s32 y, u32 w, u32 h, u32 colour) { UI_FillRect(x, y, w, h, colour); }
    void DrawCheckBoxPanel(s32 x, s32 y, u32 size) { UI_DrawCheckBoxPanel(x, y, size); }

    bool InClippingPlaneX(s32 x) { return UI_InClippingPlaneX(x); }
    bool InClippingPlaneY(s32 y) { return UI_InClippingPlaneY(y); }
    bool InClippingPlane(s32 x, s32 y) { return UI_InClippingPlane(x, y); }
    void ClearClippingPlane() { UI_ClearClippingPlane(); }
    void SetClippingPlane(s32 x, s32 y, u32 w, u32 h) { UI_SetClippingPlane(x, y, w, h); }
    void SetClippingPlaneZ(ScrollZone z) { UI_SetClippingPlaneZ(z); }

    bool        IsCursorMode() { return UI_IsCursorMode(); }
    u32         GetPressedButtons() { return UI_GetPressedButtons(); }
    u32         GetHeldButtons() { return UI_GetHeldButtons(); }
    UTouchPoint GetTouchPos() { return UI_GetTouchPos(); }
    STouchPoint GetCursorPos() { return UI_GetCursorPos(); }
    STouchPoint GetTouchDelta() { return UI_GetTouchDelta(); }
    STouchPoint GetCursorDelta() { return UI_GetCursorDelta(); }

    Button      CreateButton(s32 x, s32 y, u32 w, u32 h, char *string, u32 colour, u32 textColour) { return UI_CreateButton(x, y, w, h, string, colour, textColour); }
    ScrollZone  CreateScrollZone(s32 x, s32 y, u32 w, u32 h){ return UI_CreateScrollZone(x, y, w, h); }
    CheckBox    CreateCheckBox(s32 x, s32 y, u32 size, char *checkChar, u32 testColour, u32 colour, bool checked) { return UI_CreateCheckBox(x, y, size, checkChar, testColour, colour, checked); }
    Window      pCreateWindow(s32 x, s32 y, u32 w, u32 h, char* title) { return UI_CreateWindow(x, y, w, h, title); }
    TabPanel    CreateTabPanel(s32 x, s32 y, u32 w, u32 h, u32 btnW, u32 btnH, char **titles, u32 tabCount) { return UI_CreateTabPanel(x, y, w, h, btnW, btnH, titles, tabCount); }

    bool DisplayMessageFormat(char* str, ...){
        va_list arg; 
        va_start(arg, str);
        bool ret = UI_DisplayMessageFormat_Args(str, arg);
        va_end(arg);
        return ret;
    }
    bool DisplayMessage(char* str) { return UI_DisplayMessage(str); }
    bool GetUserInput(char* message, u32 maxInputLength, char *ouputBuffer) { return UI_GetUserInput(message, maxInputLength, ouputBuffer); }

    #pragma region process

        extern bool Proc_IsProcessAttached();
        extern Process *Proc_GetProcess();
        extern bool Proc_IsProcessDebugger();
        extern bool OpenProcess_Name(const char* name);
        extern bool OpenProcess_TitleId(u64 titleId);
        extern bool ReadMemory(Process *proc, u32 address, u8 *buffer, u32 bufferSize);
        extern bool WriteMemory(Process *proc, u32 address, u8 *buffer, u32 bufferSize);
        extern bool AllocateCodeCave(Process *proc, u32 size, u32 *codeCave);

        bool AttachedProcessTitle(u64 titleId) { return OpenProcess_TitleId(titleId); }
        bool AttachedProcessName(const char* name) { return OpenProcess_Name(name); }
        bool DetachProcess() { Proc_GetProcess()->locked = false; return CloseProcess(Proc_GetProcess()); }
        bool IsProcessAttached() { return Proc_IsProcessAttached(); }
        ProcessData GetProcess() { 
            Process *_p = Proc_GetProcess();
            ProcessData p = {
                .pid = _p->pid,
                .titleId = _p->titleId
            };
            memcpy(&(p.name), &(_p->name), 9);
            return p;
        }
        TitleInfo GetTitleInfo(u64 title){
            TitleInfo data;
            data.loc = NONE;
            data.size = 0;
            data.titleID = 0;
            memset(data.unk, 0, sizeof(data.unk));
            data.version = 0;

            AM_TitleEntry entry;
            u32 loc = 0;
            if (GetAMTitleInfo(title, &entry, &loc)){
                data.loc = loc == 0 ? SD : (loc == 1 ? CARD : NAND);
                data.size = entry.size;
                data.titleID = entry.titleID;
                memcpy(data.unk, entry.unk, sizeof(data.unk));
                data.version = entry.version;
            }
            return data;
        }
        TitleInfo GetTitleUpdateInfo1(u64 defaultTitle){
            return GetTitleInfo((defaultTitle & 0x00000000FFFFFFFF) | 0x0004000E00000000);
        }
        TitleInfo GetTitleUpdateInfo2(u64 updateTitle){
            return GetTitleInfo(updateTitle);
        }

        #pragma region debug
            bool AttachDebugger(){
                if (!Proc_IsProcessAttached() || Proc_IsProcessDebugger()) return false;
                return StartProcessDebugThread(Proc_GetProcess());
            }
            bool DetachDebugger(){
                if (!Proc_IsProcessAttached() || !Proc_IsProcessDebugger()) return false;
                return StopProcessDebugThread(0);
            }
            bool IsDebugging() { return Proc_IsProcessDebugger(); }
            bool pAllocateCodeCave(u32 *address) { return AllocateCodeCave(Proc_GetProcess(), PAGE_SIZE, address); }
            bool ReadProcessMemory(u32 address, u32 size, void *buffer) { return ReadMemory(Proc_GetProcess(), address, (u8*)buffer, size); }
            bool WriteProcessMemory(u32 address, u32 size, void *buffer) { return WriteMemory(Proc_GetProcess(), address, (u8*)buffer, size); }

        #pragma endregion debug
    #pragma endregion process
#pragma endregion pluginFuncs

// regex search and replace
// ^.*?([a-zA-Z_]+[a-zA-Z_0-9]*) +([a-zA-Z_]+[a-zA-Z_0-9]*)\((.*?)\)
// $1 (*$2) ($3);\n

#define PLUGIN_END_DATA_OFFSET (24 * 4)
#define MAX_PLUGINS_FILE_SIZE (PAGE_SIZE * 8) - PLUGIN_END_DATA_OFFSET // 8 pages (arbitrary limit) (-96 for trampoline or other stuff)
#define MAX_PLUGINS 16
typedef struct Plugin{
    u32 textSize;
    u32 dataSize;
    u32 bssSize;
    u32 rodataSize;
    u32 rolocCount;
    // address is used to check if its an active plugin
    u32 address; 
    u32 stateAddress;
    memRegion allocation;
    u32 updateCounter;
    // murmur hash used for identifications from the plugin name
    u32 hash;
} Plugin;
static Plugin plugins[MAX_PLUGINS] = { 0 };
static Plugin *curPlugin;
static u8 curPluginFile[MAX_PLUGINS_FILE_SIZE] = { 0 };
static char curPluginPath[512];

static PluginAPI API = { 0 }; // make sure to zero this or else it could call trash functions
static bool pluginInit;

static void initPlugins(){

    API.API_VERSION = API_VERSION;
    
    API.UI.DrawString = DrawString;
    API.UI.DrawStringFormatSized = DrawStringFormatSized;
    API.UI.DrawStringFormat = DrawStringFormat;
    API.UI.DrawButton = DrawButton;
    API.UI.UpdateButton = UpdateButton;
    API.UI.DoButton = DoButton;
    API.UI.UpdateScrollZone = UpdateScrollZone;
    API.UI.DrawCheckBox = DrawCheckBox;
    API.UI.UpdateCheckBox = UpdateCheckBox;
    API.UI.DoCheckBox = DoCheckBox;
    API.UI.DrawWindow = DrawWindow;
    API.UI.UpdateWindow = UpdateWindow;
    API.UI.DrawTabPanel = DrawTabPanel;
    API.UI.UpdateTabPanel = UpdateTabPanel;
    API.UI.DoTabPanel = DoTabPanel;
    API.UI.DrawPixel = DrawPixel;
    API.UI.DrawButtonOutline = DrawButtonOutline;
    API.UI.DrawPanel = DrawPanel;
    API.UI.DrawRect = DrawRect;
    API.UI.FillRect = FillRect;
    API.UI.DrawCheckBoxPanel = DrawCheckBoxPanel;
    API.UI.InClippingPlaneX = InClippingPlaneX;
    API.UI.InClippingPlaneY = InClippingPlaneY;
    API.UI.InClippingPlane = InClippingPlane;
    API.UI.ClearClippingPlane = ClearClippingPlane;
    API.UI.SetClippingPlane = SetClippingPlane;
    API.UI.SetClippingPlaneZ = SetClippingPlaneZ;

    API.UI.IsCursorMode = IsCursorMode;
    API.UI.GetPressedButtons = GetPressedButtons;
    API.UI.GetHeldButtons = GetHeldButtons;
    API.UI.GetTouchPos = GetTouchPos;
    API.UI.GetCursorPos = GetCursorPos;
    API.UI.GetTouchDelta = GetTouchDelta;
    API.UI.GetCursorDelta = GetCursorDelta;

    API.UI.CreateButton = CreateButton;
    API.UI.CreateScrollZone = CreateScrollZone;
    API.UI.CreateCheckBox = CreateCheckBox;
    API.UI.CreateWindow = pCreateWindow;
    API.UI.CreateTabPanel = CreateTabPanel;
    API.UI.DisplayMessageFormat = DisplayMessageFormat;
    API.UI.DisplayMessage = DisplayMessage;
    API.UI.GetUserInput = GetUserInput;

    API.Process.AttachedProcessTitle = AttachedProcessTitle;
    API.Process.AttachedProcessName = AttachedProcessName;
    API.Process.DetachProcess = DetachProcess;
    API.Process.IsProcessAttached = IsProcessAttached;
    API.Process.GetProcess = GetProcess;
    API.Process.GetTitleInfo = GetTitleInfo;
    API.Process.GetTitleUpdateInfo1 = GetTitleUpdateInfo1;
    API.Process.GetTitleUpdateInfo2 = GetTitleUpdateInfo2;

    API.Process.Debug.AttachDebugger = AttachDebugger;
    API.Process.Debug.DetachDebugger = DetachDebugger;
    API.Process.Debug.IsDebugging = IsDebugging;
    API.Process.Debug.AllocateCodeCave = pAllocateCodeCave;
    API.Process.Debug.ReadProcessMemory = ReadProcessMemory;
    API.Process.Debug.WriteProcessMemory = WriteProcessMemory;
    
    pluginInit = true;
}

bool DestroyPlugin(Plugin *plugin){
    for (u32 i = 0; i < MAX_PLUGINS; i++) {
        if (plugins[i].hash == plugin->hash){
            Result res = mem_free(&(plugins[i].allocation));
            if (R_FAILED(res)) return false;
            plugins[i].address = 0;
            return true;
        }
    }
    return false;
}
// used as an intermediate stage for debugging
static void* memcpy_wrapper(void* dst, const void* src, size_t n){
    void* result = memcpy(dst, src, n);
    //DisplayMessageFormat("memcpy %08X, %08X, %d (%08X)", dst, src, n, result);
    return result;
}
static int memcmp_wrapper(const void* a, const void* b, size_t n){
    int result = memcmp(a, b, n);
    //DisplayMessageFormat("memcmp %08X, %08X, %d (%d)", a, b, n, result);
    return result;
}
static void* memset_wrapper(void* ptr, int value, size_t n){
    void* result = memset(ptr, value, n);
    //DisplayMessageFormat("memset %08X, %08X, %d (%d)", ptr, value, n, result);
    return result;
}
static u32 resolveSymbol(memRegion pluginRegion, const char* name){
    static u32 resolveSymbolTrampolineOffset = PLUGIN_END_DATA_OFFSET;
    // strcmp returns 0 on match
    u32 trampoline[2];
    trampoline[0] = 0xE51FF004; // LDR PC, [PC, #-4]
    if (strcmp(name, "memcpy") == 0){
        trampoline[1] = (u32)memcpy_wrapper;
    } else if (strcmp(name, "memcmp") == 0){
        trampoline[1] = (u32)memcmp_wrapper;
    }  else if (strcmp(name, "memset") == 0){
        trampoline[1] = (u32)memset_wrapper;
    } else {
        return 0;
    }
    u32 addr = (pluginRegion.address + pluginRegion.size - 0x1000) - resolveSymbolTrampolineOffset; // (-0x1000 so we get before the "state" page)
    //DisplayMessageFormat("resolving %s, %08X, %08X\n%08X, %08X", name, trampoline[1], addr, pluginRegion.address, pluginRegion.size);
    memcpy((void*)addr, (void*)trampoline, sizeof(u32) * 2);
    resolveSymbolTrampolineOffset += (sizeof(u32) * 2); // if this get too large were hooped
    return addr;
}
// will override p
bool CreatePlugin(u32 hashId, Plugin **plugin){
    Plugin *p;
    bool found = false;
    for (u32 i = 0; i < MAX_PLUGINS; i++) {
        if (plugins[i].address == 0){
            p = &plugins[i];
            found = true;
            break;
        }
    }
    if (!found) return false;
    p->hash = hashId;

    void* fileRunner = curPluginFile;
    u32 version = 0;
    memcpy(&version, fileRunner, sizeof(u32));    fileRunner += sizeof(u32);

    u32 textSize = 0;
    u32 dataSize = 0;
    u32 bssSize = 0;
    u32 rodataSize = 0;
    
    memcpy(&textSize, fileRunner, sizeof(u32));   fileRunner += sizeof(u32);
    memcpy(&dataSize, fileRunner, sizeof(u32));   fileRunner += sizeof(u32);
    memcpy(&bssSize, fileRunner, sizeof(u32));    fileRunner += sizeof(u32);
    memcpy(&rodataSize, fileRunner, sizeof(u32)); fileRunner += sizeof(u32);

    #define ROUND_4TH(n) (n % 4 != 0 ? n + (4 - (n % 4)) : n)
    textSize = ROUND_4TH(textSize);
    dataSize = ROUND_4TH(dataSize);
    bssSize = ROUND_4TH(bssSize);
    rodataSize = ROUND_4TH(rodataSize);
    p->textSize = textSize;
    p->dataSize = dataSize;
    p->bssSize = bssSize;
    p->rodataSize = rodataSize;

    u64 totalSize_64 = (u64)textSize + (u64)dataSize + (u64)bssSize + (u64)rodataSize;
    if (totalSize_64 >= (u64)MAX_PLUGINS_FILE_SIZE) return false; // check size
    u32 totalSize = (u32)totalSize_64;
    
    // only add PLUGIN_END_DATA_OFFSET 
    Result res = mem_alloc(&(p->allocation), PAGE_ROUND((totalSize + PLUGIN_END_DATA_OFFSET + 0xFFF)) + 0x1000, true); // round to ceiling of page, add one more page for plugin state (which becomes obsolete now because we do more then just rodata and text)
    if (R_FAILED(res)) return false;
    memRegion mem = p->allocation;
    p->stateAddress = (p->allocation.address + p->allocation.size) - 0x1000;
    p->address = p->allocation.address;
    
    u32 addr = mem.address;
    memcpy((void*)addr, fileRunner, totalSize);
    fileRunner += totalSize;
    u32 textPtr = addr;   addr += textSize;
    u32 dataPtr = addr;   addr += dataSize; 
    u32 bssPtr = addr;    addr += bssSize;
    memset((void*)bssPtr, 0, bssSize); // clear bss section;
    u32 rodataPtr = addr; addr += rodataSize;
    
    struct Reloc {
        u32 offset;
        u8 section;
        u8 reference;
        u8 relocType;
        u8 isCall;
    };
    u32 relocCount = 0;
    memcpy(&relocCount, fileRunner, sizeof(u32));
    p->rolocCount = relocCount;
    fileRunner += sizeof(u32);

    u32 symbolCount = 0;
    struct Reloc relocations[relocCount];
    for (size_t i = 0; i < relocCount; i++) {
        struct Reloc *reloc = &relocations[i];
        
        u32 offset = 0;
        memcpy(&offset, fileRunner, sizeof(u32));
        fileRunner += sizeof(u32);
        if ((u32)(fileRunner - (void*)curPluginFile) >= MAX_PLUGINS_FILE_SIZE){
            return false;
        }
        u32 data = 0;
        memcpy(&data, fileRunner, sizeof(u32));
        fileRunner += sizeof(u32);
        if ((u32)(fileRunner - (void*)curPluginFile) >= MAX_PLUGINS_FILE_SIZE){
            return false;
        }
        
        reloc->offset = offset;
        reloc->section =   ((data) & 0xFF);
        reloc->reference = ((data >> 8) & 0xFF);
        reloc->relocType = ((data >> 16) & 0xFF);
        reloc->isCall =    ((data >> 24) & 0xFF);
        if (reloc->isCall) symbolCount++;
    }

    u32 sectionBase[4];
    sectionBase[0] = textPtr;
    sectionBase[1] = dataPtr;
    sectionBase[2] = bssPtr;
    sectionBase[3] = rodataPtr;
    for (u32 i = 0; i < relocCount; i++) {
        u32 offset     = relocations[i].offset;
        u32 section  = relocations[i].section;
        u32 reference  = relocations[i].reference; // section or function relocation id (not "section")
        u32 relocType  = relocations[i].relocType;
        
        u32 basePtr = sectionBase[section];
        
        if (relocType == 0){ // R_ARM_REL32
            // R_ARM_REL32: S + A - P: (symbol addr (section in this case)) + (addend, original value, 0x5C, *P) - (address of patch, 0x144)
            u32* P = (u32*)(basePtr + offset);                      // main base address + relocation address
            u32 S = sectionBase[reference];                         // reference section base
            u32 A = *P;                                             // original value
            *P = S + A - (u32)P;                                    // calculate new value
        } else if (relocType == 1){ // R_ARM_CALL
            // R_ARM_CALL: ((S + A) | T) - P: (( (symbol addr) + (addend) ) | (thumb (ignored)) ) - (patch address)
            u32* P = (u32*)(basePtr + offset);                      // plugin base address + relocation address
            u32 instr = *P;                                         // instruction
            s32 wordOffset = (s32)((instr & 0x00FFFFFF) << 8) >> 8; // sign bottom 24-bit (word offset)
            s32 A = wordOffset << 2;                                // word offset to byte offset
            
            u32 S = resolveSymbol(mem, (char*)fileRunner);          // result symbol, uses raw pointer to the file runner to compare strings
            s32 rel = (s32)(S + (u32)A - ((u32)P));                 // this is supposed to have a +8 (or -8, idk) but it doesn't work with it so somewhere its already being added
            u32 newWordOffset = (u32)(rel >> 2) & 0x00FFFFFF;       // divide by 4 and take the bottom 24 bits
            *P = (instr & 0xFF000000) | newWordOffset;              // instruction and new offset
            fileRunner += strlen(fileRunner) + 1;
        }  else if (relocType == 2){ // R_ARM_ABS32 (does not work on functions)
            // R_ARM_ABS32: S + A ((symbol addr) + (addend))
            u32* P = (u32*)(basePtr + offset);                      // main base address + relocation address
            u32 S = sectionBase[reference];                         // reference section base
            u32 A = *P;                                             // original value
            *P = S + A;                                             // calculate new value
        } else continue;
        
    }
    
    extern void svcFlushEntireDataCache(void);
    svcFlushEntireDataCache();
    
    *plugin = p;
    return true;
}
void DrawDefaultPluginSpecificDebug(){

    if (!DEBUG_MODE) return;   

    UI_SetScreen(true);

    UI_FillRect(PADDING, PADDING, 175, TOP_SCREEN_HEIGHT - (2 * PADDING) - (20 + (1 * PADDING)), BASE_COLOUR);
    UI_DrawPanel(PADDING, PADDING, 175, TOP_SCREEN_HEIGHT - (2 * PADDING) - (20 + (1 * PADDING)), true); // make sure not to draw over default top screen stuff
    u32 stringY = 2 * PADDING;
    u32 stringX = 2 * PADDING;
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "Plugin Debug");
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "hashId : 0x%08X", curPlugin->hash);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "addr1  : 0x%08X", curPlugin->address);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "addr2  : 0x%08X", curPlugin->allocation.address);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "state  : 0x%08X", curPlugin->stateAddress);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "size   : 0x%08X", curPlugin->allocation.size);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, ".text  : 0x%08X", curPlugin->textSize);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, ".rodata: 0x%08X", curPlugin->rodataSize);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, ".data  : 0x%08X", curPlugin->dataSize);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, ".bss   : 0x%08X", curPlugin->bssSize);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "counter: %d", curPlugin->updateCounter);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "reloc  : %d", curPlugin->rolocCount);

    UI_SetScreen(false);

}
MenuResult ctn_Menu_RunPlugin(){

    if (!pluginInit){
        initPlugins();
    }

    u32 size = sizeof(PluginAPI) / 4;
    u32* apiPtr = (u32*)&API;
    //bool validAPI = true;
    for (u32 i = 0; i < size; i++) {
        if (i != 1 && apiPtr[i] == 0){ // skip second u32 because its 4 bools
            UI_DisplayMessageFormat("Invalid API data. Function %d NULL", i);
            _CloseMenu();
            return MENU_RESULT_OK;
        }
    }

    Button backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);
    Button stopButton = UI_CreateButton(BOTTOM_SCREEN_WIDTH - PADDING - 50, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Stop", BASE_COLOUR, 0x000000);
    Button executeButton = UI_CreateButton(2 * PADDING, 2 * PADDING + 20, 50, 20, "Run", BASE_COLOUR, 0x000000);

    svcGetSystemTick();

    bool running = false;
    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Plugin Menu");
        DefaultWindowStart();
        MENU_BCLOSE;

        if (curPlugin == NULL){
            UI_DrawString(PADDING * 2, PADDING + 20 + PADDING, 0xFF0000, "curPlugin NULL");
        } else {
            if (curPlugin->stateAddress == 0){
                UI_DrawString(PADDING * 2, PADDING + 20 + PADDING, 0xFF0000, "stateAddress NULL");
            } else if (curPlugin->address == 0){
                UI_DrawString(PADDING * 2, PADDING + 20 + PADDING, 0xFF0000, "address NULL");
            } else {
                if (!running) {
                    if (DEBUG_MODE){
                        MENU_DO_BUTTON(executeButton) running = true;
                    } else running = true;
                } else {
                    API.PLUGIN_FIRST = false;
                    API.PLUGIN_LAST = false;
                    if (curPlugin->updateCounter == 0){
                        curPlugin->updateCounter = 1;
                        API.PLUGIN_FIRST = true;
                    }
                    bool (*pluginUpdate)(PluginAPI *api, void *_state) = (bool(*)(PluginAPI *api, void *_state))(curPlugin->address);
                    if (!pluginUpdate(&API, (void*)curPlugin->stateAddress)){
                        running = false;
                    }
                }
            }

        }

        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_DO_BUTTON(stopButton){
            if (UI_DisplayMessage("Are you sure you want to stop this plugin?\nPlugin code and memory will be discarded.\n\nA - Yes\nB - No")){
                API.PLUGIN_LAST = true;
                if (running){
                    bool (*pluginUpdate)(PluginAPI *api, void *_state) = (bool(*)(PluginAPI *api, void *_state))(curPlugin->address);
                    pluginUpdate(&API, (void*)curPlugin->stateAddress);
                }
                if (!DestroyPlugin(curPlugin)){
                    UI_DisplayMessage("Failed to destroy plugin.");
                }
                curPlugin = NULL;
                MENU_CLOSE_MENU();
            }
        }
        
        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
        DrawDefaultPluginSpecificDebug();
    } MENU_LOOP_END();

    curPlugin = NULL;

    return 0;
}

void DrawDefaultPluginsDebug(){

    if (!DEBUG_MODE) return;

    UI_SetScreen(true);

    UI_FillRect(PADDING, PADDING, 175, TOP_SCREEN_HEIGHT - (2 * PADDING) - (20 + (1 * PADDING)), BASE_COLOUR);
    UI_DrawPanel(PADDING, PADDING, 175, TOP_SCREEN_HEIGHT - (2 * PADDING) - (20 + (1 * PADDING)), true); // make sure not to draw over default top screen stuff
    u32 stringY = 2 * PADDING;
    u32 stringX = 2 * PADDING;
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "Max Plugins: %d", MAX_PLUGINS);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "Max Size   : 0x%08X", MAX_PLUGINS_FILE_SIZE);
    stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "Plugins:");
    for (int i = 0; i < MAX_PLUGINS; i++) {
        Plugin p = plugins[i];
        if (p.address != 0){
            stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "%d: 0x%08X", i, p.hash);
            stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "  Addr: 0x%08X", p.allocation.address);
            stringY = UI_DrawStringFormat(stringX, stringY, 0x000000, "  Size: 0x%08X", p.allocation.size);
        }
    }

    UI_SetScreen(false);

}
MenuResult ctn_Menu_PluginMenu(){
    #define MAX_ENTRY 32

    static Button backButton;
    static ScrollZone fileScroll;
    static Button entryButtons[MAX_ENTRY];

    MENU_ONCE_START{
        // renamed to exit to make it less confusing
        backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);
        fileScroll = UI_CreateScrollZone(1 * PADDING, PADDING + 20 + PADDING + SPACING_Y + PADDING, BOTTOM_SCREEN_WIDTH - (2 * PADDING), BOTTOM_SCREEN_HEIGHT - (2 * 20) - (4 * PADDING) - SPACING_Y - PADDING);
    
        for (int i = 0; i < MAX_ENTRY; i++) { entryButtons[i] = UI_CreateButton(0, 0, 4 * SPACING_X + 6, SPACING_Y, "Open", BASE_COLOUR, 0x000000); entryButtons[i].textOffsetX = -2; }

    } MENU_ONCE_END;

    FS_Archive archive;
    bool archiveOpened = openArchive(&archive);
    setCacheArchive(archive);
    u32 entriesRead = 0;
    static DirectoryData entries[MAX_ENTRY];

    bool pluginValid = false;
    u32 pluginCount = 0;

    if (!directoryExists("/luma/ctn")) {
        directoryCreate("/luma/ctn");
    }
    
    pluginValid = enumerateFiles("/luma/ctn", entries, MAX_ENTRY, &pluginCount, 0);

    s32 selectedIndex = -1;

    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("File Menu");
        DefaultWindowStart();
        MENU_BCLOSE;


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


        UI_DrawPanel(fileScroll.x, fileScroll.y, fileScroll.w, fileScroll.h, true);
        
        if (!archiveOpened){
            UI_DrawStringFormat(PADDING * 2, PADDING + 20 + PADDING, 0xFF0000, "Open Archive Fail");
        } else if (!pluginValid){
            UI_DrawStringFormat(PADDING * 2, PADDING + 20 + PADDING, 0xFF0000, "Enumerate File Error");
        } else {
            UI_DrawStringFormat(PADDING * 2, PADDING + 20 + PADDING, 0x000000, "ctn Plugins: /luma/ctn");

            u32 stringX = fileScroll.x + PADDING;// - fileScroll.scrollX;
            s32 stringY = fileScroll.y + PADDING - fileScroll.scrollY;
            
            UI_SetClippingPlaneZ(fileScroll);

            curPlugin = NULL;
            for (u32 i = 0; i < MAX_ENTRY; i++) {
                if (entries[i].attributes == 0 && entries[i].name[0] == 0) continue; // if no atrributes and no name its likely not a file
                bool isDir = entries[i].attributes & FS_ATTRIBUTE_DIRECTORY;
                if (isDir) continue; // ignore folders
                bool valid = entries[i].fileSize <= MAX_PLUGINS_FILE_SIZE && memcmp(entries[i].name + strlen(entries[i].name) - 5, ".cplg", 5) == 0;
                bool isSel = selectedIndex == (s32)i;
                u32 colour = isSel ? 0x007F00 : (valid ? 0x000000 : 0xFF0000); // mid-green, black, red
                entryButtons[i].x = stringX;
                entryButtons[i].y = stringY;
                if (valid) MENU_DO_BUTTON_OR(entryButtons[i], (isSel && pressed & KEY_A)){
                    Handle fh = 0;
                    sprintf(curPluginPath, "/luma/ctn/%.64s", entries[i].name);
                    u32 hash = murmurHash3(curPluginPath, 512, 0);
                    // check if plugin already exists, if a hash collisions occurs, it should just open the wrong plugin
                    for (u32 i = 0; i < MAX_PLUGINS; i++) {
                        if (plugins[i].address != 0 && plugins[i].hash == hash){
                            curPlugin = &plugins[i];
                            break;
                        }
                    }
                    if (curPlugin) break;
                    // check if empty plugin exists
                    bool emptyExists = false;
                    for (u32 i = 0; i < MAX_PLUGINS; i++) {
                        if (plugins[i].address == 0){
                            emptyExists = true;
                            break;
                        }
                    }
                    if (!emptyExists){
                        UI_DisplayMessageFormat("Cannot create another plugin.\nMax %d plugins.", MAX_PLUGINS); 
                        break;
                    }
                    Result res = FSUSER_OpenFile(&fh, archive, fsMakePath(PATH_ASCII, curPluginPath), FS_OPEN_READ, 0);
                    if (R_SUCCEEDED(res)){
                        u32 readBytes = 0;
                        FSFILE_Read(fh, &readBytes, 0, curPluginFile, (u32)entries[i].fileSize);
                        FSFILE_Close(fh);
                        if (!CreatePlugin(hash, &curPlugin)){
                           UI_DisplayMessage("CreatePlugin Failed"); 
                           break;
                        }
                    } else UI_DisplayMessage("Failed to open file");
                    
                    break;
                }
                UI_DrawStringFormatSized(0, valid ? (stringX + entryButtons[i].w + 2) : stringX, stringY, colour, "%s (0x%08X)", entries[i].name, entries[i].fileSize);
                stringY += SPACING_Y;
            }

            if (curPlugin){
                MENU_CHANGE_MENU(ctn_Menu_RunPlugin);
            }

            UI_UpdateScrollZone(&fileScroll);
    
            UI_ClearClippingPlane();
        }

        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        
        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
        DrawDefaultPluginsDebug();
    } MENU_LOOP_END();

    if (archive != 0) {
        FSUSER_CloseArchive(archive);
    }

    closeArchive(archive); // not needed but just in case
    closeCachedArchive(archive);

    return MENU_RESULT_OK;
}
