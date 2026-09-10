#include <string.h>
#include "../../sysmodules/rosalina/include/ctn/global_types.h"
#include "../../sysmodules/rosalina/include/ctn/plugin_.h"

static bool pluginUpdate(PluginAPI *api, void *_state);
static bool entry(PluginAPI *api, void *_state){
    return pluginUpdate(api, _state);
}

// python3 asmCode/pluginTest/run.py asmCode/pluginTest/3ModPlugin asmCode/pluginTest/3ModPluginMods
// give /luma/ctn/3mod.cplg test2/Lum3ds/Luma3DS/asmCode/pluginTest/3ModPlugin.cplg

// only .text, .data, .bss and .rodata section can have data in it
// you can NOT use global variables or functions only static things are aloud
// to use multiple files you must include them using "#include "file.c"
// the first function to run will be the first in the file
// the signature must be "bool [your function name](PluginAPI *api, void *_state)"
// _state can only be a maximum of 4096 bytes
// preferably dont change the functions in the api but what do i care, if you crash ur 3ds its not my problem

// python3 run.py pluginTest

#include "3ModPlugin.h"
#include "3ModPluginMods.c"
#include "3ModPluginUI.c"

typedef struct {
    u32 num1;
    bool init;
} State;

typedef struct {
    CheckBox checkBox;
    char* name;
    char* text;
    u32 memoryAddress;
    char originalBuffer[4];
    char newBuffer[4];
    bool enabled;
} Hack;

// hacks
static Hack christmasHack = { 0 };
static Hack halloweenHack = { 0 };
static Hack valentinesHack = { 0 };
static Hack oktoberfestHack = { 0 };
static Hack thanksgivingHack = { 0 };
static Hack easterHack = { 0 };

static u32 hackCount = 6;
static Hack *hacks[6] = {
    &christmasHack,
    &halloweenHack,
    &valentinesHack,
    &oktoberfestHack,
    &thanksgivingHack,
    &easterHack
};

// mods

static Button grenadeMod = { 0 };
static bool grenadeModActive = false;
static Button guideSellMod = { 0 };
static bool guideSellModActive = false;

static Button attachTerrariaButton = { 0 };
static Button initModderButton = { 0 };
static TabPanel tabPanel = { 0 };
static CodeCave codeCave = { 0 };
static bool selfAttachedTerraria = false;
static bool modderInit = false;
static bool hacksInit = false;

static Button setItemId = { 0 };
static Button setItemCount = { 0 };

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

#define DO_BUTTON(button) api->UI.DoButton(&button)
#define DO_CHECK(check) api->UI.DoCheckBox(&check)

static bool DebuggingTerraria(PluginAPI *api);

// false because it causes all sorts of issues
#define SELF_DEBUG_TERRARIA false

