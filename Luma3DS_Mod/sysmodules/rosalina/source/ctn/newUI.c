
#include "font.h" // for font[]
#include <string.h> // for strlen
#include <stdio.h> // for vsprintf
#include <stdarg.h> // for va_start and va_end
#include "ctn/ctn.h"
#include "ctn/newUI.h"

// 0x1F000000 is virtual, 0x18000000 is physical https://www.3dbrew.org/wiki/Memory_layout#:~:text=in%20this%20range)-,0x1F000000,0x18000000,-0x00600000
#define VRAM_BASE 0x1F000000
#define VRAM_BASE_PHYSICAL 0x18000000
#define VRAM_TOP_SCREEN_LEFT_0_OFFSET 0x1E6000 // https://www.3dbrew.org/wiki/Memory_layout#:~:text=framebuffer%201(240x400x3)-,0x48F000,-%2D0x4C7400%20%2D%2D%20bottom%20screen
#define VRAM_TOP_SCREEN_RIGHT_0_OFFSET 0x22C800 // https://www.3dbrew.org/wiki/Memory_layout#:~:text=set%20to%20%22off%22)-,0x22C800,-%2D0x272D00%20%2D%2D%20top%20screen
#define VRAM_BOTTOM_SCREEN_0_OFFSET 0x48F000 // https://www.3dbrew.org/wiki/Memory_layout#:~:text=Running%20System%20Applets-,0x1E6000,-%2D0x22C500%20%2D%2D%20top%20screen

#define TOP_LEFT_VRAM_ADDR ((void *)(VRAM_BASE + VRAM_TOP_SCREEN_LEFT_0_OFFSET))
#define TOP_RIGHT_VRAM_ADDR ((void *)(VRAM_BASE + VRAM_TOP_SCREEN_RIGHT_0_OFFSET))

#define TOP_LEFT_VRAM_ADDR_PA (VRAM_BASE_PHYSICAL + VRAM_TOP_SCREEN_LEFT_0_OFFSET)
#define TOP_RIGHT_VRAM_ADDR_PA (VRAM_BASE_PHYSICAL + VRAM_TOP_SCREEN_RIGHT_0_OFFSET)

#define BOTTOM_VRAM_ADDR ((void *)(VRAM_BASE + VRAM_BOTTOM_SCREEN_0_OFFSET)) // this is from draw.h but modified
// dont need BOTTOM_VRAM_ADDR_PA because its not used here

// draw to these buffer to reduse screen flickering due to "scanout seeing"
static u16 bottomScreenBuffer[BOTTOM_SCREEN_SIZE / 2];
static u16 topScreenBuffer[TOP_SCREEN_SIZE / 2]; 
static void *topScreenCopyBuffer_left;

static u32 _UI_PressedButtons;
static u32 _UI_HeldButtons;
static UTouchPoint _UI_TouchCurrent;
static STouchPoint _UI_CursorCurrent;
static STouchPoint _UI_TouchDelta;
static STouchPoint _UI_CursorDelta;
//static touchPosition _UI_TouchDown;
static u64 startTime = 0;
static bool singleInteraction;
static bool wasInteraction;
//static u64 suspendTime = 0;
static bool frameStarted = false;
static char errorBuffer[256];
static bool topScreenAvailable = false;

#define MAX_MENU_STACK 64
static MenuResult (*menuStack[MAX_MENU_STACK])(); // 64 could run but idk what to do if it does. first element goes unused
static u32 menuIndex = 0;

static u32 CUR_SCREEN_HEIGHT = BOTTOM_SCREEN_HEIGHT;
static u32 CUR_SCREEN_WIDTH = BOTTOM_SCREEN_WIDTH;
static u16* CUR_SCREEN_BUFFER = bottomScreenBuffer;

typedef struct { s32 x; s32 y; s32 endX; s32 endY; } PlaneRect;

static PlaneRect clippingPlane = { 0, 0, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT};


// needed because for some reason rosalina uses RGB565 on the bottom screen (and i guess now this uses it for the top screen to stay on brand)
u16 RGB888toRGB565(u32 rgb888) {
    return ((rgb888 & 0xF80000) >> 8)  | ((rgb888 & 0x00FC00) >> 5)  | ((rgb888 & 0x0000F8) >> 3);
}

///
/// Native Drawing
///  Send normal coordinates in and these will flip them
///

u16 *_Debug_GetScreenBuffer(){
    return CUR_SCREEN_BUFFER;
}
static s32 FlipY(s32 y){
    return (s32)CUR_SCREEN_HEIGHT - y - 1;
}
bool _PointInBuffer(u32 x, u32 y){
    return (x * CUR_SCREEN_HEIGHT + y) < (CUR_SCREEN_HEIGHT * CUR_SCREEN_WIDTH);
}
static bool _SetPixelPos(s32 x, s32 y, u16 rgb565_colour){
    if (!UI_InClippingPlane(x, y)) return false;
    u32 sy = FlipY(y);
    u32 sx = (u32)x;
    if (!_PointInBuffer(sx, sy)) return false;
    CUR_SCREEN_BUFFER[sx * CUR_SCREEN_HEIGHT + sy] = rgb565_colour;
    return true;
}
void _SetLineY(s32 x, s32 y, u32 h, u16 rgb565_colour){ 
    //bool inBounds = (clippingPlane.x <= x && clippingPlane.endX > x) && (clippingPlane.y <= y && clippingPlane.endY > y);
    if (!UI_InClippingPlaneX(x)) return;
    // check if line is entirely out of bounds
    if ((clippingPlane.y > (y + (s32)h)) || (clippingPlane.endY < y)) return;

    // change bounds to be inside clipping plane
    s32 startY = y < clippingPlane.y ? clippingPlane.y : y;
    s32 endY = (y + (s32)h) > (s32)clippingPlane.endY ? clippingPlane.endY : (y + (s32)h);

    u32 ux = x;
    // extra +1 because we draw going upwards which has it offset by -1
    u32 uStartY = (u32)((s32)BOTTOM_SCREEN_HEIGHT - startY - 1 + 1);
    u32 uEndY = (u32)((s32)BOTTOM_SCREEN_HEIGHT - endY - 1 + 1);
    if (!_PointInBuffer(ux, uStartY) || !_PointInBuffer(ux, uEndY)) return; // final check to stay in bounds
    u16* col = _Debug_GetScreenBuffer() + (ux * BOTTOM_SCREEN_HEIGHT);
    for (u32 i = uEndY; i < uStartY; i++) col[i] = rgb565_colour;
}

///
/// UI Backend
///

