// only .text, .data, .bss and .rodata section can have data in it
// you can NOT use global variables or functions only static things are aloud
// the first function to run will be the first in the file
// the signature must be "bool [your function name](PluginAPI *api, void *_state)"
// _state is an arbitrary section of memory you can use can only be a maximum of 4096 bytes (you probably dont need to use it, it was used before static variables were allowed)
// the api gives you functions, preferably dont change the functions in the api but what do i care, if you crash ur 3ds its not my problem

// change this to where you saved the header file
#include "../Luma3DS_Mod/sysmodules/rosalina/include/ctn/global_types.h" // included in plugin_.h
#include "../Luma3DS_Mod/sysmodules/rosalina/include/ctn/plugin_.h"

// other header files could be an issue
// some functions like memset or memcpy are handled but other are not
#include <string.h> 

// needs to run before anything else, this is the entry point
// THIS IS CALLED EVERY UPDATE FRAME THE 3DS DRAWS THE PLUGIN MENU, AT MOST EVERY 16ms (i think) NOT JUST ONCE.
static bool pluginUpdate(PluginAPI *api, void *_state);
bool mainUpdate(PluginAPI *api, void *_state){
    // must return. true means continue, false means stop the plugin
    return pluginUpdate(api, _state);
}

// State type, you dont need to do this, but variables could be placed in here
typedef struct {
    u32 num1;
    bool init;
} State;

static bool pluginUpdate(PluginAPI *api, void *_state){
    // safety check incase our "State" type is larger then its supposed to be
    if (sizeof(State) >= 0x1000) { // this gets eaten by compiler optimizations
        // draw text under the top menu text
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
    }

    u32 posX = TEXT_PADDING * 2;
    u32 posY = TEXT_PADDING * 2;
    // draw over original menu text
    api->UI.FillRect(posX, posY, TEXT_SPACING_X * 11, TEXT_SPACING_Y, BASE_COLOUR);
    api->UI.DrawStringFormat(posX, posY, 0x000000, "Hello World From Plugin 2.");

    return true;
}