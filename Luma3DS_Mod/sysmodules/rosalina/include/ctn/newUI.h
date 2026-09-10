#pragma once

#include <3ds.h>
#include <stdarg.h> // for va_start and va_end 

#include <ctn/global_types.h>

#define TOP_SCREEN_WIDTH  400
#define TOP_SCREEN_HEIGHT 240
#define TOP_SCREEN_SIZE (TOP_SCREEN_WIDTH * TOP_SCREEN_HEIGHT * 2) // times 2 because there 2 bytes pixels

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240
#define BOTTOM_SCREEN_SIZE (BOTTOM_SCREEN_WIDTH * BOTTOM_SCREEN_HEIGHT * 2) // times 2 because there 2 bytes pixels

#define SPACING_Y 11
#define SPACING_X 6

static const unsigned char font2[] = {
    /* 0 0x00 'nothing, recurved for \0' */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */
    0x00, /* 00000000 */

    #define FONT_SYMBOL_ERROR 1
    /* 1 0x01 'error symbol' */
    0b00000000, /* ------ */
    0b11111100, /* XXXXXX */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b11111100, /* XXXXXX */
    0b00000000, /* ------ */
    0b00000000, /* ------ */

    #define FONT_SYMBOL_DELTA 2
    /* 2 0x02 'delta symbol' */
    0b00000000, /* ------ */
    0b00010000, /* ---X-- */
    0b00101000, /* --X-X- */
    0b00101000, /* --X-X- */
    0b01000100, /* -X---X */
    0b01000100, /* -X---X */
    0b01000100, /* -X---X */
    0b01111100, /* -XXXXX */
    0b00000000, /* ------ */
    0b00000000, /* ------ */
    
    #define FONT_SYMBOL_BATTERY_1 3
    /* 3 0x03 'battery 1' */
    0b00110000, /* --XX-- */
    0b11111100, /* XXXXXX */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b11111100, /* XXXXXX */
    0b00000000, /* ------ */

    #define FONT_SYMBOL_TEMPERATURE 4
    /* 4 0x04 'temperature' */
    0b00110000, /* --XX-- */
    0b01001000, /* -X--X- */
    0b01011000, /* -X-XX- */
    0b01001000, /* -X--X- */
    0b01011000, /* -X-XX- */
    0b10000100, /* X----X */
    0b10000100, /* X----X */
    0b01001000, /* -X--X- */
    0b00110000, /* --XX-- */
    0b00000000, /* ------ */

    #define FONT_SYMBOL_CHECK 4
    /* 4 0x04 'temperature' */
    0b00110000, /* ------ */
    0b01001000, /* ------ */
    0b01011000, /* -----X */
    0b01001000, /* ----XX */
    0b01011000, /* ---XX- */
    0b10000100, /* X-XX-- */
    0b10000100, /* XXX--- */
    0b01001000, /* -X---- */
    0b00110000, /* ------ */
    0b00000000, /* ------ */
};

typedef u32 MenuResult;

// these values are not used for anything just for representation
enum MENU_RESULTS{
    MENU_RESULT_OK,
    MENU_RESULT_REBOOT
};

u16 RGB888toRGB565(u32 rgb888);

u16 *_Debug_GetScreenBuffer();
bool _PointInBuffer(u32 x, u32 y);
void _SetLineY(s32 x, s32 y, u32 h, u16 rgb565_colour);

void UI_MeasureString(u32 *sizeX, u32 *sizeY, char* string);
u32 UI_DrawString(s32 posX, s32 posY, u32 textColour, char* string);
u32 UI_DrawStringFormatSized_Args(u32 bufferSize, s32 posX, s32 posY, u32 textColour, char* formatString, va_list arg);
u32 UI_DrawStringFormatSized(u32 bufferSize, s32 posX, s32 posY, u32 textColour, char* formatString, ...);
u32 UI_DrawStringFormat_Args(s32 posX, s32 posY, u32 textColour, char* formatString, va_list arg);
u32 UI_DrawStringFormat(s32 posX, s32 posY, u32 textColour, char* formatString, ...);

// front end

void UI_DrawButton(Button *button);
bool UI_UpdateButton(Button *button);
bool UI_DoButton(Button *button);

bool UI_UpdateScrollZone(ScrollZone *scroll);

void UI_DrawCheckBox(CheckBox *checkBox);
bool UI_UpdateCheckBox(CheckBox *checkBox);
bool UI_DoCheckBox(CheckBox *checkBox);

void UI_DrawWindow(Window *window);
bool UI_UpdateWindow(Window *window);