void UI_MeasureString(u32 *sizeX, u32 *sizeY, char* string){
    
    bool inFormat = false;
    u32 maxX = 0;
    u32 charX = 0;
    u32 posY = SPACING_Y;
    u32 len = strlen(string);
    for(u32 i = 0; i < len; i++) {
        char c = string[i];
        if (c == '\n'){
            posY += SPACING_Y;
            charX = 0;
        } else if (c == '\t'){
            charX += (2 * SPACING_X);
        } else {
            if (string[i] == '[' && (i + 2 < len) && string[i + 2] == '/'){
                char type = string[i + 1];
                if (!inFormat && type == 'c' && (i + 2 + 7 < len) && string[i + 2 + 7] == ':'){ // colour
                    inFormat = true;
                    i += 9;
                    continue;
                } else if (type == 'g' && (i + 2 + 2 < len) && string[i + 2] == '/' && string[i + 2 + 2] == ']'){ // glyph
                    i += 4;
                }
            } else if (inFormat && string[i] == ']'){
                inFormat = false;
                continue;
            }
            charX += SPACING_X;
        }
        if (charX >= maxX) maxX = charX + 2; // +2 because it works
    }
    if (sizeX != NULL) *sizeX = maxX;
    if (sizeY != NULL) *sizeY = posY;
}
u32 UI_DrawString(s32 posX, s32 posY, u32 textColour, char* string) { // heavily modified from draw.c (optimized and added terraria style text formating) // TODO: optimize to use cacheing
    u32 baseColour = RGB888toRGB565(textColour);
    u32 curColour = baseColour;
    bool inFormat = false;
    u32 charX = 0;
    u32 len = strlen(string);
    for(u32 i = 0; i < len; i++) {
        char c = string[i];
        if (c == '\n'){
            posY += SPACING_Y;
            charX = 0;
        } else if (c == '\t'){
            charX += (2 * SPACING_X);
        } else {
            bool charset2 = false;
            if (string[i] == '[' && (i + 2 < len) && string[i + 2] == '/'){
                char type = string[i + 1];
                if (!inFormat && type == 'c' && (i + 2 + 7 < len) && string[i + 2 + 7] == ':'){ // colour
                    inFormat = true;
                    // basically hex to dec code. other non hex values will be 0
                    for (int j = 0; j < 6; j++) {
                        char c = string[i + 3 + j];
                        int v = 0;
                        if (c >= '0' && c <= '9') v = c - '0';
                        if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
                        if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
                        curColour += v << ((6 - j - 1) * 4); // inverse j
                    }
                    i += 9;
                    curColour = RGB888toRGB565(curColour);
                    continue;
                } else if (type == 'g' && (i + 2 + 2 < len) && string[i + 2] == '/' && string[i + 2 + 2] == ']'){ // glyph
                    c = string[i + 2 + 1]; // cant be 0
                    if ((unsigned char)c > (sizeof(font2) / 10)){
                        c = FONT_SYMBOL_ERROR;
                    }
                    charset2 = true;
                    i += 4;
                }
            } else if (inFormat && string[i] == ']'){
                curColour = baseColour;
                inFormat = false;
                continue;
            }
            u8 const* fontSet = charset2 ? font2 : font;
            u32 y = posY;
            for (s32 j = 0; j < 10; j++, y++) { // h (flip y so it draws properly)
                if (!UI_InClippingPlaneY(y)) continue;
                u32 x = (posX + charX);
                u8 v = fontSet[(c * 10) + j];
                if (v == 0) { }
                else {
                    for (s32 k = 8 - 1; k >= 2; k--, x++) { // w
                        if ((v >> k) & 1) {
                            _SetPixelPos(x, y, curColour);
                        }
                    }
                }
            }
            charX += SPACING_X;
        }
    }
    return (posY + SPACING_Y);
} 
u32 UI_DrawStringFormatSized_Args(u32 bufferSize, s32 posX, s32 posY, u32 textColour, char* formatString, va_list arg){
    #define LARGE_TEXT_BUFFER_SIZE 1024 // quarter page
    static char largeTextBuffer[LARGE_TEXT_BUFFER_SIZE];
    u32 ret = 0;
    if (bufferSize > LARGE_TEXT_BUFFER_SIZE){
        char buffer[bufferSize];
        vsprintf(buffer, formatString, arg);
        ret = UI_DrawString(posX, posY, textColour, buffer);
    } else {
        memset(largeTextBuffer, 0, LARGE_TEXT_BUFFER_SIZE);
        vsprintf(largeTextBuffer, formatString, arg);
        ret = UI_DrawString(posX, posY, textColour, largeTextBuffer);
    }
    return ret;
}
u32 UI_DrawStringFormatSized(u32 bufferSize, s32 posX, s32 posY, u32 textColour, char* formatString, ...) { // minimum size 1024, setting bufferSize less then 1024 this will use a predefined larger buffer
    va_list arg; 
    va_start(arg, formatString);
    u32 ret = UI_DrawStringFormatSized_Args(bufferSize, posX, posY, textColour, formatString, arg);
    va_end(arg);
    return ret;
}
u32 UI_DrawStringFormat_Args(s32 posX, s32 posY, u32 textColour, char* formatString, va_list arg){
    return UI_DrawStringFormatSized_Args(128, posX, posY, textColour, formatString, arg);
}
u32 UI_DrawStringFormat(s32 posX, s32 posY, u32 textColour, char* formatString, ...) { // formating as in arguments, by default all strings draw with terraria style formating
    va_list arg; 
    va_start(arg, formatString);
    u32 ret = UI_DrawStringFormat_Args(posX, posY, textColour, formatString, arg);
    va_end(arg);
    return ret;
}
static void _DrawButton(s32 x1, s32 y1, s32 x2, s32 y2, bool pressed) { // TODO: switch to line rendering
    u32 white = RGB888toRGB565(0xFFFFFF);
    u32 black = RGB888toRGB565(0x000000);
    u32 outline1 = RGB888toRGB565(0xDFDFDF);
    u32 outline2 = RGB888toRGB565(0x808080);

    if (pressed){
        // top
        for (s32 x = x1; x < x2; x++)
            _SetPixelPos(x, y1, black);
        // bottom
        for (s32 x = x1; x < x2 - 1; x++)
            _SetPixelPos(x, y2 - 1, white);
        // left
        for (s32 y = y1 + 1; y < y2 - 1; y++)
            _SetPixelPos(x1, y, black);
        // right
        for (s32 y = y1; y < y2; y++)
            _SetPixelPos((x2 - 1), y, white);
    
        // top 2
        for (s32 x = x1 + 1; x < x2 - 2; x++)
            _SetPixelPos(x, y1 + 1, outline2);
        // bottom 2
        for (s32 x = x1 + 1; x < x2 - 2; x++)
            _SetPixelPos(x, y2 - 2, outline1);
        // left 2
        for (s32 y = y1 + 2; y < y2 - 2; y++)
            _SetPixelPos((x1 + 1), y, outline2);
        // right 2
        for (s32 y = y1 + 1; y < y2 - 1; y++)
            _SetPixelPos((x2 - 2), y, outline1);
    }
    else{
        // top
        for (s32 x = x1; x < x2; x++)
            _SetPixelPos(x, y1, white);
        // bottom
        for (s32 x = x1; x < x2 - 1; x++)
            _SetPixelPos(x, y2 - 1, black);
        // left
        for (s32 y = y1 + 1; y < y2 - 1; y++)
            _SetPixelPos(x1, y, white);
        // right
        for (s32 y = y1; y < y2; y++)
            _SetPixelPos((x2 - 1), y, black);
    
        // top 2
        for (s32 x = x1 + 1; x < x2 - 2; x++)
            _SetPixelPos(x, y1 + 1, outline1);
        // bottom 2
        for (s32 x = x1 + 1; x < x2 - 2; x++)
            _SetPixelPos(x, y2 - 2, outline2);
        // left 2
        for (s32 y = y1 + 2; y < y2 - 2; y++)
            _SetPixelPos((x1 + 1), y, outline1);
        // right 2
        for (s32 y = y1 + 1; y < y2 - 1; y++)
            _SetPixelPos((x2 - 2), y, outline2);
    }
}
static void _FillRect(s32 x1, s32 y1, s32 x2, s32 y2, u32 colour) { // TODO: use memcpy to copy the first lines instead of calculating every one
    s32 h = y2 - y1;
    u32 uh = (u32)h;
    if (h < 0) return;
    u32 c = RGB888toRGB565(colour);
    for (s32 i = x1; i < x2; i++) {
        _SetLineY(i, y1, uh, c);
    }
}
static void _DrawRect(s32 x1, s32 y1, s32 x2, s32 y2, u32 colour) { // TODO: switch to line rendering
    colour = RGB888toRGB565(colour);
    for (s32 x = x1; x < x2; x++)
        _SetPixelPos(x, y1, colour);
    for (s32 x = x1; x < x2; x++)
        _SetPixelPos(x, y2 - 1, colour);
    for (s32 y = y1 + 1; y < y2 - 1; y++)
        _SetPixelPos(x1, y, colour);
    for (s32 y = y1 + 1; y < y2 - 1; y++)
        _SetPixelPos((x2 - 1), y, colour);
}
static void _DrawPanel(s32 x1, s32 y1, s32 x2, s32 y2, bool down) { // TODO: switch to line rendering

    u32 white = RGB888toRGB565(0xFFFFFF);
    u32 black = RGB888toRGB565(0x808080);
    if (down){
        // top
        for (s32 x = x1; x < x2; x++)
            _SetPixelPos(x, y1, black);
        // bottom
        for (s32 x = x1; x < x2 - 1; x++)
            _SetPixelPos(x, y2 - 1, white);
        // left
        for (s32 y = y1 + 1; y < y2 - 1; y++)
            _SetPixelPos(x1, y, black);
        // right
        for (s32 y = y1; y < y2; y++)
            _SetPixelPos((x2 - 1), y, white);
    }
    else{
        // top
        for (s32 x = x1; x < x2; x++)
            _SetPixelPos(x, y1, white);
        // bottom
        for (s32 x = x1; x < x2 - 1; x++)
            _SetPixelPos(x, y2 - 1, black);
        // left
        for (s32 y = y1 + 1; y < y2 - 1; y++)
            _SetPixelPos(x1, y, white);
        // right
        for (s32 y = y1; y < y2; y++)
            _SetPixelPos((x2 - 1), y, black);
    }
}
static void _DrawUserPanel(s32 x1, s32 y1, s32 x2, s32 y2){ // not drawing an up versions because it looks weird // TODO: switch to line rendering
    u32 white = RGB888toRGB565(0xFFFFFF);
    u32 black = RGB888toRGB565(0x000000);
    u32 outline1 = RGB888toRGB565(0xDFDFDF);
    u32 outline2 = RGB888toRGB565(0x808080);

    // top
    for (s32 x = x1; x < x2; x++)
        _SetPixelPos(x, y1, outline2);
    // bottom
    for (s32 x = x1; x < x2 - 1; x++)
        _SetPixelPos(x, y2 - 1, white);
    // left
    for (s32 y = y1 + 1; y < y2 - 1; y++)
        _SetPixelPos(x1, y, outline2);
    // right
    for (s32 y = y1; y < y2; y++)
        _SetPixelPos((x2 - 1), y, white);

    // top 2
    for (s32 x = x1 + 1; x < x2 - 2; x++)
        _SetPixelPos(x, y1 + 1, black);
    // bottom 2
    for (s32 x = x1 + 1; x < x2 - 2; x++)
        _SetPixelPos(x, y2 - 2, outline1);
    // left 2
    for (s32 y = y1 + 2; y < y2 - 2; y++)
        _SetPixelPos((x1 + 1), y, black);
    // right 2
    for (s32 y = y1 + 1; y < y2 - 1; y++)
        _SetPixelPos((x2 - 2), y, outline1);
}

