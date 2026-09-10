#include <3ds.h>
#include <3ds/services/apt.h> // for APT_HardwareResetAsync()
#include <csvc.h>

#include "font.h" // for font[]
#include "utils.h" // for isServiceUsable()

#include <stdio.h> // for sprintf
#include <string.h> // for memset

#include "ctn/newUI.h"
#include "ctn/ctn.h"
#include "ctn/network.h"


bool rosalinaMod_ShouldOpenCustomMenu = false;
bool StartOpenDebuggerMenu = false;
bool isctnMenuOpen = false;
bool DEBUG_MODE = false;

extern MenuResult ctn_Menu_ProcList();

void toBinary(u64 num, int count, char* buffer){
    for (int i = 0; i < count; i++) {
        buffer[count - i - 1] = (num & (1 << i)) ? '1' : '0';
    }
}
void u32ToBinary(u32 num, char *buffer) { toBinary(num, 32, buffer); }
void u8ToBinary(u8 num, char *buffer) { toBinary(num, 8, buffer); }

// may not draw the top screen if its not available
static bool topScreenDirty = true;
static bool wasDebug = false;
void DrawDefaultUpperScreenStuff(){

    if (!_UI_IsTopScreenAvailable()) return; // dont draw incase no top screen, if it continued to draw it would just draw on the bottom screen

    UI_SetScreen(true);

    // dont redraw to save CPU usage, and since we are not chaning it anyways
    if (topScreenDirty){
        _UI_CopyTopScreen();
        //_UI_DrawString(PADDING, PADDING, 0xFF0000, "Drew Screen");
        topScreenDirty = false;
    }

    //_UI_FillRect(0, 0, TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT, BASE_COLOUR);
    UI_FillRect(PADDING, TOP_SCREEN_HEIGHT - PADDING - 20, TOP_SCREEN_WIDTH - (2 * PADDING), 20, BASE_COLOUR);
    UI_DrawPanel(PADDING, TOP_SCREEN_HEIGHT - PADDING - 20, TOP_SCREEN_WIDTH - (2 * PADDING), 20, true);
    
    s32 stringX = 2 * PADDING;
    s32 stringY = TOP_SCREEN_HEIGHT - PADDING - 20 + PADDING;

    UI_DrawString(stringX, stringY, 0x000000, "Ver " PROGRAM_VERSION);
    stringX += (SPACING_X * 11);

    // TODO: display time maybe 
    // either use whats in ntp.c or figure out how to use this (not sure if its the correct method) https://www.3dbrew.org/wiki/PTM_Services#GetSystemTime_PTM_Service_%22ptm:gets%22
    // actually this can be done through ptmgets.h i think

    // get battery stuff
    bool batteryDetails = false;
    int charging = 0; // 0 = err, 1 = true, 2 = false
    u8 batteryTemp = 0;
    float batteryCharge = 0;
    float batteryVoltage = 0;
    // from rosalina's implementation in menuUpdateMcuInfo() in menu.c
    u8 data[4];
    if (!isServiceUsable("mcu::HWC")){
        goto endBatteryDetails;
    }
    Handle *mcuHwcHandlePtr = mcuHwcGetSessionHandle();
    *mcuHwcHandlePtr = 0;
    if (R_FAILED(srvGetServiceHandle(mcuHwcHandlePtr, "mcu::HWC"))){
        if (R_FAILED(svcControlService(SERVICEOP_STEAL_CLIENT_SESSION, mcuHwcHandlePtr, "mcu::HWC"))){
            goto endBatteryDetails;
        }
    }
    
    if (R_SUCCEEDED(MCUHWC_ReadRegister(0xA, data, 4))){
        batteryTemp = data[0];
        batteryCharge = data[1] + data[2] / 256.0f;
        batteryCharge = (u32)((batteryCharge + 0.05f) * 10.0f) / 10.0f;
        batteryVoltage = 0.02f * data[3];
        batteryVoltage = (u32)((batteryVoltage + 0.005f) * 100.0f) / 100.0f;
        batteryDetails = true;
    }
    
    // custom code here for charging details

    // this needs to be fixed (1 << 4) seems only to be true if its charging and not at 100%
    // while viewing the bits (1 << 1) also changed when unplugging and plugging it back in but it seems to always be true in practice
    // alternatively use https://www.3dbrew.org/wiki/PTM:GetBatteryChargeState
    if (R_SUCCEEDED(MCUHWC_ReadRegister(0x0F, data, 1))) { // https://www.3dbrew.org/wiki/I2C_Registers#:~:text=BatteryChargeState
        // not charging 01010101, charging 01011111 (this may be wrong)
        if (data[0] & (1 << 4)){
            charging = 1;
        } else {
            charging = 2;
        }
    }

    svcCloseHandle(*mcuHwcHandlePtr); // idk what happens if this fails
    endBatteryDetails:

    char buffer[64]; // 64 for good measure
    if (batteryDetails){
        memset(buffer, 0, sizeof(buffer));
        u32 chargeInt = (u32)batteryCharge;
        u32 chargeFrac = (u32)(batteryCharge * 10.0f) % 10u;
        if (charging > 0){
            sprintf(buffer, "[g/\x03]%c%ld.%ld%%", (charging == 1 ? '+' : '-'), chargeInt, chargeFrac);
        } else sprintf(buffer, "[g/\x03]%ld.%ld%%", chargeInt, chargeFrac);
        UI_DrawString(stringX, stringY, 0x000000, buffer);
        
        // battery using rosalina title colour
        u32 batLevel = (u32)(6 * (chargeInt / (float)100));
        UI_FillRect(stringX + 1, stringY + SPACING_Y - 3 - batLevel, 4, batLevel, 0x21C2FF); // more optimized then _UI_DrawPixel()

        stringX += (SPACING_X * (strlen(buffer) + 1 - 4)); // subtract a few for the formating char

        memset(buffer, 0, sizeof(buffer));
        u32 voltageInt = (u32)batteryVoltage;
        u32 voltageFrac = (u32)(batteryVoltage * 100.0f) % 100u;
        sprintf(buffer, "%ld.%ldV", voltageInt, voltageFrac);
        UI_DrawString(stringX, stringY, 0x000000, buffer);
        stringX += (SPACING_X * (strlen(buffer) + 1));
        
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "[g/\x04]%02hhu""\xF8""C", batteryTemp); // \xF8, 248 - degree symbol https://www.lookuptables.com/text/extended-ascii-table
        UI_DrawString(stringX, stringY, 0x000000, buffer);

        // fill some of the thermometer
        u32 posX = stringX + 1;
        u32 posY = stringY + SPACING_Y - 5;
        UI_DrawPixel(posX++, posY, 0xAE0000);
        UI_DrawPixel(posX, posY + 1, 0xAE0000);
        UI_DrawPixel(posX++, posY, 0xAE0000);
        UI_DrawPixel(posX, posY + 1, 0xAE0000);
        UI_DrawPixel(posX++, posY, 0xAE0000);
        UI_DrawPixel(posX++, posY, 0xAE0000);

        stringX += (SPACING_X * (strlen(buffer) + 1 - 4)); // subtract a few for the formating char
    } else {
        UI_DrawString(stringX, stringY, 0xFF0000, "[g/\x03] Err");
        stringX += (SPACING_X * 10);
    }
    
    s64 ramUsage = 0;
    if (R_SUCCEEDED(svcGetSystemInfo(&ramUsage, 0, 0))){
        u32 totalMem = osGetMemRegionSize(MEMREGION_ALL);
        memset(buffer, 0, sizeof(buffer));
        u64 usage = ramUsage / 1024 / 1024;
        u64 total = totalMem / 1024 / 1024;
        sprintf(buffer, "%lld/%lld MB", usage, total);
        UI_DrawString(stringX, stringY, 0x000000, buffer);
        stringX += (SPACING_X * 10);
    } else {
        UI_DrawString(stringX, stringY, 0xFF0000, "[g/\x03] Err");
        stringX += (SPACING_X * 10);
    }
    

    
    memset(buffer, 0, sizeof(buffer));
    u64 timeMs = (u64)((double)UI_FrameTime() / (double)268123.480f); // 268123.480f ticks per millisecond
    sprintf(buffer, "[g/\x02]t: %ldms", (u32)(timeMs)); // not ideal since it still has some time before actually being done rendering
    stringX = TOP_SCREEN_WIDTH - PADDING - (SPACING_X * (strlen(buffer) + 1 - 4)); // subtract a few for the formating char
    UI_DrawString(stringX, stringY, timeMs >= 16 ? 0xFF0000 : 0x000000, buffer); // red if render time is more then 16ms, technically this doesn't mater
    

    if (DEBUG_MODE){

        u32 pH = TOP_SCREEN_HEIGHT - (2 * PADDING) - (20 + (1 * PADDING));
        u32 pW = 75;
        u32 pX = TOP_SCREEN_WIDTH - pW - PADDING;
        u32 pY = PADDING;

        UI_FillRect(pX, pY, pW, pH, BASE_COLOUR);
        UI_DrawPanel(pX, pY, pW, pH, true);

        stringY = pY + PADDING;
        UTouchPoint t = UI_GetTouchPos();
        UI_DrawStringFormat(pX + PADDING, stringY, 0x000000, "%d,%d", t.x, t.y);
        stringY += SPACING_Y;
        STouchPoint c = UI_GetCursorPos();
        UI_DrawStringFormat(pX + PADDING, stringY, 0x000000, "%d,%d", c.x, c.y);
        stringY += SPACING_Y;
        STouchPoint cd = UI_GetCursorDelta();
        UI_DrawStringFormat(pX + PADDING, stringY, 0x000000, "%d,%d", cd.x, cd.y);
        stringY += SPACING_Y;
        UI_DrawStringFormat(pX + PADDING, stringY, 0x000000, "ts %08X", _UI_GetTopScreenFormat().raw);
        stringY += SPACING_Y;

    }
    if (!DEBUG_MODE && wasDebug){
        topScreenDirty = true;
    }
    wasDebug = DEBUG_MODE;

    UI_SetScreen(false);
}

