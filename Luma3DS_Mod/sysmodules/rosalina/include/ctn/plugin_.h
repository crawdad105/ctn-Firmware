#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "global_types.h"

// from types.h
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef struct ProcessData {
    u64 titleId;
    u32 pid;
    char name[9];
} ProcessData;

typedef enum {
    NONE,
    SD,
    CARD,
    NAND
} TitleLocation;

typedef struct {
	u64 titleID;
	u64 size;
	u16 version;
	u8 unk[6];
    TitleLocation loc;
} TitleInfo;

typedef struct{
    // this should always be the first function
    u32 (*API_VERSION)();
    bool PLUGIN_FIRST;
    bool PLUGIN_LAST;
    bool PLUGIN_RESERVED_1;
    bool PLUGIN_RESERVED_2;
    struct {
        u32 (*DrawString) (s32 posX, s32 posY, u32 textColour, char* string);
        u32 (*DrawStringFormatSized) (u32 bufferSize, s32 posX, s32 posY, u32 textColour, char* formatString, ...);
        u32 (*DrawStringFormat) (s32 posX, s32 posY, u32 textColour, char* formatString, ...);
        void (*DrawButton) (Button *button);
        bool (*UpdateButton) (Button *button);
        bool (*DoButton) (Button *button);
        bool (*UpdateScrollZone) (ScrollZone *scroll);
        void (*DrawCheckBox) (CheckBox *checkBox);
        bool (*UpdateCheckBox) (CheckBox *checkBox);
        bool (*DoCheckBox) (CheckBox *checkBox);
        void (*DrawWindow) (Window *window);
        bool (*UpdateWindow) (Window *window);
        void (*DrawTabPanel) (TabPanel *tabPanel);
        bool (*UpdateTabPanel) (TabPanel *tabPanel);
        bool (*DoTabPanel) (TabPanel *tabPanel);
        bool (*DrawPixel) (s32 x, s32 y, u32 colour);
        void (*DrawButtonOutline) (s32 x, s32 y, u32 w, u32 h, bool down);
        void (*DrawPanel) (s32 x, s32 y, u32 w, u32 h, bool down);
        void (*DrawRect) (s32 x, s32 y, u32 w, u32 h, u32 colour);
        void (*FillRect) (s32 x, s32 y, u32 w, u32 h, u32 colour);
        void (*DrawCheckBoxPanel) (s32 x, s32 y, u32 size);
        bool (*InClippingPlaneX) (s32 x);
        bool (*InClippingPlaneY) (s32 y);
        bool (*InClippingPlane) (s32 x, s32 y);
        void (*ClearClippingPlane) ();
        void (*SetClippingPlane) (s32 x, s32 y, u32 w, u32 h);
        void (*SetClippingPlaneZ) (ScrollZone z);
        bool (*IsCursorMode) ();
        u32 (*GetPressedButtons) ();
        u32 (*GetHeldButtons) ();
        UTouchPoint (*GetTouchPos) ();
        STouchPoint (*GetCursorPos) ();
        STouchPoint (*GetTouchDelta) ();
        STouchPoint (*GetCursorDelta) ();
        Button (*CreateButton) (s32 x, s32 y, u32 w, u32 h, char *string, u32 colour, u32 textColour);
        ScrollZone (*CreateScrollZone) (s32 x, s32 y, u32 w, u32 h);
        CheckBox (*CreateCheckBox) (s32 x, s32 y, u32 size, char *checkChar, u32 textColour, u32 colour, bool checked);
        Window (*CreateWindow) (s32 x, s32 y, u32 w, u32 h, char* title);
        TabPanel (*CreateTabPanel) (s32 x, s32 y, u32 w, u32 h, u32 btnW, u32 btnH, char **titles, u32 tabCount);
        bool (*DisplayMessageFormat) (char* str, ...);
        bool (*DisplayMessage) (char* str);
        bool (*GetUserInput) (char* message, u32 maxInputLength, char *ouputBuffer);
    } UI;
    struct {
        bool (*AttachedProcessTitle) (u64 defaultTitle);
        bool (*AttachedProcessName) (const char* name);
        bool (*DetachProcess) ();
        bool (*IsProcessAttached) ();
        ProcessData (*GetProcess) ();
        TitleInfo (*GetTitleInfo) (u64 title);
        TitleInfo (*GetTitleUpdateInfo1) (u64 defaultTitle);
        TitleInfo (*GetTitleUpdateInfo2) (u64 updateTitle);

        struct {
            bool (*AttachDebugger) ();
            bool (*DetachDebugger) ();
            bool (*IsDebugging) ();
            bool (*AllocateCodeCave) (u32 *address);
            bool (*ReadProcessMemory) (u32 address, u32 size, void *buffer);
            bool (*WriteProcessMemory) (u32 address, u32 size, void *buffer);
        } Debug;
    } Process;
} PluginAPI;

#define TOP_SCREEN_WIDTH  400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define TEXT_SPACING_Y 11
#define TEXT_SPACING_X 6
#define TEXT_PADDING 5

#define BASE_COLOUR 0xC0C0C0 // gray
#define THEME_COLOUR 0x716CCD // purpleish-blue (same as cModLoader)
