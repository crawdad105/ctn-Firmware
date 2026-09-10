#include <string.h>
#include "../../sysmodules/rosalina/include/ctn/global_types.h"
#include "../../sysmodules/rosalina/include/ctn/plugin_.h"

// only .text, .data, .bss and .rodata section can have data in it
// you can NOT use global variables or functions only static things are aloud
// the first function to run will be the first in the file
// the signature must be "bool [your function name](PluginAPI *api, void *_state)"
// _state can only be a maximum of 4096 bytes
// preferably dont change the functions in the api but what do i care, if you crash ur 3ds its not my problem

// python3 run.py pluginTest

typedef struct {
    u32 num1;
    bool init;
} State;

static Button attachTerrariaButton = { 0 };
static Button grenadeMod = { 0 };
static TabPanel tabPanel = { 0 };
static u32 codeCave = 0;
static bool terrariaAttached = false;
static bool grenadeModActive = false;

typedef enum{
    STATE_NONE,
    ERROR,
    NO_PROCESS,
    NOT_DEBUGGING,
    NOT_TERRARIA,

    ZERO_TERRARIA,
    BASE_TERRARIA,
    SHOP_TERRARIA,
    OTHER_TERRARIA,
    NEW_TERRARIA
} GameState;

static GameState gameState = STATE_NONE;
static u32 version = 0;

#define TERRARIA_BASE   0x000400000016A900ULL
#define TERRARIA_UPDATE 0x0004000E0016A900ULL

#define DO_BUTTON(button) api->UI.DoButton(&button)

static bool HollyHandGrenadeMod(PluginAPI *api);
static bool DebuggingTerraria(PluginAPI *api);

static bool pluginUpdate(PluginAPI *api, void *_state){
    
    // check incase state is too big
    if (sizeof(State) >= 0x1000) { // this gets eaten by compiler optimizations
        api->UI.DrawStringFormat(TEXT_PADDING * 2, TEXT_PADDING + 20 + TEXT_PADDING, 0x000000, "State too large (%d)", sizeof(State));   
        return false;
    }
    if (api->API_VERSION() != 1){ // check api version
        return false;
    }
    
    State *state = (State*)_state;
    if (!(state->init)){
        state->num1 = 0;
        state->init = true;
        attachTerrariaButton = api->UI.CreateButton(0, 0, 100, 20, "Attach Terraria", BASE_COLOUR, 0x000000);
        grenadeMod = api->UI.CreateButton(BOTTOM_SCREEN_WIDTH - (2 * TEXT_PADDING) - 100, (2 * TEXT_PADDING) + 20 + TEXT_SPACING_Y + TEXT_PADDING, 100, 20, "Holly Grenade", BASE_COLOUR, 0x000000);
        char* tabNames[4] = {
            "Main",
            "Hacks",
            "Mods",
            "Other"
        };
        tabPanel = api->UI.CreateTabPanel(TEXT_PADDING, (TEXT_PADDING * 2) + 20, BOTTOM_SCREEN_WIDTH - (TEXT_PADDING * 2), BOTTOM_SCREEN_HEIGHT - (TEXT_PADDING * 4) - (20 * 2), 50, 16, tabNames, 4);
    }
    
    if (api->Process.IsProcessAttached()){
        if (api->Process.Debug.IsDebugging()){
            ProcessData proc = api->Process.GetProcess();
            if (proc.titleId == TERRARIA_BASE){
                TitleInfo info = api->Process.GetTitleUpdateInfo1(TERRARIA_BASE);
                version = info.version;
                if (info.loc == 0){ // if failed to get update data
                    info = api->Process.GetTitleInfo(TERRARIA_BASE);
                    version = info.version;
                    if (info.loc == 0){
                        version = 0;
                    }
                }
                if (version == 0){
                    gameState = ZERO_TERRARIA;
                } else if (version == 1024) {
                    gameState = BASE_TERRARIA;
                } else if (version == 5216) {
                    gameState = NEW_TERRARIA;
                } else if (version == 1040) {
                    gameState = SHOP_TERRARIA;
                } else {
                    gameState = OTHER_TERRARIA;
                }
            } else {
                gameState = NOT_TERRARIA;
            }
        } else {
            gameState = NOT_DEBUGGING;
        }
    } else {
        gameState = NO_PROCESS;
    }

    tabPanel.tabCount = 1;
    if (gameState == NEW_TERRARIA){
        tabPanel.tabCount = 4;
    }

    u32 posX = TEXT_PADDING * 2;
    u32 posY = TEXT_PADDING * 2;
    api->UI.FillRect(posX, posY, TEXT_SPACING_X * 11, TEXT_SPACING_Y, BASE_COLOUR);
    api->UI.DrawStringFormat(posX, posY, 0x000000, "Welcome to [c/7F0000:3][c/716CCD:ModLoader]. cModLoader for the 3ds.");

    api->UI.UpdateTabPanel(&tabPanel);
    api->UI.DrawTabPanel(&tabPanel);
    u32 innerX = tabPanel.x + TEXT_PADDING;
    u32 innerY = tabPanel.y + TEXT_PADDING + tabPanel.btnHeight;

    switch (tabPanel.selectedTab) {
    case 0: { // main
        u32 stringX = innerX;
        u32 stringY = innerY;
        if (gameState == ZERO_TERRARIA){
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "An error occurred. \"Version 0\"");
            break;
        }
        if (gameState == ERROR){
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "An error occurred");
            break;
        }
        if (gameState == NO_PROCESS){
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "No proccess attached.\nAttach terraria to continue.");
            break;
        }
        if (gameState == NOT_DEBUGGING){
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "Not debugger attached.\nDebug terraria to continue.");
            break;
        }
        if (gameState == NOT_TERRARIA){
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "Not Terraria.\nDebug Terraria to continue.");
            break;
        }
        
        if (gameState == OTHER_TERRARIA){
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "YOU MAY HAVE AN UNARCHIVED VERSION OF TERRARIA");
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "DO NOT UPDATE THE GAME");
            stringY += TEXT_SPACING_Y;
            stringY = api->UI.DrawString(stringX, stringY, 0x000000, "Go to GodMode9 and dump the game");
            stringY = api->UI.DrawString(stringX, stringY, 0x000000, "  then report it to someone and keep a copy.");
            stringY = api->UI.DrawString(stringX, stringY, 0x000000, "  crawdad105 will want this version to archive");
            stringY += TEXT_SPACING_Y;
        }
        if (gameState != NEW_TERRARIA){
            stringY = api->UI.DrawStringFormat(stringX, stringY, 0xFF0000, "Unsupported version of terraria (%d)(v%d.%d.%d)", version, version >> 10, (version >> 4) & 0x3F, version & 0xF);
            stringY += TEXT_SPACING_Y;
            break;
        }

        stringY = api->UI.DrawStringFormat(stringX, stringY, 0x000000, "Terraria attached! (%d)(v%d.%d.%d)", version,version >> 10, (version >> 4) & 0x3F, version & 0xF);
        stringY += TEXT_SPACING_Y;

    } break;
    case 1: { // Hacks
        // no hurt
        // 
    } break;
    case 2: { // Mods
        
    } break;        
    case 3: { // Other

    } break;        
    default:{
        u32 stringX = innerX;
        u32 stringY = innerY;
        stringY = api->UI.DrawStringFormat(stringX, stringY, 0xFF0000, "Error Tab %d", tabPanel.selectedTab);
    } break;        
    }

    return true;
}