static void _DrawDebug(){
    u16 colour = RGB888toRGB565(0x007FFF); // greenish blue
    UI_SetScreen(false);
    for (int i = 0; i < BOTTOM_SCREEN_WIDTH; i++)
        _SetPixelPos(i, _UI_TouchCurrent.y, colour);
    _SetLineY(_UI_TouchCurrent.x, 0, BOTTOM_SCREEN_HEIGHT, colour);
}

///
/// UI front end
///

void UI_DrawButton(Button *button){
    _FillRect(button->x + 2, button->y + 2, button->x + button->w - 2, button->y + button->h - 2, button->colour);
    _DrawButton(button->x, button->y, button->x + button->w, button->y + button->h, button->state_pressed);
    s32 totalPadding = ((s32)button->h - SPACING_Y); // not sure what happens if this is negative
    if (button->str != NULL)
        UI_DrawString(button->x + 4 + button->textOffsetX, button->y + (totalPadding / 2) + 1 + button->textOffsetY, button->textColour, button->str);
}
bool UI_UpdateButton(Button *button){
    bool wasPressed = button->state_pressed;

    bool onDown = _UI_PressedButtons & KEY_TOUCH;
    bool isHold = _UI_HeldButtons & KEY_TOUCH;

    s32 x = (s32)_UI_TouchCurrent.x;
    s32 y = (s32)_UI_TouchCurrent.y;

    bool inBounds = UI_InClippingPlane(x, y);

    bool isPressed = inBounds && (x > button->x && x < button->x + button->w) && (y > button->y && y < button->y + button->h) && (onDown || isHold);

    button->state_onDown = onDown && isPressed;
    button->state_onUp = wasPressed && !isHold;
    button->state_pressed = isPressed && isHold;

    if (singleInteraction && wasInteraction) return false;
    if (button->state_onUp) wasInteraction = true;
    return button->state_onUp;
}
bool UI_DoButton(Button *button){
    bool ret = UI_UpdateButton(button);
    UI_DrawButton(button);
    return ret;
}

bool UI_UpdateScrollZone(ScrollZone *scroll){

    bool isDown = _UI_PressedButtons & KEY_TOUCH;
    bool isHold = _UI_HeldButtons & KEY_TOUCH;

    s32 x = (s32)_UI_TouchCurrent.x;
    s32 y = (s32)_UI_TouchCurrent.y;

    bool isPressed = (singleInteraction ? !wasInteraction : true) && (x > scroll->x && x < scroll->x + scroll->w) && (y > scroll->y && y < scroll->y + scroll->h) && isDown;

    bool scrolling = scroll->state_scrolling;

    if (scrolling && !isDown && !isHold){
        scroll->state_scrolling = false;
    } else if (scrolling){
        scroll->scrollX += _UI_TouchDelta.x;
        scroll->scrollY += _UI_TouchDelta.y;
        scroll->state_scrolling = true;
    } else if (!scrolling && isPressed){ // start scrolling
        scroll->state_scrolling = true;
    }

    if (singleInteraction && wasInteraction) return false;
    if (scrolling) wasInteraction = true;
    return scrolling;

}

void UI_DrawCheckBox(CheckBox *checkBox){
    _FillRect(checkBox->x + 2, checkBox->y + 2, checkBox->x + checkBox->size - 2, checkBox->y + checkBox->size - 2, checkBox->colour);
    _DrawUserPanel(checkBox->x, checkBox->y, checkBox->x + checkBox->size, checkBox->y + checkBox->size);
    if (!(checkBox->checked)) return;
    s32 p = (s32)checkBox->size / 2;
    s32 totalPaddingX = (p - (SPACING_X / 2));
    s32 totalPaddingY = (p - (SPACING_Y / 2));
    UI_DrawString((s32)checkBox->x + totalPaddingX + checkBox->charOffsetX, (s32)checkBox->y + totalPaddingY + checkBox->charOffsetY + 1, 0x000000, checkBox->checkChar); // maybe change this to something unique 
}
bool UI_UpdateCheckBox(CheckBox *checkBox){ // TODO: change to on up instead of on down maybe

    bool isDown = _UI_PressedButtons & KEY_TOUCH;

    s32 x = (s32)_UI_TouchCurrent.x;
    s32 y = (s32)_UI_TouchCurrent.y;

    bool inBounds = UI_InClippingPlane(x, y);
    bool isPressed = inBounds && (x > checkBox->x && x < checkBox->x + checkBox->size) && (y > checkBox->y && y < checkBox->y + checkBox->size) && isDown;

    if (isPressed){
        if (!singleInteraction || !wasInteraction){
            checkBox->checked = !checkBox->checked;
        }
    }

    if (singleInteraction && wasInteraction) return false;
    if (isPressed) wasInteraction = true;
    return isPressed;
}
bool UI_DoCheckBox(CheckBox *checkBox){
    bool ret = UI_UpdateCheckBox(checkBox);
    UI_DrawCheckBox(checkBox);
    return ret;
}

