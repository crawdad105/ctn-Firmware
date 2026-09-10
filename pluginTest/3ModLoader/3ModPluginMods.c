
#include "3ModPlugin.h"

#include <string.h>
#include "../../sysmodules/rosalina/include/ctn/global_types.h"
#include "../../sysmodules/rosalina/include/ctn/plugin_.h"

/*
static bool WriteSetDefault_Cave(PluginAPI *api, CodeCave *codeCave){
    if (codeCave == NULL || codeCave->startAddr == 0) return false;
    static char buffer1[] = {
        0x04, 0xE0, 0x2D, 0xE5, // push 	{lr}
        0x85, 0x02, 0x85, 0xE0, // add      r0, r5, r5, lsl #5
        0x00, 0x30, 0xA0, 0xE3, // mov      r3, #0
        0x00, 0x01, 0x84, 0xE0, // add      r0, r4, r0, lsl #2
        //0x01, 0x20, 0xA0, 0xE3, // mov      r2, #1
        //0x00, 0x10, 0x9F, 0xE5, // ldr      r1, [pc]
        //0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        //0xA7, 0x00, 0x00, 0x00, // .word
        0x01, 0x50, 0x85, 0xE2, // add      r5, r5, #1
        0x04, 0xC0, 0x9F, 0xE5, // ldr      ip, [pc, #4]
        0x3C, 0xFF, 0x2F, 0xE1, // blx      ip
        0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        0xDC, 0xDF, 0x3A, 0x00, // .word
        0x04, 0xE0, 0x9D, 0xE4, // pop      {lr}
        0x1E, 0xFF, 0x2F, 0xE1	// bx       lr
    };
    u32 modSize = sizeof(buffer1);
    if (codeCave->startAddr + modSize > codeCave->endAddr) return false;
    if (!api->Process.Debug.WriteProcessMemory(codeCave->startAddr, sizeof(buffer1), buffer1)){
        return false;
    }
    codeCave->SetDefault_Item_Addr = codeCave->startAddr;
    codeCave->startAddr += modSize;
    return true;
}
static bool WriteSetDefaultCall_Cave(PluginAPI *api, CodeCave *codeCave, u32 id, u32 stack){
    if (codeCave == NULL || codeCave->startAddr == 0 || codeCave->SetDefault_Item_Addr == 0) return false;
    static char buffer1[] = {
        0x00, 0x10, 0x9F, 0xE5, // ldr      r1, [pc]
        0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        0x01, 0x00, 0x00, 0x00, // .word
        
        0x00, 0x20, 0x9F, 0xE5, // ldr      r2, [pc]
        0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        0x01, 0x00, 0x00, 0x00, // .word

        0x04, 0xc0, 0x9f, 0xe5, // ldr      ip, [pc, #4]
        0x3c, 0xff, 0x2f, 0xe1, // blx      ip
        0x02, 0xf0, 0x8f, 0xe2, // add      pc, pc, #2
        0x00, 0x00, 0x00, 0x00  // .word
    };
    buffer1[8 + 0] = (id & 0x00) >> 0;
    buffer1[8 + 1] = (id & 0xFF00) >> 8;
    buffer1[8 + 2] = (id & 0xFF0000) >> 16;
    buffer1[8 + 3] = (id & 0xFF000000) >> 24;

    buffer1[20 + 0] = (stack & 0x00) >> 0;
    buffer1[20 + 1] = (stack & 0xFF00) >> 8;
    buffer1[20 + 2] = (stack & 0xFF0000) >> 16;
    buffer1[20 + 3] = (stack & 0xFF000000) >> 24;

    buffer1[36 + 0] = (codeCave->SetDefault_Item_Addr & 0x00) >> 0;
    buffer1[36 + 1] = (codeCave->SetDefault_Item_Addr & 0xFF00) >> 8;
    buffer1[36 + 2] = (codeCave->SetDefault_Item_Addr & 0xFF0000) >> 16;
    buffer1[36 + 3] = (codeCave->SetDefault_Item_Addr & 0xFF000000) >> 24;

}

*/