#define MAX_WINDOW_COUNT 8
static bool windowInit[MAX_WINDOW_COUNT] = { 0 };
static bool windowAlive[MAX_WINDOW_COUNT] = { 0 }; // if false the window can be overriden
static Window windows[MAX_WINDOW_COUNT] = { 0 };

static void InitWindow(u32 i, char* title, u32 x, u32 y, u32 width, u32 height){
    windowAlive[i] = true;
    windows[i] = UI_CreateWindow(x, y, width, height, title);
    windows[i].visible = true;
    windows[i].active = true;
    windows[i].OnDraw = NULL;
    windows[i].OnUpdate = NULL;
    
    windows[i].button = UI_CreateButton(0, 0, 9, 9, "-", BASE_COLOUR, 0x000000);
    
    windowInit[i] = true;
}

void DefaultWindowStart(){
    for (u32 i = 0; i < MAX_WINDOW_COUNT; i++) {
        if (windowAlive[i] && windows[i].active){
            UI_UpdateWindow(&windows[i]);
        }
    }
}
void DefaultWindowEnd(){
    for (u32 i = 0; i < MAX_WINDOW_COUNT; i++) {
        if (windowAlive[i] && windows[i].active){
            if (windows[i].visible){
                UI_DrawWindow(&windows[i]);
            }
        }
    }
}