#define TITLE_BAR_SIZE (s32)13
void UI_DrawWindow(Window *window){

    UI_FillRect(window->x, window->y, window->w, TITLE_BAR_SIZE, THEME_COLOUR); // title background
    if (!window->minimized){
        UI_FillRect(window->x, (window->y) + TITLE_BAR_SIZE, window->w, window->h - TITLE_BAR_SIZE, BASE_COLOUR); // background
    }
    UI_DrawString(window->x + 2, window->y + 3, 0xFFFFFF, window->title); // title
    UI_DrawButtonOutline(window->x, window->y, window->w, window->minimized ? TITLE_BAR_SIZE + 2 : window->h, false); // main borders

    window->button.textOffsetX = -3;
    UI_DrawButton(&(window->button));

    if (window->OnDraw != NULL){
        window->OnDraw(window);
    }

}
bool UI_UpdateWindow(Window *window){

    if (window->OnUpdate != NULL){
        window->OnUpdate(window);
    }

    window->button.x = window->x + window->w - 3 - window->button.w;
    window->button.y = window->y + 3;
    if (UI_UpdateButton(&(window->button))){
        if (window->button.str[0] == '-'){
            window->minimized = true;
            window->button.str = "+";
        } else {
            window->minimized = false;
            window->button.str = "-";
        }
    }

    bool onDown = UI_GetPressedButtons() & KEY_TOUCH;
    bool isHold = UI_GetHeldButtons() & KEY_TOUCH;

    UTouchPoint touch = UI_GetTouchPos();
    s32 x = (s32)touch.x;
    s32 y = (s32)touch.y;

    bool wasPressed = window->pressed;
    bool inTitleBar = (x > window->x && x < window->x + window->w) && (y > window->y && y < (window->y) + TITLE_BAR_SIZE + 4); // +4 for padding
    bool isPressed =  (window->minimized ? inTitleBar : (x > window->x && x < window->x + window->w) && (y > window->y && y < window->y + window->h)) && (onDown || isHold);

    window->mouseDown = onDown && isPressed;
    window->mouseUp = wasPressed && !isHold;
    window->pressed = isPressed;
    
    if (inTitleBar && onDown){ // on start press
        if (singleInteraction ? !wasInteraction : true)
            window->dragging = true;
    }
    
    if (window->dragging || window->pressed) wasInteraction = true; // if dragging or was pressed

    STouchPoint t = UI_GetTouchDelta();
    if (window->dragging && isHold){
        if (!onDown){ // dont do this on the first frame of being pressed or else the offset could be wrong
            // the cursor mode flips the x direction, probably and issue with my code.
            if (UI_IsCursorMode()){
                window->x += t.x;
            } else{
                window->x -= t.x;
            }
            window->y -= t.y;
        }
    } else {
        window->dragging = false;
    }


    return window->dragging;
}

void _RecalculateTabPanel(TabPanel *tabPanel){
    u32 btnX = tabPanel->x + 2;
    u32 btnY = tabPanel->y + 2;
    u32 btnW = tabPanel->btnWidth;
    u32 btnH = tabPanel->btnHeight;
    u32 selected = tabPanel->selectedTab;
    Button *btns = tabPanel->tabButtons;
    u32 btnCount = (tabPanel->tabCount > MAX_TAB_BUTTON ? MAX_TAB_BUTTON : tabPanel->tabCount);
    for (u32 i = 0; i < btnCount; i++) {
        btns[i].x = btnX;
        btns[i].y = btnY;
        btns[i].w = btnW;
        btns[i].h = btnH;
        if (i == selected){
            btns[i].x = btnX - 2;
            btns[i].y = btnY - 2;
            btns[i].w = btnW + 4;
            btns[i].h = btnH + 4;
        }
        btnX += btnW;
    }
}
void UI_DrawTabPanel(TabPanel *tabPanel){
    u32 X = tabPanel->x;
    u32 Y = tabPanel->y;
    u32 W = tabPanel->w;
    u32 H = tabPanel->h;
    u32 bW = tabPanel->btnWidth;
    u32 bH = tabPanel->btnHeight;

    _RecalculateTabPanel(tabPanel);

    u32 selected = tabPanel->selectedTab;
    Button *btns = tabPanel->tabButtons;
    u32 btnCount = (tabPanel->tabCount > MAX_TAB_BUTTON ? MAX_TAB_BUTTON : tabPanel->tabCount);
    for (u32 i = 0; i < btnCount; i++) {
        if (i != selected){
            UI_DrawButton(&btns[i]);
        }
    }
    UI_DrawButton(&btns[selected]); //  draw last to be over the other buttons

    UI_FillRect(X, Y + bH, W, H - bH, BASE_COLOUR);
    UI_DrawButtonOutline(X, Y + bH, W, H - bH, false);
    UI_FillRect(X + (selected * bW) + 2, Y + bH, bW, 2, BASE_COLOUR);
}
bool UI_UpdateTabPanel(TabPanel *tabPanel){

    _RecalculateTabPanel(tabPanel);

    u32 selected = tabPanel->selectedTab;
    Button *btns = tabPanel->tabButtons;
    u32 btnCount = (tabPanel->tabCount > MAX_TAB_BUTTON ? MAX_TAB_BUTTON : tabPanel->tabCount);
    u32 newSel = selected;
    // update selected first, do other then draw selected last so overlapping things work
    bool ret = false;
    UI_UpdateButton(&btns[selected]); // do nothing if pressed
    for (u32 i = 0; i < btnCount; i++) {
        if (i != selected){
            if (UI_UpdateButton(&btns[i])) {
                newSel = i;
                ret = true;
            }
        }
    }
    tabPanel->selectedTab = newSel;
    return ret;
}
bool UI_DoTabPanel(TabPanel *tabPanel){
    bool ret = UI_UpdateTabPanel(tabPanel);
    UI_DrawTabPanel(tabPanel);
    return ret;
}

bool UI_DrawPixel(s32 x, s32 y, u32 colour){
    if (x < 0 || y < 0) return false; // quick check
    return _SetPixelPos(x, y, RGB888toRGB565(colour));
}   
void UI_DrawButtonOutline(s32 x, s32 y, u32 w, u32 h, bool down){
    _DrawButton(x, y, x + (s32)w, y + (s32)h, down);
}
void UI_DrawPanel(s32 x, s32 y, u32 w, u32 h, bool down){
    _DrawPanel(x, y, x + (s32)w, y + (s32)h, down);
}
void UI_DrawRect(s32 x, s32 y, u32 w, u32 h, u32 colour){
    _DrawRect(x, y, x + (s32)w, y + (s32)h, colour);
}
void UI_FillRect(s32 x, s32 y, u32 w, u32 h, u32 colour){
    _FillRect(x, y, x + (s32)w, y + (s32)h, colour);
}
void UI_DrawCheckBoxPanel(s32 x, s32 y, u32 size){
    _DrawUserPanel(x, y, x + (s32)size, y + (s32)size);
}

bool UI_InClippingPlaneX(s32 x){
    return clippingPlane.x <= x && clippingPlane.endX > x;
}
bool UI_InClippingPlaneY(s32 y){
    return clippingPlane.y <= y && clippingPlane.endY > y;
}
bool UI_InClippingPlane(s32 x, s32 y){
    return (clippingPlane.x <= x && clippingPlane.endX > x) && (clippingPlane.y <= y && clippingPlane.endY > y);
}
void UI_ClearClippingPlane(){
    clippingPlane.x = 0;
    clippingPlane.y = 0;
    clippingPlane.endX = CUR_SCREEN_WIDTH;
    clippingPlane.endY = CUR_SCREEN_HEIGHT;
}
void UI_SetClippingPlane(s32 x, s32 y, u32 w, u32 h){
    clippingPlane.x = x;
    clippingPlane.y = y;
    clippingPlane.endX = x + (s32)w;
    clippingPlane.endY = y + (s32)h;
}
void UI_SetClippingPlaneZ(ScrollZone z) { UI_SetClippingPlane(z.x, z.y, z.w, z.h); }


///
/// menu
///

// i dont like this method but previously it just render an entire menu "inside" another so the stack would keep growing
u32 _DoMenu(MenuResult *res){
    *res = 0;
    if (menuIndex != 0 && menuIndex < MAX_MENU_STACK){
        MenuResult (*foo)() = menuStack[menuIndex];
        if (foo != NULL && foo != 0){
            *res = foo(); // menuIndex may change here
        } else {
            UI_DisplayMessage("Zero func pointer");
            menuIndex--; // go back a menu incase of null func pointer
        }
    }
    return menuIndex; // new menu index
}
bool _CloseMenu(){ // returns if it successfully closed the menu, only ever false if no more menus to close
    if (menuIndex == 0) return false;
    menuIndex--; // can drop to 0, this is fine because in _DoMenu if it returns 0 (the value of menuIndex) it tells the base menu to leave
    return true;
}
bool _OpenMenu(MenuResult (*foo)()){ // returns if it successfully opened the menu, only ever false if no more "room" (custom stack space) to open another menu
    // check if previous menu is the new one, not keeping this incase the user wants nesting menus (i dont know if this code works anyways)
    //if (menuIndex > 1){
    //    if (menuStack[menuIndex - 1] == foo){
    //        _BackMenu();
    //        return true;
    //    }
    //}
    if (menuIndex == MAX_MENU_STACK - 1){
        UI_DisplayMessage("Too many sub menus, you must go back.");
        return false;
    }
    menuStack[++menuIndex] = foo;
    return true;
}