static bool HolyHandGrenadeMod(PluginAPI *api, CodeCave *codeCave){
    u32 modSize = 29 * 4;
    if (codeCave == NULL || codeCave->startAddr == 0){
        return false;
    }
    static char buffer2[29 * 4] = {
        0x04, 0xE0, 0x2D, 0xE5, // push 	{lr}
        
        0x85, 0x02, 0x85, 0xE0, // add      r0, r5, r5, lsl #5
        0x00, 0x30, 0xA0, 0xE3, // mov      r3, #0
        0x00, 0x01, 0x84, 0xE0, // add      r0, r4, r0, lsl #2
        0x01, 0x20, 0xA0, 0xE3, // mov      r2, #1
        0x00, 0x10, 0x9F, 0xE5, // ldr      r1, [pc]
        0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        0xA7, 0x00, 0x00, 0x00, // .word
        0x01, 0x50, 0x85, 0xE2, // add      r5, r5, #1
        0x04, 0xC0, 0x9F, 0xE5, // ldr      ip, [pc, #4]
        0x3C, 0xFF, 0x2F, 0xE1, // blx      ip
        0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        0xDC, 0xDF, 0x3A, 0x00, // .word

        0x85, 0x02, 0x85, 0xE0, // add      r0, r5, r5, lsl #5
        0x00, 0x30, 0xA0, 0xE3, // mov      r3, #0
        0x00, 0x01, 0x84, 0xE0, // add      r0, r4, r0, lsl #2
        0x01, 0x20, 0xA0, 0xE3, // mov      r2, #1
        0x00, 0x10, 0x9F, 0xE5, // ldr      r1, [pc]
        0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        0xC5, 0x13, 0x00, 0x00, // .word
        0x01, 0x50, 0x85, 0xE2, // add      r5, r5, #1
        0x04, 0xC0, 0x9F, 0xE5, // ldr      ip, [pc, #4]
        0x3C, 0xFF, 0x2F, 0xE1, // blx      ip
        0x02, 0xF0, 0x8F, 0xE2, // add      pc, pc, #2
        0xDC, 0xDF, 0x3A, 0x00, // .word

        0x04, 0xE0, 0x9D, 0xE4, // pop      {lr}
        0x00, 0xC0, 0x9F, 0xE5, // ldr      ip, [pc]
        0x1C, 0xFF, 0x2F, 0xE1, // bx       ip
        0x24, 0x76, 0x3F, 0x00  // .word
    };

    // write code cave first incase the second read fails
    if (!api->Process.Debug.WriteProcessMemory(codeCave->startAddr, sizeof(buffer2), buffer2)){
        return false;
    }
    
    static char buffer1[4 * 3] = {
        0x00, 0xC0, 0x9F, 0xE5, // LDR  r12, [pc, #0]
        0x1C, 0xFF, 0x2F, 0xE1, // BLX  r12
        0x00, 0x00, 0x00, 0x00  // .word
    };
    buffer1[8] = (codeCave->startAddr & 0xFF) >> 0;
    buffer1[9] = (codeCave->startAddr & 0xFF00) >> 8;
    buffer1[10] = (codeCave->startAddr & 0xFF0000) >> 16;
    buffer1[11] = (codeCave->startAddr & 0xFF000000) >> 24;
    if (!api->Process.Debug.WriteProcessMemory(HOLY_HAND_GRENADE_ADDR, sizeof(buffer1), buffer1)){
        return false;
    }
    
    codeCave->startAddr += modSize;
    return true;
}