static bool pluginUpdate(PluginAPI *api, void *_state){
    
    // check incase state is too big
    if (sizeof(State) >= 0x1000) { // this gets eaten by compiler optimizations
        api->UI.DrawStringFormat(TEXT_PADDING * 2, TEXT_PADDING + 20 + TEXT_PADDING, 0x000000, "State too large (%d)", sizeof(State));   
        return false;
    }
    if (api->API_VERSION() != 1){ // check api version
        return false;
    }
    
    if(SELF_DEBUG_TERRARIA && api->PLUGIN_LAST){
        if (selfAttachedTerraria){
            api->Process.Debug.DetachDebugger();
            api->Process.DetachProcess();
        }
        return true; // doesn't mater
    }

    State *state = (State*)_state;
    if (!(state->init)){
        state->num1 = 0;
        state->init = true;
        attachTerrariaButton = api->UI.CreateButton(0, 0, 100, 20, "Debug Terraria", BASE_COLOUR, 0x000000);
        char* tabNames[4] = {
            "Main",
            "Hacks",
            "Mods",
            "Other"
        };
        tabPanel = api->UI.CreateTabPanel(TEXT_PADDING, (TEXT_PADDING * 2) + 20, BOTTOM_SCREEN_WIDTH - (TEXT_PADDING * 2), BOTTOM_SCREEN_HEIGHT - (TEXT_PADDING * 4) - (20 * 2), 50, 16, tabNames, 4);
        initModderButton = api->UI.CreateButton(0, 0, 100, 20, "Init 3ModLoader", BASE_COLOUR, 0x000000);
        
        u32 innerX = tabPanel.x + TEXT_PADDING;
        u32 innerY = tabPanel.y + TEXT_PADDING + tabPanel.btnHeight;
        
        //otherPageScroll = api->UI.CreateScrollZone();

        grenadeMod =   api->UI.CreateButton(innerX, innerY, 100, 20, "Holy Grenade", BASE_COLOUR, 0x000000);
        guideSellMod = api->UI.CreateButton(innerX, innerY + 20 + TEXT_PADDING, 100, 20, "Guide Sell Mod", BASE_COLOUR, 0x000000);

        setItemId =    api->UI.CreateButton(innerX, 0, 90, 20, "Set Item 0 Id", BASE_COLOUR, 0x000000);
        setItemCount = api->UI.CreateButton(innerX + setItemId.w + TEXT_PADDING, 0, 110, 20, "Set Item 0 Count", BASE_COLOUR, 0x000000);

        // size and pos calculated when drawn
        christmasHack.checkBox      = api->UI.CreateCheckBox(0, 0, 0, "X", 0x000000, 0xFFFFFF, false);
        christmasHack.memoryAddress = CHRISTMAS_HACK_ADDR;
        char buffer1[4] = { CHRISTMAS_HACK_OLD };
        char buffer2[4] = { CHRISTMAS_HACK_NEW };
        memcpy(christmasHack.originalBuffer, buffer1, 4);
        memcpy(christmasHack.newBuffer, buffer2, 4);
        christmasHack.name = "Christmas";
        christmasHack.text = "Always Christmas";
        christmasHack.enabled = false;
        

        halloweenHack.checkBox      = api->UI.CreateCheckBox(0, 0, 0, "X", 0x000000, 0xFFFFFF, false);
        halloweenHack.memoryAddress = HALLOWEEN_HACK_ADDR;
        char buffer3[4] = { HALLOWEEN_HACK_OLD };
        char buffer4[4] = { HALLOWEEN_HACK_NEW };
        memcpy(halloweenHack.originalBuffer, buffer3, 4);
        memcpy(halloweenHack.newBuffer, buffer4, 4);
        halloweenHack.name = "Halloween";
        halloweenHack.text = "Always Halloween";
        halloweenHack.enabled = false;

        
        valentinesHack.checkBox     = api->UI.CreateCheckBox(0, 0, 0, "X", 0x000000, 0xFFFFFF, false);
        valentinesHack.memoryAddress = VALENTINES_HACK_ADDR;
        char buffer5[4] = { VALENTINES_HACK_OLD };
        char buffer6[4] = { VALENTINES_HACK_NEW };
        memcpy(valentinesHack.originalBuffer, buffer5, 4);
        memcpy(valentinesHack.newBuffer, buffer6, 4);
        valentinesHack.name = "Valentine's";
        valentinesHack.text = "Always Valentine's";
        valentinesHack.enabled = false;
        

        oktoberfestHack.checkBox    = api->UI.CreateCheckBox(0, 0, 0, "X", 0x000000, 0xFFFFFF, false);
        oktoberfestHack.memoryAddress = OKTOBERFEST_HACK_ADDR;
        char buffer7[4] = { OKTOBERFEST_HACK_OLD };
        char buffer8[4] = { OKTOBERFEST_HACK_NEW };
        memcpy(oktoberfestHack.originalBuffer, buffer7, 4);
        memcpy(oktoberfestHack.newBuffer, buffer8, 4);
        oktoberfestHack.name = "Oktoberfest";
        oktoberfestHack.text = "Always Oktoberfest";
        oktoberfestHack.enabled = false;

        
        thanksgivingHack.checkBox   = api->UI.CreateCheckBox(0, 0, 0, "X", 0x000000, 0xFFFFFF, false);
        thanksgivingHack.memoryAddress = THANKSGIVING_HACK_ADDR;
        char buffer9[4] = { THANKSGIVING_HACK_OLD };
        char buffer10[4] = { THANKSGIVING_HACK_NEW };
        memcpy(thanksgivingHack.originalBuffer, buffer9, 4);
        memcpy(thanksgivingHack.newBuffer, buffer10, 4);
        thanksgivingHack.name = "Thanksgiving";
        thanksgivingHack.text = "Always Thanksgiving";
        thanksgivingHack.enabled = false;
        
        
        easterHack.checkBox         = api->UI.CreateCheckBox(0, 0, 0, "X", 0x000000, 0xFFFFFF, false);
        easterHack.memoryAddress = EASTER_HACK_ADDR;
        char buffer11[4] = { EASTER_HACK_OLD };
        char buffer12[4] = { EASTER_HACK_NEW };
        memcpy(easterHack.originalBuffer, buffer11, 4);
        memcpy(easterHack.newBuffer, buffer12, 4);
        easterHack.name = "Easter";
        easterHack.text = "Always Easter";
        easterHack.enabled = false;

    }
    
    if (api->Process.IsProcessAttached()){
        if (api->Process.Debug.IsDebugging()){
            if (!hacksInit){
                char buffer3[4] = { 0 };
                for (int i = 0; i < hackCount; i++) {
                    hacks[i]->enabled = false;
                    if (!(api->Process.Debug.ReadProcessMemory(hacks[i]->memoryAddress, 4, buffer3))){
                        api->UI.DisplayMessageFormat("Failed to read proccess memory (0x%08X)", hacks[i]->memoryAddress);
                    } else {
                        if (memcmp(buffer3, hacks[i]->originalBuffer, 4) == 0){
                            hacks[i]->enabled = true;
                        } else if (memcmp(buffer3, hacks[i]->newBuffer, 4) == 0){
                            hacks[i]->enabled = true;
                        } else {
                            api->UI.DisplayMessageFormat("Unexpected memory value (hack: \"%d\")", hacks[i]->name);
                        }
                    }
                }
                hacksInit = true;
            }
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
                if (version == 0) gameState = ZERO_TERRARIA;
                else if (version == TER_VER_BASE) gameState = BASE_TERRARIA;
                else if (version == TER_VER_NEW ) gameState = NEW_TERRARIA;
                else if (version == TER_VER_SHOP) gameState = SHOP_TERRARIA;
                else gameState = OTHER_TERRARIA;
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
        tabPanel.tabCount = 2;
        if (modderInit){
            tabPanel.tabCount = 4;
        }
    }

    u32 posX = TEXT_PADDING * 2;
    u32 posY = TEXT_PADDING * 2;
    api->UI.FillRect(posX, posY, TEXT_SPACING_X * 11, TEXT_SPACING_Y, BASE_COLOUR);
    api->UI.DrawStringFormat(posX, posY, 0x000000, "Welcome to [c/7F0000:3][c/716CCD:ModLoader]. cModLoader for the 3ds.");

    bool tabDirty = false;
    u32 num = tabPanel.selectedTab;
    api->UI.UpdateTabPanel(&tabPanel);
    api->UI.DrawTabPanel(&tabPanel);
    tabDirty = tabPanel.selectedTab != num; 
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
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "No proccess attached.\nAttach Terraria to continue.");
            if (SELF_DEBUG_TERRARIA){
                attachTerrariaButton.x = stringX;
                attachTerrariaButton.y = stringY;
                if (DO_BUTTON(attachTerrariaButton)){
                    if (api->Process.AttachedProcessTitle(TERRARIA_BASE)){
                        if (api->Process.Debug.AttachDebugger(TERRARIA_BASE)){
                            selfAttachedTerraria = true;
                        } else api->UI.DisplayMessage("Failed to debug proccess");
                    } else api->UI.DisplayMessage("Failed to attach proccess");
                }
            }
            break;
        }
        if (gameState == NOT_DEBUGGING){
            stringY = api->UI.DrawString(stringX, stringY, 0xFF0000, "Not debugger attached.\nDebug Terraria to continue.");
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
            stringY = api->UI.DrawStringFormat(stringX, stringY, 0xFF0000, "Unsupported version of Terraria (%d)(v%d.%d.%d)", version, version >> 10, (version >> 4) & 0x3F, version & 0xF);
            stringY += TEXT_SPACING_Y;
            break;
        }

        stringY = api->UI.DrawStringFormat(stringX, stringY, 0x000000, "Terraria attached! (%d)(v%d.%d.%d)", version,version >> 10, (version >> 4) & 0x3F, version & 0xF);

        if (!modderInit){
            initModderButton.x = stringX;
            initModderButton.y = stringY;
            if (DO_BUTTON(initModderButton)){
                // check for code cave using holly hand grenade mod
                // ideally this is put in unused memory somewhere
                char buffer1[4 * 3] = { 0 };
                api->Process.Debug.ReadProcessMemory(HOLY_HAND_GRENADE_ADDR, 4 * 3, buffer1);
                char buffer2[4 * 2] = { 0x00, 0xC0, 0x9F, 0xE5, 0x1C, 0xFF, 0x2F, 0xE1 };
                bool found = false;
                for (u32 i = 0; i < 4 * 2; i++) {
                    if (buffer1[i] == buffer2[i]){
                        found = true;
                        break;
                    }
                }
                codeCave.startAddr = 0;
                if (!found) api->Process.Debug.AllocateCodeCave(&codeCave.startAddr);
                else {
                    ((char*)&codeCave)[0] = buffer1[8];
                    ((char*)&codeCave)[1] = buffer1[9];
                    ((char*)&codeCave)[2] = buffer1[10];
                    ((char*)&codeCave)[3] = buffer1[11];
                }

                modderInit = true;
            }
        } else {
            if (codeCave.startAddr != 0){
                stringY = api->UI.DrawString(stringX, stringY, 0x000000, "3ModLoader initalized!");
                stringY = api->UI.DrawStringFormat(stringX, stringY, 0x000000, "Code Cave: 0x%08X", codeCave.startAddr);
            } else {
                stringY = api->UI.DrawString(stringX, stringY, 0x000000, "Failed to initialize 3ModLoader.\nReport to crawdad105 because\n  idk what to do now...");
            }
        }

    } break;
    case 1: { // Hacks
        u32 innerX = tabPanel.x + TEXT_PADDING;
        u32 innerY = tabPanel.y + TEXT_PADDING + tabPanel.btnHeight;
        u32 size = TEXT_SPACING_Y;
        for (int i = 0; i < hackCount; i++) {
            if (hacks[i] == NULL){
                api->UI.DrawStringFormat(innerX, innerY, 0xFF0000, "Null hack %d", i);
            } else {
                hacks[i]->checkBox.x = innerX;
                hacks[i]->checkBox.y = innerY;
                hacks[i]->checkBox.size = 13;
                hacks[i]->checkBox.charOffsetX = 0;
                hacks[i]->checkBox.charOffsetY = 0;
                if (hacks[i]->enabled){
                    hacks[i]->checkBox.colour = 0xFFFFFF;
                    if (DO_CHECK((hacks[i]->checkBox))){
                        bool c = hacks[i]->checkBox.checked;
                        if (!api->Process.Debug.WriteProcessMemory(hacks[i]->memoryAddress, 4, c ? hacks[i]->newBuffer : hacks[i]->originalBuffer)){
                            api->UI.DisplayMessage("Failed to write memory");
                        }
                    }
                } else {
                    hacks[i]->checkBox.colour = 0x969696;
                    api->UI.DrawCheckBox(&(hacks[i]->checkBox));
                }
                if (hacks[i]->text == NULL){
                    api->UI.DrawStringFormat(hacks[i]->checkBox.x + hacks[i]->checkBox.size, hacks[i]->checkBox.y + 1, 0xFF0000, "Null hack text %d", i);
                } else {
                    api->UI.DrawString(hacks[i]->checkBox.x + hacks[i]->checkBox.size, hacks[i]->checkBox.y + 1, 0x000000, hacks[i]->text);
                }
            }
            innerY += size + TEXT_PADDING - 2;
        }



        // no hurt
        // max health
    } break;
    case 2: { // Mods
        api->UI.DrawString(grenadeMod.x + grenadeMod.w + TEXT_PADDING, grenadeMod.y, 0x000000, "Demolitionist sells holy\nhand grenade.");
        grenadeMod.textColour = grenadeModActive ? 0x007F00 : 0x000000;
        if (DO_BUTTON(grenadeMod)){
            if (!grenadeModActive){
                if (HolyHandGrenadeMod(api, &codeCave)){
                    grenadeModActive = true;
                } else {
                    api->UI.DisplayMessage("Failed to start mod");
                }
            } else api->UI.DisplayMessage("Mod already enabled");
        }
        api->UI.DrawString(guideSellMod.x + guideSellMod.w + TEXT_PADDING, guideSellMod.y, 0x000000, "Gives the guide a shop.");
        guideSellMod.textColour = guideSellModActive ? 0x007F00 : 0x000000;
        if (DO_BUTTON(guideSellMod)){
            if (!guideSellModActive){
                if (GuideSellMod(api, &codeCave)){
                    guideSellModActive = true;
                } else {
                    api->UI.DisplayMessage("Failed to start mod");
                }
            } else api->UI.DisplayMessage("Mod already enabled");
        }
    } break;
    case 3: { // Other
        u32 stringX = innerX;
        u32 stringY = innerY;
        char buffer3[4] = { 0 };
        if (api->Process.Debug.ReadProcessMemory((u32)PLAYER_ADDR_PTR, 4, buffer3)){
            stringY = api->UI.DrawStringFormat(stringX, stringY, 0x000000, "Plr Addr: 0x%08X", ((u32*)buffer3)[0]);
            stringY = api->UI.DrawStringFormat(stringX, stringY, 0x000000, "Plr Inv: 0x%08X", ((u32*)buffer3)[0] + PLAYER_INV_OFF);
        } else {
            stringY = api->UI.DrawStringFormat(stringX, stringY, 0xFF0000, "Plr Addr: Read Fail");
        }

        setItemId.y = stringY;
        setItemCount.y = stringY;

        if (DO_BUTTON(setItemId)){
            char input[64] = {0};
            if (api->UI.GetUserInput("Input item id. (base 10)\nmax: 65535, min: 1", 5, input)){
                u32 num = 0;
                bool fail = false;
                u32 i = 0;
                while(input[i] != 0){
                    num *= 10;
                    if (input[i] >= '0' && input[i] <= '9'){
                        num += input[i] - '0';
                    } else {
                        api->UI.DisplayMessageFormat("Invalid number \"%c\"", input[i]);
                        fail = true;
                        break;
                    }
                    i++;
                }
                if (!fail){
                    if (num > 0xFFFF){
                        api->UI.DisplayMessageFormat("Value too large");
                    } else if (num < 1){
                        api->UI.DisplayMessageFormat("Value too small");
                    } else {
                        api->Process.Debug.ReadProcessMemory((u32)PLAYER_ADDR_PTR, 4, buffer3);
                        api->Process.Debug.WriteProcessMemory(((u32*)buffer3)[0] + PLAYER_INV_OFF + PLAYER_ITEM_ID1_OFF, 2, (char*)&num);
                        api->Process.Debug.WriteProcessMemory(((u32*)buffer3)[0] + PLAYER_INV_OFF + PLAYER_ITEM_ID2_OFF, 2, (char*)&num);
                    }
                }
            }
        }
        if (DO_BUTTON(setItemCount)){
            char input[64] = {0};
            if (api->UI.GetUserInput("Input item count. (base 10)\nmax: 65535, min: 1", 5, input)){
                u32 num = 0;
                bool fail = false;
                u32 i = 0;
                while(input[i] != 0){
                    num *= 10;
                    if (input[i] >= '0' && input[i] <= '9'){
                        num += input[i] - '0';
                    } else {
                        api->UI.DisplayMessageFormat("Invalid number \"%c\"", input[i]);
                        fail = true;
                        break;
                    }
                    i++;
                }
                if (!fail){
                    if (num > 0xFFFF){
                        api->UI.DisplayMessageFormat("Value too large");
                    } else if (num < 1){
                        api->UI.DisplayMessageFormat("Value too small");
                    } else {
                        api->Process.Debug.ReadProcessMemory((u32)PLAYER_ADDR_PTR, 4, buffer3);
                        api->Process.Debug.WriteProcessMemory(((u32*)buffer3)[0] + PLAYER_INV_OFF + PLAYER_ITEM_COUNT1_OFF, 2, (char*)&num);
                        api->Process.Debug.WriteProcessMemory(((u32*)buffer3)[0] + PLAYER_INV_OFF + PLAYER_ITEM_COUNT2_OFF, 2, (char*)&num);
                    }
                }
            }
        }

    } break;
    default:{
        u32 stringX = innerX;
        u32 stringY = innerY;
        stringY = api->UI.DrawStringFormat(stringX, stringY, 0xFF0000, "Error Tab %d", tabPanel.selectedTab);
    } break;
    }

    return true;
}

static bool DebuggingTerraria(PluginAPI *api){
    if (!(api->Process.IsProcessAttached())) return false;
    if (!(api->Process.Debug.IsDebugging())) return false;
    ProcessData proc = api->Process.GetProcess();
    if (proc.titleId != TERRARIA_BASE && proc.titleId != TERRARIA_UPDATE) return false;
    return true;
}