// top screen setup (rosalina only does bottom screen)
// both screens need to be cached or else some programs will break, at last for FTPD if only the left screen is managed it will break rendering and eventually both the top and bottom screens will becomes lines
// the top screen looks bad in 3d mode on the old 3ds, not sure why
// see whatever rosalina does for the bottom screen, i just copied that for the top screen(s)

/*

top screen GPU_FB_TOP_FMT value because i cant find info anyware online
0x10400570 bottom
0x10400470 top

00000000 00001000 00000001 00100001 < 0x80121 ftpd (citro3d rendering)
00000000 00001000 00000011 01000001 < 0x80341 Terraria
00000000 00001000 00000011 00100001 < 0x80321 home screen
                                111 < colour format
			11                      < Rosalina sets this
                            1       < "isNormal2d"
                             1      < "is3d"
????????????  ?????????    ?  ??    < -- unknown
                0       11  1  0    < top, set by https://github.com/derrekr/fastboot3DS/blob/master/source/arm11/hardware/gfx.c
                0       11  0  0    < bottom, set by https://github.com/derrekr/fastboot3DS/blob/master/source/arm11/hardware/gfx.c

*/

static void *framebufferCache_left;
static u32 framebufferCacheSize_left;
static u32 gpuSavedFramebufferAddr1_left, gpuSavedFramebufferAddr2_left;

static void *framebufferCache_right;
static u32 framebufferCacheSize_right;
static u32 gpuSavedFramebufferAddr1_right, gpuSavedFramebufferAddr2_right;

static u32 gpuSavedFramebufferFormat, gpuSavedFramebufferStride, gpuSavedFillColor;

// according to https://www.3dbrew.org/wiki/Memory_layout the vram size is only (w*h*3) but the screen format GSP_RGBA8_OES uses 4 bites so im using 4, not sure whats correct
#define TOP_SCREEN_FULL_SIZE (TOP_SCREEN_WIDTH * TOP_SCREEN_HEIGHT * 4) // 384,000 rounded to 385024 (0x5E000)

#include "draw.h" // include this here because too many things use code from it to recreate things

// may not set to top screen if top screen is unavailable, if it fails drawing will continue on the bottom screen, due to the clipping plane and screen bounds it should still draw without a crash although it will be messed up
void UI_SetScreen(bool top){
    if (top && topScreenAvailable){
        CUR_SCREEN_BUFFER = topScreenBuffer;
        CUR_SCREEN_WIDTH = TOP_SCREEN_WIDTH;
        CUR_SCREEN_HEIGHT = TOP_SCREEN_HEIGHT;
    } else {
        CUR_SCREEN_BUFFER = bottomScreenBuffer;
        CUR_SCREEN_WIDTH = BOTTOM_SCREEN_WIDTH;
        CUR_SCREEN_HEIGHT = BOTTOM_SCREEN_HEIGHT;
    }
    UI_ClearClippingPlane();
}
void _UI_FlushTopScreen(){ // TODO: make 2 different rendering planes for 3D stuff, 3D cant change while using rosalina so it might not be a good idea
    if (!frameStarted || !topScreenAvailable) return;
    memcpy(TOP_LEFT_VRAM_ADDR, topScreenBuffer, TOP_SCREEN_SIZE);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)TOP_LEFT_VRAM_ADDR, TOP_SCREEN_SIZE);
    // if ((gpuSavedFramebufferFormat & (1 << 6)) == 0) // check if in 2d is probably not needed since we fix the buffer at the end
    memcpy(TOP_RIGHT_VRAM_ADDR, topScreenBuffer, TOP_SCREEN_SIZE);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)TOP_RIGHT_VRAM_ADDR, TOP_SCREEN_SIZE);
}
void _UI_FlushBottomScreen(){
    if (!frameStarted) return;
    memcpy(BOTTOM_VRAM_ADDR, bottomScreenBuffer, BOTTOM_SCREEN_SIZE);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)BOTTOM_VRAM_ADDR, BOTTOM_SCREEN_SIZE); // flush; renders screen
}
bool _UI_IsTopScreenAvailable(){
    return topScreenAvailable;
}

FramebufferFormat _UI_GetTopScreenFormat(){
    FramebufferFormat fmt = { 
        .format = gpuSavedFramebufferFormat & 7,
        .stride = gpuSavedFramebufferStride,
        .isNormal2d = (gpuSavedFramebufferFormat & (1 << 6)) != 0, // from Draw_GetCurrentScreenInfo in draw.c
        .is3d = (gpuSavedFramebufferFormat & (1 << 5)) != 0, // from Draw_GetCurrentScreenInfo in draw.c
        .raw = gpuSavedFramebufferFormat
    };
    return fmt;
}

// converts the screen to RGB565 so colour detail is lost
void _UI_CopyTopScreen(){ // probably slow as christmas
    if (!topScreenAvailable) return;
    if (topScreenCopyBuffer_left == NULL){
        u32 padding = 5;
        UI_DrawString(padding, padding, 0xFF0000, "Err ScreenBuffer NULL");
        return;
    }

    u16 *copyBuffer = topScreenBuffer;
    u8 *originalBuffer = topScreenCopyBuffer_left; // default to left i guess

    u32 pixelType = gpuSavedFramebufferFormat & 7; // 0000-0111

    u32 SIZE = TOP_SCREEN_WIDTH * TOP_SCREEN_HEIGHT;
    u32 c = 0x00000000;

    // using a "switch" outside the loop so we dont recompute the same thing a billion times
    switch (pixelType) {
    case GSP_RGBA8_OES: { // size 4
        // not sure what alpha is for so im just doing the colours
        for (u32 i = 0; i < SIZE; i++) {
            c = 0;
            c += (*(originalBuffer++)) << 16;   // B
            c += (*(originalBuffer++)) << 8;    // G
            c += (*(originalBuffer++));         // R
            originalBuffer++; // skip alpha
            (*(copyBuffer++)) = (u16)RGB888toRGB565(c);
        }
    } break;
    case GSP_BGR8_OES: {// size 3
        for (u32 i = 0; i < SIZE; i++) {
            c = 0;
            c += (*(originalBuffer++));         // B
            c += (*(originalBuffer++)) << 8;    // G
            c += (*(originalBuffer++)) << 16;   // R
            (*(copyBuffer++)) = (u16)RGB888toRGB565(c);
        }
    } break;
    case GSP_RGB565_OES: { // size 2
        // we are already converting into this type so nothing is done, technically a memcpy would suffice
        for (u32 i = 0; i < SIZE; i++) {
            c = 0;
            c += (*(originalBuffer++));         // B
            c += (*(originalBuffer++)) << 8;    // G
            (*(copyBuffer++)) = (u16)c;
        }
    } break;
    case GSP_RGB5_A1_OES: // size 2
        // im too lazy
    break;
    case GSP_RGBA4_OES: // size 2
        // im too lazy
    break;
    }
}

#define KERNPA2VA(a) ((a) + (GET_VERSION_MINOR(osGetKernelVersion()) < 44 ? 0xD0000000 : 0xC0000000)) // from draw.c
typedef struct FrameBufferArgs {
    u32 size;
} FrameBufferArgs;
static void _CopyTopBuffer_Kernal(FrameBufferArgs *args){
    u8* addr = (u8*)KERNPA2VA(Draw_GetCurrentFramebufferAddress(true, true));
    memcpy(topScreenCopyBuffer_left, addr, args->size);
}
static void _CopyTopBuffer(u32 size){
    if (topScreenCopyBuffer_left == NULL) return;
    FrameBufferArgs args = { size };
    svcCustomBackdoor(_CopyTopBuffer_Kernal, &args);
}