static bool GuideSellMod(PluginAPI *api, CodeCave *codeCave){
    if (codeCave == NULL || codeCave->startAddr == 0){
        return false;
    }
    
    u32 item_count = 26;
    u32 item_ids[] = {
        // weapons
        677, // scyth
        673, // Icemourne 
        1553, // sdmg
        // random
        858, // blizzard in bottle "experimental ID"
        602,
        603,
        604,
        619,
        620,
        621,
        2503,
        // exclusive items
        5048,
        5061,
        5055,
        5052,
        // reds stuff
        665,
        666,
        667,
        668,
        678,
        // other
        407,
        17,
        1131,
        486,
        393,
        255
    };

    // write override jump to normal npc shop code for guide
    static char buffer1[4 * 3] = {
        0x00, 0xC0, 0x9F, 0xE5, // LDR  r12, [pc, #0]
        0x1C, 0xFF, 0x2F, 0xE1, // BX   r12
        0x00, 0x00, 0x00, 0x00  // .word
    };
    buffer1[8] =  (GUIDE_SELL_STUFF_JUMP_BACK & 0xFF) >> 0;
    buffer1[9] =  (GUIDE_SELL_STUFF_JUMP_BACK & 0xFF00) >> 8;
    buffer1[10] = (GUIDE_SELL_STUFF_JUMP_BACK & 0xFF0000) >> 16;
    buffer1[11] = (GUIDE_SELL_STUFF_JUMP_BACK & 0xFF000000) >> 24;
    if (!api->Process.Debug.WriteProcessMemory(GUIDE_SELL_STUFF_ADDR, sizeof(buffer1), buffer1)){
        return false;
    }

    // we want the jump to here
    u32 codeCaveJumpAddr = codeCave->startAddr;

    // write first init and jump for code
    static char buffer_1[] = {
        0x04, 0x30, 0x9f, 0xe5,
        0x04, 0x10, 0x8f, 0xe2,
        0x03, 0xf0, 0x8f, 0xe0,
    };
    if (!api->Process.Debug.WriteProcessMemory(codeCave->startAddr, sizeof(buffer_1), buffer_1)){
        return false;
    }
    codeCave->startAddr += sizeof(buffer_1);

    // write length
    static char buffer_2[] = {
        0x00, 0x00, 0x00, 0x00
    };
    buffer_2[0] = ((item_count * 4) & 0xFF) >> 0;
    buffer_2[1] = ((item_count * 4) & 0xFF00) >> 8;
    buffer_2[2] = ((item_count * 4) & 0xFF0000) >> 16;
    buffer_2[3] = ((item_count * 4) & 0xFF000000) >> 24;
    if (!api->Process.Debug.WriteProcessMemory(codeCave->startAddr, sizeof(buffer_2), buffer_2)){
        return false;
    }
    codeCave->startAddr += sizeof(buffer_2);

    // write ids
    for (u32 i = 0; i < item_count; i++) {
        buffer_2[0] = (item_ids[i] & 0xFF) >> 0;
        buffer_2[1] = (item_ids[i] & 0xFF00) >> 8;
        buffer_2[2] = (item_ids[i] & 0xFF0000) >> 16;
        buffer_2[3] = (item_ids[i] & 0xFF000000) >> 24;
        if (!api->Process.Debug.WriteProcessMemory(codeCave->startAddr, sizeof(buffer_2), buffer_2)){
            return false;
        }
        codeCave->startAddr += sizeof(buffer_2);
    }
    
    // write item code
    static char buffer_3[] = {
        0x0a, 0x00, 0x2d, 0xe9,
        0x00, 0x10, 0x91, 0xe5,
        0x85, 0x02, 0x85, 0xe0,
        0x00, 0x30, 0xa0, 0xe3,
        0x00, 0x01, 0x84, 0xe0,
        0x01, 0x20, 0xa0, 0xe3,
        0x01, 0x50, 0x85, 0xe2,
        0x04, 0xc0, 0x9f, 0xe5,
        0x3c, 0xff, 0x2f, 0xe1,
        0x02, 0xf0, 0x8f, 0xe2,
        0xdc, 0xdf, 0x3a, 0x00,
        0x0a, 0x00, 0xbd, 0xe8,
        0x04, 0x10, 0x81, 0xe2,
        0x04, 0x30, 0x43, 0xe2,
        0x00, 0x00, 0x53, 0xe3,
        0xef, 0xff, 0xff, 0x1a,

        0x00, 0xc0, 0x9f, 0xe5, // ldr      ip, [pc]
        0x1c, 0xff, 0x2f, 0xe1, // bx       ip
        0x9c, 0x8f, 0x3f, 0x00  // .word
    };
    if (!api->Process.Debug.WriteProcessMemory(codeCave->startAddr, sizeof(buffer_3), buffer_3)){
        return false;
    }
    codeCave->startAddr += sizeof(buffer_3);
    
    // patch jump table to our own code
    static char buffer2[4 * 1] = {
        0x00, 0x00, 0x00, 0x00, // .word
    };
    buffer2[0 + 0] = (codeCaveJumpAddr & 0xFF) >> 0;
    buffer2[0 + 1] = (codeCaveJumpAddr & 0xFF00) >> 8;
    buffer2[0 + 2] = (codeCaveJumpAddr & 0xFF0000) >> 16;
    buffer2[0 + 3] = (codeCaveJumpAddr & 0xFF000000) >> 24;
    if (!api->Process.Debug.WriteProcessMemory(GUIDE_SELL_STUFF_ADDR_2, sizeof(buffer2), buffer2)){
        return false;
    }
    
    // patch a check for valid talk-to npcs
    static char buffer3[4 * 2] = {
        0x00, 0x00, 0x41, 0xE2,	// sub      r0, r1, #0
        0x11, 0x00, 0x50, 0xE3  // cmp      r0, #0x11
    };
    if (!api->Process.Debug.WriteProcessMemory(GUIDE_SELL_STUFF_ADDR_3, sizeof(buffer3), buffer3)){
        return false;
    }
    
    return true;
}