void UI_DrawTabPanel(TabPanel *tabPanel);
bool UI_UpdateTabPanel(TabPanel *tabPanel);
bool UI_DoTabPanel(TabPanel *tabPanel);

bool UI_DrawPixel(s32 x, s32 y, u32 colour);
void UI_DrawButtonOutline(s32 x, s32 y, u32 w, u32 h, bool down);
void UI_DrawPanel(s32 x, s32 y, u32 w, u32 h, bool down);
void UI_DrawRect(s32 x, s32 y, u32 w, u32 h, u32 colour);
void UI_FillRect(s32 x, s32 y, u32 w, u32 h, u32 colour);
void UI_DrawCheckBoxPanel(s32 x, s32 y, u32 size);

bool UI_InClippingPlaneX(s32 x);
bool UI_InClippingPlaneY(s32 y);
bool UI_InClippingPlane(s32 x, s32 y);
void UI_ClearClippingPlane();
void UI_SetClippingPlane(s32 x, s32 y, u32 w, u32 h);
void UI_SetClippingPlaneZ(ScrollZone z);


u32 _DoMenu();
bool _CloseMenu();
bool _OpenMenu(MenuResult (*foo)());

typedef struct {
    u8 format;
    u16 stride;
    bool isNormal2d; // not sure what this means but its in rosalina
    bool is3d; // not sure what this means but its in rosalina
    u32 raw;
} FramebufferFormat;
FramebufferFormat _UI_GetTopScreenFormat();
void _UI_CopyTopScreen();
void _UI_InitTopScreen();
void _UI_DeinitTopScreen();

void UI_SetScreen(bool top);
bool _UI_IsTopScreenAvailable();

bool UI_IsCursorMode();
u32 UI_GetPressedButtons();
u32 UI_GetHeldButtons();
UTouchPoint UI_GetTouchPos();
STouchPoint UI_GetCursorPos();
STouchPoint UI_GetTouchDelta();
STouchPoint UI_GetCursorDelta();

void UI_FlushScreens();
u64 UI_FrameTime();
void UI_UpdatePressedInput();
void UI_StartFrame();
void UI_EndFrame();

Button UI_CreateButton(s32 x, s32 y, u32 w, u32 h, char *string, u32 colour, u32 textColour);
ScrollZone UI_CreateScrollZone(s32 x, s32 y, u32 w, u32 h);
CheckBox UI_CreateCheckBox(s32 x, s32 y, u32 size, char *checkChar, u32 textColour, u32 colour, bool checked);
Window UI_CreateWindow(s32 x, s32 y, u32 w, u32 h, char* title);
TabPanel UI_CreateTabPanel(s32 x, s32 y, u32 w, u32 h, u32 btnW, u32 btnH, char **titles, u32 tabCount);

bool UI_DisplayMessageFormat_Args(char* str, va_list arg);
bool UI_DisplayMessageFormat(char* str, ...);
bool UI_DisplayMessage(char* str);
bool UI_GetUserInput(char* message, u32 maxInputLength, char *ouputBuffer);
bool UI_CheckWithUserFormat(char* message, char *buttonNo, char *buttonYes, ...);
bool UI_CheckWithUser(char* message, char *buttonNo, char *buttonYes);

#define MENU_LOOP_START() while(true) { UI_StartFrame();
#define MENU_LOOP_END() UI_EndFrame(); } UI_EndFrame();
#define MENU_BCLOSE if (UI_GetPressedButtons() & KEY_B) { _CloseMenu(); break; }

#define MENU_CHANGE_MENU(menu) { if (_OpenMenu(menu)){ UI_EndFrame(); break; } }
#define MENU_CLOSE_MENU() { _CloseMenu(); break; }

#define MENU_ONCE_START static bool _once = false; if (!_once) { ;
#define MENU_ONCE_END ; _once = true; }

#define MENU_DRAW_BACKGROUND() UI_FillRect(0, 0, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT, BASE_COLOUR);
#define MENU_DRAW_TITLE(string) UI_FillRect(PADDING, PADDING, BOTTOM_SCREEN_WIDTH - (2 * PADDING), 20, BASE_COLOUR); UI_DrawPanel(PADDING, PADDING, BOTTOM_SCREEN_WIDTH - (2 * PADDING), 20, true); UI_DrawString(PADDING + 4, PADDING + 4, 0x000000, string);
#define MENU_DO_BUTTON(button) if ((UI_UpdateButton(&button) ? (UI_DrawButton(&button), 1) : (UI_DrawButton(&button), 0)))
#define MENU_DO_BUTTON_OR(button, or) if ((UI_UpdateButton(&button) ? (UI_DrawButton(&button), 1) : (UI_DrawButton(&button), 0) || or))