void _UI_InitTopScreen(){ // Draw_SetupFramebuffer
    if (topScreenAvailable) return;

    while((GPU_PSC0_CNT | GPU_PSC1_CNT | GPU_TRANSFER_CNT | GPU_CMDLIST_CNT) & 1); // rosalina has this so im assuming its usefull

    // not sure if this is a bad idea, it seems to work fine, ive never had issues with it
    u32 addr = 0x0D800000; // 0x800000 more then rosalina, hopefully thats fine (i do not remember how or why i chose this value)
    u32 tmp;
    u32 size = TOP_SCREEN_FULL_SIZE;
    size = (size + 0xFFF) >> 12 << 12;

    topScreenAvailable = false;

    framebufferCache_left = NULL;
    framebufferCacheSize_left = 0;
    framebufferCache_right = NULL;
    framebufferCacheSize_right = 0;
    
    Result res = svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_ALLOC | MEMOP_REGION_SYSTEM, MEMPERM_READWRITE, true);
    if (R_FAILED(res)) { return;
    } else {
        framebufferCache_left = (void*)addr;
        framebufferCacheSize_left = size;
    }
    addr += size; // hopefully this is also fine
    res = svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_ALLOC | MEMOP_REGION_SYSTEM, MEMPERM_READWRITE, true);
    if (R_FAILED(res)) { 
        if (framebufferCache_left != NULL)
            svcControlMemory(&tmp, (u32)framebufferCache_left, 0, size, MEMOP_FREE, 0);
        framebufferCacheSize_left = 0;
        framebufferCache_left = NULL;
        return;
    } else {
        framebufferCache_right = (void*)addr;
        framebufferCacheSize_right = size;
    }
    addr += size; // hopefully this too is also fine
    topScreenAvailable = true;
    // not a big deal if this fails
    res = svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_ALLOC | MEMOP_REGION_SYSTEM, MEMPERM_READWRITE, true);
    if (R_FAILED(res)) {
        topScreenCopyBuffer_left = NULL;
    } else {
        topScreenCopyBuffer_left = (void*)addr;
    }

    u32 topSize = GPU_FB_TOP_STRIDE * TOP_SCREEN_WIDTH;
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)TOP_LEFT_VRAM_ADDR, topSize);
    memcpy(framebufferCache_left, TOP_LEFT_VRAM_ADDR, topSize);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)TOP_RIGHT_VRAM_ADDR, topSize);
    memcpy(framebufferCache_right, TOP_RIGHT_VRAM_ADDR, topSize);

    // getting a more direct version of the screen incase its not located at TOP_LEFT_VRAM_ADDR
    static const u32 pixelSize[] = { 4, 3, 2, 2, 2 };
    _CopyTopBuffer(TOP_SCREEN_WIDTH * TOP_SCREEN_HEIGHT * pixelSize[GPU_FB_TOP_FMT & 7]); // uses topScreenCopyBuffer_left

    u32 format = GPU_FB_TOP_FMT;
    gpuSavedFramebufferAddr1_left = GPU_FB_TOP_LEFT_ADDR_1;
    gpuSavedFramebufferAddr2_left = GPU_FB_TOP_LEFT_ADDR_2;
    GPU_FB_TOP_LEFT_ADDR_1 = GPU_FB_TOP_LEFT_ADDR_2 = TOP_LEFT_VRAM_ADDR_PA;

    gpuSavedFramebufferAddr1_right = GPU_FB_TOP_RIGHT_ADDR_1;
    gpuSavedFramebufferAddr2_right = GPU_FB_TOP_RIGHT_ADDR_2;
    GPU_FB_TOP_RIGHT_ADDR_1 = GPU_FB_TOP_RIGHT_ADDR_2 = TOP_RIGHT_VRAM_ADDR_PA;
    
    gpuSavedFramebufferFormat = format;
    gpuSavedFramebufferStride = GPU_FB_TOP_STRIDE;
    format = (format & ~7) | GSP_RGB565_OES;
    format |= 3 << 8; // not sure what these bit flips do the original code doesn't say "set VRAM bits"
    GPU_FB_TOP_FMT = format;
    GPU_FB_TOP_STRIDE = TOP_SCREEN_HEIGHT * 2; 
    gpuSavedFillColor = LCD_TOP_FILLCOLOR;
    LCD_TOP_FILLCOLOR = 0;
}
void _UI_DeinitTopScreen(){ // Draw_RestoreFramebuffer

    if (!topScreenAvailable) return;
    
    u32 topSize = gpuSavedFramebufferStride * TOP_SCREEN_WIDTH;
    memcpy(TOP_LEFT_VRAM_ADDR, framebufferCache_left, topSize);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)TOP_LEFT_VRAM_ADDR, topSize); 
    memcpy(TOP_RIGHT_VRAM_ADDR, framebufferCache_right, topSize);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)TOP_RIGHT_VRAM_ADDR, topSize); 

    LCD_TOP_FILLCOLOR = gpuSavedFillColor;
    GPU_FB_TOP_STRIDE = gpuSavedFramebufferStride;
    GPU_FB_TOP_FMT = gpuSavedFramebufferFormat;
    GPU_FB_TOP_LEFT_ADDR_1 = gpuSavedFramebufferAddr1_left;
    GPU_FB_TOP_LEFT_ADDR_2 = gpuSavedFramebufferAddr2_left;
    GPU_FB_TOP_RIGHT_ADDR_1 = gpuSavedFramebufferAddr1_right;
    GPU_FB_TOP_RIGHT_ADDR_2 = gpuSavedFramebufferAddr2_right;

    u32 tmp;

    u32 size = TOP_SCREEN_FULL_SIZE;
    size = (size + 0xFFF) >> 12 << 12;

    if (framebufferCache_right != NULL) svcControlMemory(&tmp, (u32)framebufferCache_right, 0, size, MEMOP_FREE, 0);
    framebufferCacheSize_right = 0;
    framebufferCache_right = NULL;

    if (framebufferCache_left != NULL) svcControlMemory(&tmp, (u32)framebufferCache_left, 0, size, MEMOP_FREE, 0);
    framebufferCacheSize_left = 0;
    framebufferCache_left = NULL;

    if (topScreenCopyBuffer_left != NULL) svcControlMemory(&tmp, (u32)topScreenCopyBuffer_left, 0, size, MEMOP_FREE, 0);
    topScreenCopyBuffer_left = NULL;

    topScreenAvailable = false;
}

///
/// generate stuff
///

bool cursorMode = false;

bool UI_IsCursorMode() { return cursorMode; }
u32 UI_GetPressedButtons() { return _UI_PressedButtons; }
u32 UI_GetHeldButtons() { return _UI_HeldButtons; }
UTouchPoint UI_GetTouchPos() { return _UI_TouchCurrent; }
STouchPoint UI_GetCursorPos() { return _UI_CursorCurrent; }
STouchPoint UI_GetTouchDelta() { return _UI_TouchDelta; }
STouchPoint UI_GetCursorDelta() { return _UI_CursorDelta; }


void UI_FlushScreens(){
    _UI_FlushBottomScreen();
    _UI_FlushTopScreen();
}
u64 UI_FrameTime(){
    return svcGetSystemTick() - startTime;
}