/* Works on the first version
static bool HollyHandGrenadeMod(PluginAPI *api){
    u8 buffer1[3 * 4] = { 0x00, 0xC0, 0x9F, 0xE5, 0x1C, 0xFF, 0x2F, 0xE1, (codeCave & 0x00) >> 0, (codeCave & 0xFF00) >> 8, (codeCave & 0xFF0000) >> 16, (codeCave & 0xFF000000) >> 24 };
    u8 buffer2[172] = { 
        0x04, 0xE0, 0x2D, 0xE5,
        0x85, 0x03, 0x84, 0xE0, 
        0x00, 0x30, 0xA0, 0xE3, 
        0x01, 0x20, 0xA0, 0xE3, 
        0xA8, 0x10, 0xA0, 0xE3,
        0x01, 0x50, 0x85, 0xE2, 
        0x04, 0xC0, 0x9F, 0xE5, 
        0x3C, 0xFF, 0x2F, 0xE1, 
        0x02, 0xF0, 0x8F, 0xE2,
        0x48, 0x0F, 0x39, 0x00, 
        0x85, 0x03, 0x84, 0xE0, 
        0x00, 0x30, 0xA0, 0xE3, 
        0x01, 0x20, 0xA0, 0xE3,
        0xA6, 0x10, 0xA0, 0xE3, 
        0x01, 0x50, 0x85, 0xE2, 
        0x04, 0xC0, 0x9F, 0xE5, 
        0x3C, 0xFF, 0x2F, 0xE1,
        0x02, 0xF0, 0x8F, 0xE2, 
        0x48, 0x0F, 0x39, 0x00, 
        0x85, 0x03, 0x84, 0xE0, 
        0x00, 0x30, 0xA0, 0xE3,
        0x01, 0x20, 0xA0, 0xE3, 
        0xA7, 0x10, 0xA0, 0xE3, 
        0x01, 0x50, 0x85, 0xE2, 
        0x04, 0xC0, 0x9F, 0xE5,
        0x3C, 0xFF, 0x2F, 0xE1, 
        0x02, 0xF0, 0x8F, 0xE2, 
        0x48, 0x0F, 0x39, 0x00, 
        0x85, 0x03, 0x84, 0xE0,
        0x00, 0x30, 0xA0, 0xE3, 
        0x01, 0x20, 0xA0, 0xE3, 
        0x00, 0x10, 0x9F, 0xE5, 
        0x02, 0xF0, 0x8F, 0xE2,
        0xC5, 0x13, 0x00, 0x00, 
        0x01, 0x50, 0x85, 0xE2, 
        0x04, 0xC0, 0x9F, 0xE5, 
        0x3C, 0xFF, 0x2F, 0xE1,
        0x02, 0xF0, 0x8F, 0xE2, 
        0x48, 0x0F, 0x39, 0x00, 
        0x04, 0xE0, 0x9D, 0xE4, 
        0x00, 0xC0, 0x9F, 0xE5,
        0x1C, 0xFF, 0x2F, 0xE1, 
        0xEC, 0x96, 0x3D, 0x00
    };

    if (api->Process.Debug.WriteProcessMemory(0x003D96A4, sizeof(buffer1), buffer1)){    
    } else { api->UI.DisplayMessage("write memory Fail 1"); return false; }
    if (api->Process.Debug.WriteProcessMemory(codeCave, sizeof(buffer2), buffer2)){    
    } else { api->UI.DisplayMessage("write memory Fail 2"); return false; }
    return true;
}
*/

static bool DebuggingTerraria(PluginAPI *api){
    if (!(api->Process.IsProcessAttached())) return false;
    if (!(api->Process.Debug.IsDebugging())) return false;
    ProcessData proc = api->Process.GetProcess();
    if (proc.titleId != TERRARIA_BASE && proc.titleId != TERRARIA_UPDATE) return false;
    return true;
}