// returns a pointer to the window in a window array. do NOT free this instance
Window* CreateWindow(char* title, u32 x, u32 y, u32 w, u32 h, void (*OnDraw)(Window *window), void (*OnUpdate)(Window *window)){
    for (u32 i = 0; i < MAX_WINDOW_COUNT; i++) {
        if (windowAlive[i] == false){
            InitWindow(i, title, x, y, w, h);
            windows[i].OnDraw = OnDraw;
            windows[i].OnUpdate = OnUpdate;
            return &windows[i];
        }
    }
    return NULL;
}

/// 
/// Other menues
///  
MenuResult ctn_Menu_Credits(){

    // WARNING: string is over 128 bytes long, if formating use UI_DrawStringFormatSized for a sized buffer
    static char* str1 = 
        "Credit to crawdad105 for doing stuff.\n"
        "Google, ChatGPT and Claude helped some.\n"
        "Only trivial code was written by AI if any.\n"
        "\n"
        "I dont know if what im doing is legal.\n"
    ;
    static Button backButton = { 0 };
    MENU_ONCE_START{
        backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);
    } MENU_ONCE_END;

    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Credits");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;

        UI_DrawStringFormatSized(0, 2 * PADDING, 2 * PADDING + 20, 0x000000, str1);

        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
    } MENU_LOOP_END();
    
    return MENU_RESULT_OK;
}
MenuResult ctn_Menu_Help(){

    // WARNING: string is over 128 bytes long, if formating use UI_DrawStringFormatSized for a sized buffer
    static char* str1 = "Im surprised your reading this.\nI didn't think i would release this\n well if you need help\n go to my discord and ask i guess.\n  [c/716CCD:crawdad.dev/discord]\n(the link doesn't work here obviously)";
    static Button backButton = { 0 };
    MENU_ONCE_START{
        backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);
    } MENU_ONCE_END;

    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Help");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;

        UI_DrawString(2 * PADDING, 2 * PADDING + 20, 0x000000, str1);

        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
    } MENU_LOOP_END();
    
    return MENU_RESULT_OK;
}
MenuResult ctn_Menu_Test(){

    static Button backButton = { 0 };
    #define BTN_CNT 5
    static char* strings[BTN_CNT] = {
        "Page 1",
        "Page 2",
        "Page 3",
        "Page 4",
        "Page 5"
    };
    static TabPanel tab = { 0 };
    //static Button buttons[BTN_CNT];
    MENU_ONCE_START{
        backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);
        //for (u32 i = 0; i < BTN_CNT; i++) {
        //    buttons[i] = _UI_CreateButton(0, 0, 0, 0, strings[i], BASE_COLOUR, 0x000000);
        //}
        tab = UI_CreateTabPanel(2 * PADDING, PADDING + 20 + PADDING, BOTTOM_SCREEN_WIDTH - (4 * PADDING), BOTTOM_SCREEN_HEIGHT - (2 * 20) - (4 * PADDING), 50, 16, strings, BTN_CNT);
    } MENU_ONCE_END;

    bool wasDebug = DEBUG_MODE;
    DEBUG_MODE = true;

    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Test");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;

        u32 pressed = UI_GetPressedButtons();
        if (pressed & KEY_R) {
            if (tab.selectedTab < tab.tabCount - 1)
                tab.selectedTab++;
        }
        if (pressed & KEY_L) {
            if (tab.selectedTab > 0)
                tab.selectedTab--;
        }

        UI_DoTabPanel(&tab);

        UI_DrawStringFormat(tab.x + PADDING, tab.y + tab.btnHeight + PADDING, 0x000000, "Selected Tab Page %d", tab.selectedTab);
        
        if (tab.selectedTab == 0){

            u32 innerX = tab.x + 2;
            u32 innerY = tab.y + tab.btnHeight + 2;
            //u32 innerWidth = tab.w - 4;
            //u32 innerHeight = tab.h - 4 - tab.btnHeight;

            UI_ClearClippingPlane();

            // test drawing
            for (u32 i = 0; i < 100; i++) {
                for (u32 j = 0; j < 100; j++) {
                    s32 x = (s32)(i + innerX + 5);
                    s32 y = (s32)(j + innerY + 5);
                    UI_DrawPixel(x, y, 0x00FF00);
                }
            }

            // test clipping
            UI_SetClippingPlane(innerX + 25, innerY + 25, 25, 25);

            UI_FillRect(innerX, innerY, 50, 50, 0xFF0000);
            UI_ClearClippingPlane();

            UI_SetClippingPlane(innerX + 5, innerY + 5, 5, 5);
            UI_FillRect(innerX + 5, innerY + 5, 5, 5, 0x7F00FF); // purple
            UI_FillRect(innerX, innerY + 25, 50, 50, 0xFF0000);
            UI_DrawButtonOutline(innerX, innerY + 25, 50, 50, false);
            UI_ClearClippingPlane();
            
            // test text
            UI_DrawString(innerX + 5, innerY + 5, 0x000000, "Some Text");
        }
        
        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
    } MENU_LOOP_END();
    
    DEBUG_MODE = wasDebug;

    return MENU_RESULT_OK;

}
static bool shouldReboot = false;