#include <3ds/services/hid.h>
void UI_UpdatePressedInput(){
    hidScanInput();
    _UI_PressedButtons = hidKeysDown();
}
void UI_StartFrame(){
    if (frameStarted) return;
    
    singleInteraction = true;
    wasInteraction = false;

    UI_SetScreen(false); // calls _UI_ClearClippingPlane();
    frameStarted = true;
    startTime = svcGetSystemTick();
    hidScanInput();
    _UI_PressedButtons = hidKeysDown();
    _UI_HeldButtons = hidKeysHeld();

    _UI_TouchDelta.x = _UI_TouchCurrent.x;
    _UI_TouchDelta.y = _UI_TouchCurrent.y;

    hidTouchRead((touchPosition*)&_UI_TouchCurrent);

    _UI_TouchDelta.x -= _UI_TouchCurrent.x;
    _UI_TouchDelta.y -= _UI_TouchCurrent.y;

    
    float cmult = 0.02;
    float cthreash = 32;
    circlePosition cp;
    hidCircleRead(&cp);
    _UI_CursorDelta.x = cp.dx;
    _UI_CursorDelta.y = cp.dy;
    float cpxr = (float)(s32) (_UI_CursorDelta.x < -cthreash || _UI_CursorDelta.x > cthreash ? _UI_CursorDelta.x : 0);
    float cpyr = (float)(s32)-(_UI_CursorDelta.y < -cthreash || _UI_CursorDelta.y > cthreash ? _UI_CursorDelta.y : 0);
    _UI_CursorCurrent.x += (s32)(cpxr * cmult);
    _UI_CursorCurrent.y += (s32)(cpyr * cmult);
    if (_UI_CursorCurrent.x < 0) _UI_CursorCurrent.x = 0;
    if (_UI_CursorCurrent.y < 0) _UI_CursorCurrent.y = 0;
    if (_UI_CursorCurrent.x > BOTTOM_SCREEN_WIDTH) _UI_CursorCurrent.x = BOTTOM_SCREEN_WIDTH - 1;
    if (_UI_CursorCurrent.y > BOTTOM_SCREEN_HEIGHT) _UI_CursorCurrent.y = BOTTOM_SCREEN_HEIGHT - 1;

    if (_UI_PressedButtons & KEY_SELECT) {
        cursorMode = !cursorMode;
        _UI_CursorCurrent.x = BOTTOM_SCREEN_WIDTH / 2;
        _UI_CursorCurrent.y = BOTTOM_SCREEN_HEIGHT / 2;
    }
    if (cursorMode){
        _UI_TouchDelta.x = (s32) (cpxr * cmult);
        _UI_TouchDelta.y = (s32)-(cpyr * cmult); // un negative it
        
        _UI_TouchCurrent.x = _UI_CursorCurrent.x;
        _UI_TouchCurrent.y = _UI_CursorCurrent.y;

        _UI_PressedButtons &= ~KEY_TOUCH; // remove pressed
        if (_UI_PressedButtons & KEY_A){
            _UI_PressedButtons |= KEY_TOUCH; // un-remove pressed if A pressed
        }

        _UI_HeldButtons &= ~KEY_TOUCH; // remove held
        if (_UI_HeldButtons & KEY_A){
            _UI_HeldButtons |= KEY_TOUCH; // un-remove held if A held
        }
    }

    Draw_Lock();
}
void UI_EndFrame(){
    if (!frameStarted) return;
    if (DEBUG_MODE) _DrawDebug();
    if (cursorMode){
        u16 colour = RGB888toRGB565(0x00FF00); // green
        UI_SetScreen(false);
        for (int i = 0; i < BOTTOM_SCREEN_WIDTH; i++)
            _SetPixelPos(i, _UI_CursorCurrent.y, colour);
        _SetLineY(_UI_CursorCurrent.x, 0, BOTTOM_SCREEN_HEIGHT, colour);
    }
    UI_FlushScreens();
    Draw_Unlock();
    frameStarted = false;
    u64 tickTime = UI_FrameTime();
    u64 dtNs = tickTime * SECOND_IN_NANOSECOND / TICKS_PER_SECOND;
    if (dtNs < TARGET_NANOSECOND)
        svcSleepThread(TARGET_NANOSECOND - dtNs);
}

///
/// Creators
///

Button UI_CreateButton(s32 x, s32 y, u32 w, u32 h, char *string, u32 colour, u32 textColour){
    Button btn = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .textOffsetX = 0,
        .textOffsetY = 0,
        .str = string,
        .colour = colour,
        .textColour = textColour,
        .rawState = 0
    }; 
    return btn;
}
ScrollZone UI_CreateScrollZone(s32 x, s32 y, u32 w, u32 h){
    ScrollZone zone = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .scrollX = 0,
        .scrollY = 0,
        .rawState = 0
    };
    return zone;
}
CheckBox UI_CreateCheckBox(s32 x, s32 y, u32 size, char *checkChar, u32 textColour, u32 colour, bool checked){
    CheckBox check = {
        .x = x,
        .y = y,
        .size = size,
        .colour = colour,
        .textColour = textColour,
        .checked = checked,
        .charOffsetX = 0,
        .charOffsetY = 0,
        .checkChar = checkChar
    };
    return check;
}
Window UI_CreateWindow(s32 x, s32 y, u32 w, u32 h, char* title){
    Window win = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .title = title,
        .rawState = 0,
        .active = false,
        .OnDraw = NULL,
        .OnUpdate = NULL
    };    
    return win;
}
TabPanel UI_CreateTabPanel(s32 x, s32 y, u32 w, u32 h, u32 btnW, u32 btnH, char **titles, u32 tabCount){
    TabPanel tab = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .btnHeight = btnH,
        .btnWidth = btnW,
        .selectedTab = 0,
        .tabCount = tabCount
    };
    u32 realTabCount = (tab.tabCount > MAX_TAB_BUTTON ? MAX_TAB_BUTTON : tab.tabCount);
    for (u32 i = 0; i < realTabCount; i++) {
        tab.tabButtons[i] = UI_CreateButton(0, 0, 0, 0, titles[i], BASE_COLOUR, 0x000000);
    }
    
    return tab;
}

///
/// Other Functions
///


bool UI_DisplayMessageFormat_Args(char* str, va_list arg){

    memset(errorBuffer, 0, sizeof(errorBuffer)); // clear buffer
    vsprintf(errorBuffer, str, arg);
    
    u32 strW = 0;
    u32 strH = 0;
    UI_MeasureString(&strW, &strH, errorBuffer);

    bool aPressed = false;

    if (frameStarted){ // if already rendering: draw stuff and flush screen then pause until input
        PlaneRect clipPlane = clippingPlane;
        UI_ClearClippingPlane();
        UI_FillRect(0, 0, strW + 2, strH + 2, 0);
        UI_DrawString(2, 2, 0xFF0000, errorBuffer);
        UI_FlushScreens();
        u32 input;
        while (true){
            hidScanInput();
            input = hidKeysDown();
            if (input & KEY_B) { aPressed = false; break; }
            if (input & KEY_A) { aPressed = true; break; }
            svcSleepThread(16666666LL); // wait fixed 16 milliseconds because its very unlikely the previous few lines are going to take too long to run
        }
        clippingPlane = clipPlane;
    } else { // if not already rendering: start like normal
        u32 pressed = 0;
        MENU_LOOP_START(); {
            pressed = UI_GetPressedButtons();
            if (pressed & KEY_B) { aPressed = false; break; }
            if (pressed & KEY_A) { aPressed = true; break; }
            UI_FillRect(0, 0, strW + 2, strH + 2, 0);
            UI_DrawString(2, 2, 0xFF0000, errorBuffer);
        } MENU_LOOP_END();
    }
    return aPressed;
}
// returns true if A was pressed or false if B was pressed
bool UI_DisplayMessageFormat(char* str, ...){
    va_list arg; 
    va_start(arg, str);
    bool ret = UI_DisplayMessageFormat_Args(str, arg);
    va_end(arg);
    return ret;
}
// returns true if A was pressed or false if B was pressed
bool UI_DisplayMessage(char* str){
    return UI_DisplayMessageFormat(str);
}
// max input length is subtracted by 1 so if your buffer is 64 chars long you CAN use 64 as the max input
bool UI_GetUserInput(char* message, u32 maxInputLength, char *ouputBuffer) {
    if (!frameStarted) return false;

    u32 btnSize = 18;
    u32 padding = 2;

    const u32 lineButtonCount[4] = { 13, 12, 11, 10 };
    const char lowerChars[4][13] = {
        "`1234567890-=",
        "qwertyuiop[]?",
        "asdfghjkl;'??",
        "zxcvbnm,./???"
    };
    const char upperChars[4][13] = {
        "~!@#$%^&*()_+",
        "QWERTYUIOP{}?",
        "ASDFGHJKL:\"??",
        "ZXCVBNM<>????"
    };
    const u32 offsets[4] = { 0, btnSize + padding + (btnSize / 2), btnSize / 4, btnSize / 2 };
    u32 lineEndX[4] = { 0, 0, 0, 0 };
    u32 totalWidth = (((btnSize + padding) * 13) - padding) + ((btnSize * 2) + (2 * padding));
    u32 messageSizeY = 0;
    UI_MeasureString(NULL, &messageSizeY, message);
    u32 baseButtonY = PADDING + (messageSizeY) + PADDING + 20 + PADDING; // 1 line of text, the text box area and badding
    u32 baseButtonX = (BOTTOM_SCREEN_WIDTH / 2) - (totalWidth / 2);
    u32 buttonCount = 13 + 12 + 11 + 10;
    char str[buttonCount][2];
    Button buttons[buttonCount];

    u32 btnIndex = 0;
    u32 btnX = baseButtonX;
    u32 btnY = baseButtonY;
    for (u32 i = 0; i < 4; i++) {
        btnX += offsets[i];
        for (u32 j = 0; j < lineButtonCount[i]; j++) {
            str[btnIndex][0] = lowerChars[i][j];
            str[btnIndex][1] = 0;
            buttons[btnIndex] = UI_CreateButton(btnX + (j * (btnSize + padding)), btnY, btnSize, btnSize, str[btnIndex], BASE_COLOUR, 0x000000);
            buttons[btnIndex].textOffsetX + 1; // makes it centered
            lineEndX[i] = buttons[btnIndex].x + buttons[btnIndex].w;
            btnIndex++;
        }
        btnY += padding + btnSize;
    }
    
    UI_EndFrame(); // stop active frame
    PlaneRect clipPlane = clippingPlane;
    UI_ClearClippingPlane();

    Button back = UI_CreateButton(PADDING     , BOTTOM_SCREEN_HEIGHT - PADDING - 20, 75, 20, "Chancel", BASE_COLOUR, 0x000000);
    Button okBtn = UI_CreateButton(BOTTOM_SCREEN_WIDTH - PADDING - 75, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 75, 20, "Ok", BASE_COLOUR, 0x000000);

    Button tabBtn = UI_CreateButton(baseButtonX, baseButtonY + (padding + btnSize), offsets[1] - padding, btnSize, "tab", BASE_COLOUR, 0x000000);
    tabBtn.textOffsetX = -2;
    Button capsBtn = UI_CreateButton(baseButtonX, baseButtonY + ((padding + btnSize) * 2), offsets[1] - padding + offsets[2], btnSize, "caps", BASE_COLOUR, 0x000000);
    capsBtn.textOffsetX = -2;
    Button backspaceBtn = UI_CreateButton(lineEndX[0] + padding, baseButtonY, totalWidth - lineEndX[0], btnSize, "<--", BASE_COLOUR, 0x000000);
    Button spaceBtn = UI_CreateButton((offsets[0] + offsets[1] + offsets[2] + offsets[3]) + btnSize + padding + btnSize - padding, btnY, (btnSize + padding) * 6, btnSize, "Space", BASE_COLOUR, 0x000000);
    Button backslashBtn = UI_CreateButton(lineEndX[1] + padding, tabBtn.y, totalWidth - lineEndX[1], btnSize, "\\", BASE_COLOUR, 0x000000);

    bool caps = false;

    s32 inputIndex = strlen(ouputBuffer);
    bool validInput = false;
    u32 caretTimer = 0;
    u32 caretMax = 30;

    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DO_BUTTON(back) { break; }
        MENU_DO_BUTTON(okBtn) { validInput = true; break; }

        #define CAP_USER_INPUT if (inputIndex >= (s32)maxInputLength - 2) { inputIndex = (s32)maxInputLength - 2; }

        for (u32 i = 0; i < buttonCount; i++) {
            MENU_DO_BUTTON(buttons[i]){
                ouputBuffer[inputIndex++] = buttons[i].str[0];
                CAP_USER_INPUT
            }
        }

        MENU_DO_BUTTON(capsBtn){
            caps = !caps;
            btnIndex = 0;
            for (u32 i = 0; i < 4; i++) {
                for (u32 j = 0; j < lineButtonCount[i]; j++) {
                    buttons[btnIndex].str[0] = caps ? upperChars[i][j] : lowerChars[i][j];
                    btnIndex++;
                }
            }
        }
        MENU_DO_BUTTON(tabBtn){
            ouputBuffer[inputIndex++] = '\t'; // tab is printed as 2 spaces 
            CAP_USER_INPUT
        }
        MENU_DO_BUTTON(backspaceBtn){
            if (--inputIndex < 0){
                inputIndex = 0;
            }
            ouputBuffer[inputIndex] = 0;
        }
        MENU_DO_BUTTON(spaceBtn){
            ouputBuffer[inputIndex++] = ' ';
            CAP_USER_INPUT
        }
        MENU_DO_BUTTON(backslashBtn){
            ouputBuffer[inputIndex++] = '\\';
            CAP_USER_INPUT
        }

        UI_DrawString(PADDING, PADDING, 0x000000, message);

        u32 stringX = PADDING;
        u32 stringY = messageSizeY + PADDING;
        UI_DrawPanel(stringX, stringY, BOTTOM_SCREEN_WIDTH - (2 * PADDING), 20, BASE_COLOUR);
        UI_DrawString(stringX + PADDING, stringY + PADDING, 0x000000, ouputBuffer);
        u32 w = 0;
        u32 h = 0;
        UI_MeasureString(&w, &h, ouputBuffer);

        if (caretTimer++ > caretMax)
            for (u32 i = 0; i < SPACING_Y; i++) {
                UI_DrawPixel(stringX + PADDING + w, stringY + PADDING + i - 1, 0x000000);
                UI_DrawPixel(stringX + PADDING + w - 1, stringY + PADDING + i - 1, 0x000000);
            }
        if (caretTimer++ > (caretMax * 2)) caretTimer = 0;
        

    } MENU_LOOP_END();

    UI_StartFrame(NULL, NULL, NULL); // restart frame
    
    clippingPlane = clipPlane;
    return validInput;
}

// message (including args) must not exceed 1024
bool UI_CheckWithUserFormat(char *buttonNo, char *buttonYes, char* message, ...) {
    static Button yesBtn = { 0 };
    static Button back = { 0 };
    static char largeTextBuffer[LARGE_TEXT_BUFFER_SIZE];
    va_list arg; 
    va_start(arg, message);
    vsprintf(largeTextBuffer, message, arg);

    if (!frameStarted) return false;
    UI_EndFrame(); // stop active frame
    PlaneRect clipPlane = clippingPlane;
    UI_ClearClippingPlane();
    if (buttonNo != NULL){
        u32 str1 = strlen(buttonNo) * SPACING_Y + 8;
        back = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, str1, 20, buttonNo, BASE_COLOUR, 0x000000);
    } else {
        u32 str1 = strlen("Back") * SPACING_Y + 8;
        back = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, str1, 20, "Back", BASE_COLOUR, 0x000000);
    }
    if (buttonYes != NULL){
        u32 str2 = strlen(buttonYes) * SPACING_Y + 8;
        yesBtn = UI_CreateButton(BOTTOM_SCREEN_WIDTH - PADDING - str2, BOTTOM_SCREEN_HEIGHT - PADDING - 20, str2, 20, buttonYes, BASE_COLOUR, 0x000000);
    }
    bool yes = false;
    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DO_BUTTON(back) { yes = false; break; }
        if (buttonYes != NULL) MENU_DO_BUTTON(yesBtn) { yes = true; break; }
        UI_DrawString(PADDING, PADDING, 0x000000, largeTextBuffer);
    } MENU_LOOP_END();
    UI_StartFrame(NULL, NULL, NULL); // restart frame
    clippingPlane = clipPlane;

    va_end(arg);
    return yes;
}
bool UI_CheckWithUser(char *buttonNo, char *buttonYes, char* message) {
    return UI_CheckWithUserFormat(buttonNo, buttonYes, message);
}