// main menu
extern MenuResult ctn_Menu_FileMenu();
extern MenuResult ctn_Menu_PluginMenu();
extern MenuResult ctn_Menu_Networking();
MenuResult ctn_Menu_MainMenu() {
    
    static Button networkButton = { 0 };
    static Button procViewBtn = { 0 };
    static Button pluginButton = { 0 };
    MENU_ONCE_START{
        u32 btnArrW = (16 * SPACING_X) + 4;
        u32 btnArrX = 2 * PADDING;
        procViewBtn =   UI_CreateButton(btnArrX, (2 * PADDING) + 20 + SPACING_Y + PADDING,    btnArrW, 20, "Process Viewer", BASE_COLOUR, 0x000000);
        networkButton = UI_CreateButton(btnArrX, procViewBtn.y + procViewBtn.h + PADDING,     btnArrW, 20, "Networking", BASE_COLOUR, 0x000000);
        pluginButton =  UI_CreateButton(btnArrX, networkButton.y + networkButton.h + PADDING, btnArrW, 20, "ctn Plugins", BASE_COLOUR, 0x000000);
    } MENU_ONCE_END;

    Button backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);
    Button reboot = UI_CreateButton(PADDING + backButton.x + backButton.w, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Reboot", BASE_COLOUR, 0xFF0000);
    Button credits = UI_CreateButton(BOTTOM_SCREEN_WIDTH - (1 * PADDING) - 75, BOTTOM_SCREEN_HEIGHT - PADDING - 20,                75, 20, "Credits...", BASE_COLOUR, 0x000000);
    Button help = UI_CreateButton(BOTTOM_SCREEN_WIDTH - (1 * PADDING) - 75, BOTTOM_SCREEN_HEIGHT - PADDING - 20 - PADDING - 20, 75, 20, "Help...", BASE_COLOUR, 0x000000);
    Button testButton = UI_CreateButton(BOTTOM_SCREEN_WIDTH - (1 * PADDING) - 75, BOTTOM_SCREEN_HEIGHT - PADDING - 20 - PADDING - 20 - PADDING - 20, 75, 20, "Test...", BASE_COLOUR, 0x000000);

    Button fileButton = UI_CreateButton(BOTTOM_SCREEN_WIDTH - (1 * PADDING) - 75, BOTTOM_SCREEN_HEIGHT - PADDING - 20 - PADDING - 20 - PADDING - 20 - PADDING - 20, 75, 20, "Files...", BASE_COLOUR, 0x000000);
    
    //Button procViewBtn = UI_CreateButton(2 * PADDING, (2 * PADDING) + 20 + SPACING_Y + PADDING, (16 * SPACING_X) + 4, 20, "Process Viewer", BASE_COLOUR, 0x000000);
    //Button testInput = UI_CreateButton(procViewBtn.x, procViewBtn.y + procViewBtn.h + PADDING, procViewBtn.w, 20, "Test Input", BASE_COLOUR, 0x000000);
    //Button pluginButton = UI_CreateButton(testInput.x, testInput.y + testInput.h + PADDING, testInput.w, 20, "ctn Plugins", BASE_COLOUR, 0x000000);

    CheckBox debugBox = UI_CreateCheckBox(BOTTOM_SCREEN_WIDTH - (1 * PADDING) - 75, PADDING * 2 + 20, SPACING_Y + 2, "X", 0x000000, 0xFFFFFF, DEBUG_MODE);

    //Button startNetworkThreadBtn = UI_CreateButton(debugBox.x, debugBox.y + debugBox.size + PADDING, 75, 20, "Net Thread", BASE_COLOUR, 0x000000);

    char inputBuffer[64];
    memset(inputBuffer, 0, sizeof(inputBuffer));

    bool doReboot = false;

    u32 held = 0;
    MENU_LOOP_START(); {
        held = UI_GetHeldButtons();
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("[c/716CCD:ctn Firmware]");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;

        MENU_DO_BUTTON(reboot) {
            if (held & KEY_R){
                doReboot = true;
                _CloseMenu(); break;
            } else {
                UI_DisplayMessage("To reboot hold R and press the button.");
            }
        }

        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_DO_BUTTON(credits) MENU_CHANGE_MENU(ctn_Menu_Credits);
        MENU_DO_BUTTON(help) MENU_CHANGE_MENU(ctn_Menu_Help);
        MENU_DO_BUTTON(testButton) MENU_CHANGE_MENU(ctn_Menu_Test);
        MENU_DO_BUTTON(fileButton) MENU_CHANGE_MENU(ctn_Menu_FileMenu);

        MENU_DO_BUTTON(procViewBtn) MENU_CHANGE_MENU(ctn_Menu_ProcList);
        MENU_DO_BUTTON(networkButton) MENU_CHANGE_MENU(ctn_Menu_Networking);
        MENU_DO_BUTTON(pluginButton) MENU_CHANGE_MENU(ctn_Menu_PluginMenu);

        //MENU_DO_BUTTON(startNetworkThreadBtn) { }

        UI_DrawString((2 * PADDING), (2 * PADDING) + 20, 0x000000, "Welcome to... idk, [c/716CCD:ctn Firmware]?");

        // debug stuff

        UI_DoCheckBox(&debugBox);
        DEBUG_MODE = debugBox.checked;
        UI_DrawString(debugBox.x + debugBox.size + PADDING, debugBox.y + 1, DEBUG_MODE ? 0x00FF00 : 0x00000, "Debug");

        // other stuff

        DrawDefaultUpperScreenStuff();

        DefaultWindowEnd();
    } MENU_LOOP_END();

    if (doReboot){
        shouldReboot = true;
        return MENU_RESULT_REBOOT;
    }
    return MENU_RESULT_OK;

}


extern void menuLeave(void); // from menu.c
extern MenuResult ctn_Menu_ProcView();
extern MenuResult ctn_Menu_ProcList();
void ctn_MenuManager(void){
    isctnMenuOpen = true;
    rosalinaMod_ShouldOpenCustomMenu = false; // used for debugging thread (and in an actual modification to rosalina in menu.c)

    _UI_InitTopScreen();
    _OpenMenu(ctn_Menu_MainMenu);
    if (StartOpenDebuggerMenu){
        _OpenMenu(ctn_Menu_ProcList);
        _OpenMenu(ctn_Menu_ProcView);
        StartOpenDebuggerMenu = false;
    }

    MenuResult res = 0;
    while (true){
        topScreenDirty = true; // set top screen dirty so when switching menus it gets reset
        if (_DoMenu(&res) == 0){
            break;
        }
        if (shouldReboot) break;
    }
    _UI_DeinitTopScreen();

    isctnMenuOpen = false;
    rosalinaMod_ShouldOpenCustomMenu = false;

    // same code as in RosalinaMenu_PowerOffOrReboot
    if (shouldReboot){
        menuLeave(); // default rosalina stuff
        APT_HardwareResetAsync();
    }
}