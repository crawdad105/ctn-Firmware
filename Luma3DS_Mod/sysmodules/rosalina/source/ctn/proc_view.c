#include <3ds.h>
#include <3ds/os.h> // for osGetTime()
#include "font.h" // for font[]
#include <MyThread.h>
#include "ctn/ctn.h"
#include "ctn/newUI.h"
#include "ctn/memory_.h"
#include "ctn/process.h"
#include "ctn/debug_.h"
#include "ctn/file.h"

#include <string.h> // memset and strcpy
#include <stdio.h> // for sprintf

// from base_menu.c
extern void DrawDefaultUpperScreenStuff();
extern void DefaultWindowStart();
extern void DefaultWindowEnd();

// do NOT use for active program, this will not stay as the open program
static u32 targetProcessPid = PID_NULL;
static struct {
    bool active;
    u32 start;
    u32 size;
} targetMemory;

#define GREENISH "008800"
#define ORANGEISH "CD7800"
#define PURPLE "5D00FF"

// scan stuff
void DrawDefaultProcInfo(){

    UI_SetScreen(true);

    UI_FillRect(PADDING, PADDING, 100, TOP_SCREEN_HEIGHT - (2 * PADDING) - (20 + (1 * PADDING)), BASE_COLOUR);
    UI_DrawPanel(PADDING, PADDING, 100, TOP_SCREEN_HEIGHT - (2 * PADDING) - (20 + (1 * PADDING)), true); // make sure not to draw over default top screen stuff
    u32 stringY = 2 * PADDING;
    u32 stringX = 2 * PADDING;
    UI_DrawStringFormat(stringX, stringY, 0x000000, "[c/" PURPLE ":%s] (%d)", targetProcess.name, targetProcess.pid);
    stringY += SPACING_Y;
    UI_DrawStringFormat(stringX, stringY, 0x000000, "H: 0x%X", targetProcess.handle);
    stringY += SPACING_Y * 1;
    if (targetProcess.debugging){
        UI_DrawStringFormat(stringX, stringY, 0x000000, "D: 0x%X", targetProcess.debugHandle);
        stringY += SPACING_Y * 1;
    }
    if (targetMemory.active){
        UI_DrawStringFormat(stringX, stringY, 0x000000, "pos: [c/" GREENISH ":0x%08X]\nsize:[c/" GREENISH ":0x%X]", targetMemory.start, targetMemory.size);
        stringY += SPACING_Y * 2; // *2 because new line
    }
    for (u32 i = 0; i < Scan.memoryScanPass; i++) {
        stringY += SPACING_Y * 1;
        UI_DrawStringFormat(stringX, stringY, 0x000000, "%02X: %d (%d)", (Scan.scanResults[i] & 0xFF000000) >> 24, Scan.scanResults[i] & 0x0FFFFF, (Scan.scanResults[i] & 0xF00000) >> 20);
    }

    UI_SetScreen(false);

}

MenuResult ctn_Menu_ProcMemScan(){
    Button backButton = UI_CreateButton(PADDING     , BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);

    u32 scanBtnW = 50;
    Button scanResetBtn = UI_CreateButton(BOTTOM_SCREEN_WIDTH - PADDING - scanBtnW, (2 * PADDING) + 20, scanBtnW, 20, "Reset", BASE_COLOUR, 0x000000);
    Button scanBtn = UI_CreateButton(scanResetBtn.x, scanResetBtn.y + scanResetBtn.h + PADDING, scanBtnW, 20, "Scan L1", BASE_COLOUR, 0x000000);
    Button scanBtn2 = UI_CreateButton(scanResetBtn.x, scanBtn.y + scanBtn.h + PADDING, scanBtnW, 20, "Scan L2", BASE_COLOUR, 0x000000);
    Button scanBtn3 = UI_CreateButton(scanResetBtn.x, scanBtn2.y + scanBtn2.h + PADDING, scanBtnW, 20, "Scan L3", BASE_COLOUR, 0x000000);
    Button scanHelpBtn = UI_CreateButton(scanResetBtn.x, scanBtn3.y + scanBtn3.h + PADDING + PADDING, scanBtnW, 20, "Help", BASE_COLOUR, 0x000000);

    u32 minAddr = targetMemory.start;
    u32 maxAddr = targetMemory.start + targetMemory.size;

    u32 width = 16;
    //u32 height = 16;
    u32 scroll = 0;

    u32 heldCounterUp = 0;
    u32 heldCounterDown = 0;
    u32 holdMax = 30; // should be long enough

    u32 pressed = 0;
    u32 held = 0;
    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Memory Scanner");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;

        pressed = UI_GetPressedButtons();
        held = UI_GetHeldButtons();
        
        if (pressed & KEY_DUP) {if (scroll != 0) scroll -= 1;}
        if (pressed & KEY_DDOWN) scroll += 1;
        if (held & KEY_DUP){
            if (heldCounterUp > holdMax) {if (scroll != 0) scroll -= 1;}
            else heldCounterUp++;
        } else heldCounterUp = 0;
        if (held & KEY_DDOWN){
            if (heldCounterDown > holdMax) scroll += 1;
            else heldCounterDown++;
        } else heldCounterDown = 0;

        if (scroll >= Scan.memoryScanAddressIndex){
            scroll = Scan.memoryScanAddressIndex - 1;
        }
        
        u32 doScan = 0;
        MENU_DO_BUTTON(scanBtn) doScan = 1;
        MENU_DO_BUTTON(scanBtn2) doScan = 2;
        MENU_DO_BUTTON(scanBtn3) doScan = 3;
        MENU_DO_BUTTON(scanResetBtn){ ScanMemoryReset(); }
        MENU_DO_BUTTON(scanHelpBtn){ 
            UI_CheckWithUserFormat("Ok", NULL, 
                "Scanning memory is split into 3 levels.\n"
                "This similar to what cheat engine can do.\n"
                "L1: 4096 byte chunks, L2: 256 bytes and L3 16 bytes.\n"
                "\n"
                "1.Chose L1 and provide a byte.\n"
                "2.Cause program memory to change.\n"
                "3.Run L1 again with a new expected byte.\n"
                "4.Once results <= 2048 repeat septs 1-3 for L2.\n"
                "5.Then repeat step 4 with L3.\n"
                "\n"
                "The same level may need to be done multiple times.\n"
                "You can not move down levels.\n"
                "Press reset to reset all results.\n"
            );
        }
        if (doScan > 0){
            if (Scan.memoryScanPass > 0){
                u32 lastCount = Scan.scanResults[Scan.memoryScanPass - 1] & 0x0FFFFF;
                u32 lastScan = (Scan.scanResults[Scan.memoryScanPass - 1] >> 20) & 0x00000F;
                if (lastScan > doScan){
                    UI_DisplayMessageFormat("Cannot scan lower level then previous scan.");
                    goto endDoScan;
                }
                if (doScan > lastScan && lastCount > 2048){
                    UI_DisplayMessageFormat("Too many results, scan again to reduce it.");
                    goto endDoScan;
                }
            } else{
                if (doScan != 1){
                    UI_DisplayMessageFormat("Must start with scan level 1.");
                    goto endDoScan;
                }
            }

            char userInput[64];
            memset(userInput, 0, sizeof(userInput));
            if (UI_GetUserInput("Input a single hex value", 64, userInput)){
                u32 num = 0;
                HexStringToNum(userInput, &num);
                if (num > 0xFF){
                    UI_DisplayMessageFormat("Input too large.");
                } else{
                    u32 count2 = 0;
                    u64 searchTime = ScanMemory(minAddr, maxAddr, targetProcess.debugHandle, num, &count2, doScan);
                    UI_DisplayMessageFormat("Found %d in %lldms (%d)", count2, (searchTime / TICKS_TO_MS), doScan);
                    // ScanMemory changes Scan.memoryScanPass
                    Scan.scanResults[Scan.memoryScanPass - 1] = (num << 24) | (count2 & 0x0FFFFF) | ((doScan << 20) & 0xF00000);
                }
            }
            scroll = 0;
            endDoScan:
        }

        if (Scan.memoryScanPass > 0){

            u32 stringY = 2 * PADDING + 20;
            u32 stringX = 0;

            u32 prevPageAddr = 1; // page address cant be 1 since its a multiple of 0x1000
            Result readRes = 0;
            for (u32 i = scroll; i < scroll + 16; i++) { // start at scroll
                if (i >= Scan.memoryScanAddressIndex) break;
                u32 addr = Scan.memoryScanAddresses[i];
                stringX = 1 * PADDING;
                bool badAddr = addr < minAddr || addr >= maxAddr; // this shouldn't be possible
                UI_DrawStringFormat(stringX, stringY, badAddr ? 0xFF0000 : 0x008800, "%08X", addr);
                stringX += 9 * SPACING_X;
                u32 pageRemain = PAGE_REMAIN(addr);
                u32 pageAddr = PAGE_ROUND(addr); // round to page addr
                if (prevPageAddr != pageAddr){ 
                    readRes = svcReadProcessMemory(dataBuffer, targetProcess.debugHandle, pageAddr, PAGE_SIZE); // not what memoryScanBuffer is for but oh well, its not needed data
                    prevPageAddr = pageAddr;
                }
                if (R_FAILED(readRes)) {
                    for (u32 j = 0; j < width; j++) {
                        UI_DrawStringFormat(stringX, stringY, 0xFF0000, "--");
                        stringX += (2 * SPACING_X) + 1;
                    }
                } else {
                    for (u32 j = 0; j < width; j++) {
                        char c = dataBuffer[pageRemain + j];
                        u32 colour = j % 2 == 0 ? 0x5B5B5B : 0;
                        UI_DrawStringFormat(stringX, stringY, colour, "%02X", c);
                        stringX += (2 * SPACING_X) + 1;
                    }
                }
                stringY += SPACING_Y;
            }
        }
        
        DrawDefaultUpperScreenStuff();

        DefaultWindowEnd();

        DrawDefaultProcInfo();

    } MENU_LOOP_END();

    MenuResult mres = { 0 };
    return mres;
}

MenuResult ctn_Menu_ProcMemView(){
    Button backButton = UI_CreateButton(PADDING     , BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);

    Button searchBtn = UI_CreateButton(BOTTOM_SCREEN_WIDTH - PADDING - 50, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Search", BASE_COLOUR, 0x000000);
    Button gotoBtn = UI_CreateButton(searchBtn.x - PADDING - 50, searchBtn.y, 50, 20, "Goto", BASE_COLOUR, 0x000000);
    Button scanBtn = UI_CreateButton(gotoBtn.x - PADDING - 50, searchBtn.y, 50, 20, "Scan", BASE_COLOUR, 0x000000);
    
    CheckBox toggleAscii = UI_CreateCheckBox(BOTTOM_SCREEN_WIDTH - PADDING - (SPACING_Y + 2), (2 * PADDING) + 20, SPACING_Y + 2, "X", 0x000000, 0xFFFFFF, false);

    u32 minAddr = targetMemory.start;
    u32 maxAddr = targetMemory.start + targetMemory.size;

    bool bufferPage1 = false;
    bool bufferPage2 = false;
    u32 bufferAddr1 = 0;
    u32 bufferAddr2 = 0;
    static u8 buffer[PAGE_SIZE * 2];

    static u32 searchResults[128];
    memset(searchResults, 0, sizeof(searchResults));
    static u32 searchResultsLength[128];
    memset(searchResultsLength, 0, sizeof(searchResultsLength));
    u32 maxSearchResults = 128;
    u32 searchResultCount = 0;

    static SearchTerm searchTerms[64];
    memset(searchTerms, 0, sizeof(searchTerms));
    u32 searchTermCount = 0;

    s32 scroll = 0;

    u32 heldCounterUp = 0;
    u32 heldCounterDown = 0;
    u32 heldCounterR = 0;
    u32 heldCounterL = 0;
    u32 holdMax = 30; // should be long enough

    u32 width = 16;
    u32 height = 16;

    u32 pressed = 0;
    u32 held = 0;
    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Memory Viewer");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;
        // do user input for scrolling TODO: add touch interface for pixel scrolling and maybe momentum for scrolling
        
        pressed = UI_GetPressedButtons();
        held = UI_GetHeldButtons();

        if (pressed & KEY_DUP) scroll -= 1;
        if (pressed & KEY_DDOWN) scroll += 1;
        if (held & KEY_DUP){
            if (heldCounterUp > holdMax) scroll -= 1;
            else heldCounterUp++;
        } else heldCounterUp = 0;
        if (held & KEY_DDOWN){
            if (heldCounterDown > holdMax) scroll += 1;
            else heldCounterDown++;
        } else heldCounterDown = 0;

        if (pressed & KEY_R) scroll += 0x100;
        if (pressed & KEY_L) scroll -= 0x100;
        if (held & KEY_R){
            if (heldCounterR > holdMax) scroll += 0x100;
            else heldCounterR++;
        } else heldCounterR = 0;
        if (held & KEY_L){
            if (heldCounterL > holdMax) scroll -= 0x100;
            else heldCounterL++;
        } else heldCounterL = 0;
        
        UI_DoCheckBox(&toggleAscii);

        MENU_DO_BUTTON(searchBtn) {
            char userInput[64];
            memset(userInput, 0, sizeof(userInput));

            if (UI_GetUserInput("Create search, use hex chars for bytes\n ? - wildcard, * - unknown byte gap.\n Use \"\" for strings.", 64, userInput)){
    
                CreatSearchTermList(userInput, searchTerms, 64, &searchTermCount);
                
                UI_DisplayMessageFormat("Search %d (%s)\n%06X\n%06X\n%06X\n%06X\n%06X\n%06X\n%06X\n%06X", searchTermCount, userInput, SearchToNum(searchTerms[0]), SearchToNum(searchTerms[1]), SearchToNum(searchTerms[2]), SearchToNum(searchTerms[3]), SearchToNum(searchTerms[4]), SearchToNum(searchTerms[5]), SearchToNum(searchTerms[6]), SearchToNum(searchTerms[7]));

                u64 searchTime = SearchMemory(minAddr, maxAddr, targetProcess.debugHandle, searchTerms, searchTermCount, searchResults, maxSearchResults, &searchResultCount, searchResultsLength, 0);
                UI_DisplayMessageFormat("Found %d in %lldms\n%08X\n%08X\n%08X\n%08X\n%08X\n%08X\n%08X\n%08X", searchResultCount, (searchTime / TICKS_TO_MS), searchResults[0], searchResults[1], searchResults[2], searchResults[3], searchResults[4], searchResults[5], searchResults[6], searchResults[7]);
            }

        }
        MENU_DO_BUTTON(gotoBtn){
            char userInput[64];
            memset(userInput, 0, sizeof(userInput));
            if (UI_GetUserInput("Input a 4 byte address.", 64, userInput)){
                SearchTerm search[4];
                memset(search, 0, sizeof(search));
                CreatSearchTermList(userInput, search, 4, NULL);
                // curAddr = minAddr + (scroll * width);
                // (curAddr - minAddr) / width = scroll
                u32 tempAddr = ((search[0].searchByte << 24) & 0xFF000000) | ((search[1].searchByte << 16) & 0x00FF0000) | ((search[2].searchByte << 8) & 0x0000FF00) | ((search[0].searchByte << 0) & 0x000000FF);
                scroll = (tempAddr - minAddr) / width; // i dont care if this overflows or underflows, thats the users problem
            }
        }
        MENU_DO_BUTTON(scanBtn) {
            MENU_CHANGE_MENU(ctn_Menu_ProcMemScan);
        }
        
        bool showAscii = toggleAscii.checked;

        u32 curAddr = (minAddr + (scroll * width));
        //u32 curEndAddr = (minAddr + (scroll * width) + (width * height));
        u32 roundedAddr = PAGE_ROUND(curAddr);

        // keep roundedAddr (and bufferAddr1) in bounds
        if (roundedAddr < minAddr) roundedAddr = minAddr;
        if (roundedAddr > maxAddr - PAGE_SIZE) roundedAddr = maxAddr - PAGE_SIZE;

        // check for change then read data if needed
        if (roundedAddr != bufferAddr1){
            bufferAddr1 = roundedAddr;
            bufferAddr2 = roundedAddr + PAGE_SIZE;
            Result readRes = svcReadProcessMemory(buffer, targetProcess.debugHandle, bufferAddr1, PAGE_SIZE);
            if (R_FAILED(readRes)) bufferPage1 = false;
            else bufferPage1 = true;
            if (bufferAddr2 < maxAddr){ // only read if not outside of range
                readRes = svcReadProcessMemory(buffer + PAGE_SIZE, targetProcess.debugHandle, bufferAddr2, PAGE_SIZE);
                if (R_FAILED(readRes)) bufferPage2 = false;
                else bufferPage2 = true;
            } else {
                bufferPage2 = false;
            }
        }

        // test error page
        //if (bufferAddr1 == 0x00103000) bufferPage1 = false;
        //if (bufferAddr2 == 0x00103000) bufferPage2 = false;

        u32 stringY = 2 * PADDING + 20;
        u32 stringX = 0;

        // because searchResults are ordered if we find a match we can move to the next one instead of checking every result with every address
        u32 searchMatchIndex = 0;
        // first first possible match
        for (u32 i = 0; i < searchResultCount; i++) {
            if (searchResults[i] >= curAddr){
                searchMatchIndex = i;
                break;
            }
        }
        bool doSearchMatch = searchResultCount > 0;
        u32 curMatchAddr = searchResults[searchMatchIndex];
        u32 curMatchLength = 0;

        for (u32 i = 0; i < height; i++) {
            u32 addr = curAddr + (width * i);
            stringX = 1 * PADDING;
            bool badAddr = addr < minAddr || addr >= maxAddr; // check if address is out of bounds
            UI_DrawStringFormat(stringX, stringY, badAddr ? 0xFF0000 : 0x008800, "%08X", addr);
            stringX += 9 * SPACING_X;
            if (!badAddr){
                bool page2 = addr >= bufferAddr2 ? true : false;
                badAddr = page2 ? !bufferPage2 : !bufferPage1; // check for bad page data, this should only happen if svcReadProcessMemory() fails
                u32 offset = (addr & 0xFFF) + (page2 ? PAGE_SIZE : 0); // get remainder from address and add PAGE_SIZE if on next page
                if (badAddr){
                    for (u32 j = 0; j < width; j++) {
                        UI_DrawStringFormat(stringX, stringY, 0xFF0000, "--");
                        stringX += (2 * SPACING_X) + 1;
                        if (j == (width / 2) - 1) stringX += (SPACING_X / 2);
                    }
                } else {
                    for (u32 j = 0; j < width; j++) {
                        char c = buffer[offset + j];
                        u32 colour = j % 2 == 0 ? 0x5B5B5B : 0;
                        if (doSearchMatch){
                            u32 realAddr = addr + j;
                            if (realAddr == curMatchAddr){
                                curMatchLength = searchResultsLength[searchMatchIndex]; // -1 because the current byte shouldn't be counted 
                                searchMatchIndex++;
                                if (searchMatchIndex >= searchResultCount){
                                    doSearchMatch = false;
                                } else {
                                    curMatchAddr = searchResults[searchMatchIndex];
                                }
                            }
                        } 
                        if (showAscii){
                            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) colour = 0xF2F2F2; // not including SPACE because its not visible anyways
                            if (!showAscii && c == 0) colour = 0x969696;
                            if (curMatchLength > 0){
                                colour = 0x00C9FF; // bright blue
                                curMatchLength--;
                            }
                            bool isPrintable = (c > 32 && c <= 126);
                            UI_DrawStringFormat(stringX, stringY, colour, "%c", isPrintable ? c : '.');
                            stringX += (SPACING_X);
                            if (j == (width / 2) - 1) stringX += 2;
                        } else {
                            if (curMatchLength > 0){
                                colour = 0x00C9FF; // bright blue
                                curMatchLength--;
                            }
                            UI_DrawStringFormat(stringX, stringY, colour, "%02X", c);
                            stringX += (2 * SPACING_X) + 1;
                            if (j == (width / 2) - 1) stringX += (SPACING_X / 2);
                        }
                    }
                }
            }
            stringY += SPACING_Y;
        }
        
        DrawDefaultUpperScreenStuff();

        DefaultWindowEnd();

        DrawDefaultProcInfo();

    } MENU_LOOP_END();

    return MENU_RESULT_OK;
}

extern Window* debugWindowCreate();
Window* debugWindow;
MenuResult ctn_Menu_ProcView(){

    static Button dumpMemoryButton = { 0 };
    static Button dumpMemoryStartButton = { 0 };
    static Button dumpMemoryEndButton = { 0 };
    u32 dumpMemoryStart = targetProcess.textSection.failed ? 0 : targetProcess.textSection.start;
    u32 dumpMemoryEnd = targetProcess.textSection.failed ? PAGE_SIZE : targetProcess.textSection.start + targetProcess.textSection.size;

    static Button writeMemoryButton = { 0 };
    static Button allocateCodeCave = { 0 };
    //static Button memoryMod = { 0 };

    static Button addBreakpointButton = { 0 };
    static Button removeBreakpointButton = { 0 };
    static Button breakpointButtons[MAX_BREAKPOINTS];

    static Button threadButtons[MAX_DEBUG_THREAD];
    static Button memButtons[MAX_SECTIONS];
    static u32 innerX = 0;
    static u32 innerY = 0;
    static u32 innerWidth = 0;
    static u32 innerHeight = 0;
    static TabPanel tab = { 0 };
    static ScrollZone memoryScroll = { 0 };

    if (!targetProcess.active){
        if (targetProcessPid == PID_NULL){
            _CloseMenu();
            return MENU_RESULT_OK;
            // i guess go back
        }
        OpenProcess(&targetProcess, targetProcessPid);
    }


    MENU_ONCE_START{
        for (int i = 0; i < MAX_SECTIONS; i++) { memButtons[i] = UI_CreateButton(0, 0, 4 * SPACING_X + 6, SPACING_Y, "View", BASE_COLOUR, 0x000000); memButtons[i].textOffsetX = -2; }
        for (int i = 0; i < MAX_DEBUG_THREAD; i++) { threadButtons[i] = UI_CreateButton(0, 0, 4 * SPACING_X + 6, SPACING_Y, "View", BASE_COLOUR, 0x000000); threadButtons[i].textOffsetX = -2; }
        for (int i = 0; i < MAX_BREAKPOINTS; i++) { breakpointButtons[i] = UI_CreateButton(0, 0, 3 * SPACING_X + 6, SPACING_Y, "Rmv", BASE_COLOUR, 0x000000); breakpointButtons[i].textOffsetX = -2; }
        debugWindow = debugWindowCreate();
        char *tabPageTitles[5] = { "Info", "Memory", "Debug", "Thread", "Break" };
        tab = UI_CreateTabPanel(1 * PADDING, PADDING + 20 + PADDING, BOTTOM_SCREEN_WIDTH - (2 * PADDING), BOTTOM_SCREEN_HEIGHT - (2 * 20) - (4 * PADDING), 50, 16, tabPageTitles, 5);
    
        innerX = tab.x + 2;
        innerY = tab.y + tab.btnHeight + 2;
        innerWidth = tab.w - 4;
        innerHeight = tab.h - 4 - tab.btnHeight;

        memoryScroll = UI_CreateScrollZone(innerX + PADDING, innerY + PADDING, 120, innerHeight - (2 * PADDING));

        u32 y = innerY + innerHeight - 14 - PADDING;
        u32 x = memoryScroll.x + memoryScroll.w + PADDING + (SPACING_X * 10) + 4;
        dumpMemoryStartButton = UI_CreateButton(x, y - SPACING_Y - 4, SPACING_X * 10 + 6, 14, "Dump Start", BASE_COLOUR, 0x000000);
        dumpMemoryStartButton.textOffsetX = -2;
        dumpMemoryEndButton = UI_CreateButton(x, y, SPACING_X * 10 + 6, 14, "Dump End", BASE_COLOUR, 0x000000);
        dumpMemoryEndButton.textOffsetX = -2;
        u32 w = SPACING_X * 4 + 6;
        dumpMemoryButton = UI_CreateButton(innerWidth - w + 1, y, w, 14, "Dump", BASE_COLOUR, 0x000000); // +1 because its better, idk why
        dumpMemoryButton.textOffsetX = -2;

        writeMemoryButton = UI_CreateButton(innerX + innerWidth - 75 - PADDING, innerY + PADDING, 75, 20, "Write Mem", BASE_COLOUR, 0x000000);
        allocateCodeCave = UI_CreateButton(writeMemoryButton.x, writeMemoryButton.y + (PADDING + 20), 75, 20, "Alloc Cave", BASE_COLOUR, 0x000000);
        //memoryMod = _UI_CreateButton(writeMemoryButton.x, writeMemoryButton.y + (PADDING + 20), 75, 20, "Alloc Cave", BASE_COLOUR, 0x000000);

        addBreakpointButton = UI_CreateButton(innerX + innerWidth - 75 - PADDING, innerY + innerHeight - (PADDING + 20), 75, 20, "Add Break", BASE_COLOUR, 0x000000);
        removeBreakpointButton = UI_CreateButton(addBreakpointButton.x, addBreakpointButton.y - (PADDING + 20), 75, 20, "Rmv Break", BASE_COLOUR, 0x000000);

    } MENU_ONCE_END;


    Button backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);

    // debug stuff
    Button toggleDebuggingButton = UI_CreateButton(innerX + innerWidth - 75 - PADDING, innerY + innerHeight - 20 - PADDING, 75, 20, targetProcess.debugging ? "Stop Debug" : "Start Debug", BASE_COLOUR, 0x000000);
    Button continueDebuggerButton = UI_CreateButton(toggleDebuggingButton.x, toggleDebuggingButton.y - PADDING - 20, 75, 20, "Continue", BASE_COLOUR, 0x000000);
    ScrollZone debugMessageScroll = UI_CreateScrollZone(innerX + PADDING, innerY + PADDING + SPACING_Y, innerWidth - (2 * PADDING) - 75 - PADDING, innerHeight - (2 * PADDING) - SPACING_Y);
    
    // thread stuff
    ScrollZone threadListScroll = UI_CreateScrollZone(innerX + PADDING, innerY + SPACING_Y + PADDING, 100, innerHeight - SPACING_Y - PADDING - PADDING);

    // breakpoint stuff
    ScrollZone breakListScroll = UI_CreateScrollZone(innerX + PADDING, innerY + SPACING_Y + PADDING, 95, innerHeight - SPACING_Y - PADDING - PADDING);

    /*
    
    Handle procHandle = 0;
    Handle debugHandle = 0;
    if (CurrentProcess.active){
        procHandle = CurrentProcess.handle;
        debugHandle = CurrentProcess.debugHandle;
        goto skipGetProcHandles;
    } else {
        if (CurrentProcess.info.pid == 0){
            DisplayMessage("pid zero");
            MenuResult mres = { 0 };
            return mres;
        }
        Result res = svcOpenProcess(&procHandle, CurrentProcess.info.pid);
        if(R_FAILED(res)) { DisplayMessage("svcOpenProcess Failed"); MenuResult mres = { 0 }; return mres; }
        
        // debug handle for memory read and write
        res = svcDebugActiveProcess(&debugHandle, CurrentProcess.info.pid);
        if(R_FAILED(res)) {
            DisplayMessage("svcDebugActiveProcess Failed");
            if (procHandle != 0){
                res = svcCloseHandle(procHandle);
                if(R_FAILED(res)) DisplayMessage("svcCloseHandle Failed");
            }
            MenuResult mres = { 0 };
            return mres;
        }
    
        CurrentProcess.handle = procHandle;
        CurrentProcess.debugHandle = debugHandle;
    }

    skipGetProcHandles:
    CurrentProcess.active = true;

    // get default values (not very usefull)
    MemSectionInfo text, rodata, data, heap;
    bool textStartMismatch = false, rodataStartMismatch = false, dataStartMismatch = false;;
    bool textSizeMismatch = false, rodataSizeMismatch = false, dataSizeMismatch = false;;

    s64 tempAddr;
    MemSectionInfo memInfo;

    // get text section info
    
    text.failed = false;
    text.perm = 0;
    text.state = 0;
    text.readable = false;
    if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, TEXT_SECTION_ADDR))){
        text.start = (u32)tempAddr;
        // not sure what happens if size failes to be goten but is found using queryMemInfo
        if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, TEXT_SECTION_SIZE))) text.size = (u32)tempAddr;
        else text.failed = true;
        memInfo = queryMemInfo(procHandle, text.start);
        if (!memInfo.failed){
            // not sure what it means when they mismatch, i guess just stick with the original
            if (text.start != memInfo.start) textStartMismatch = true;
            if (text.size != memInfo.size) textSizeMismatch = true;
            text.perm = memInfo.perm;
            text.state = memInfo.state;
        }
        text.failed = memInfo.failed;
        text.readable = memInfo.perm & MEMPERM_READ; // this is not how the normal sections are computed
    }

    // get read only data section info

    rodata.failed = false;
    rodata.perm = 0;
    rodata.state = 0;
    rodata.readable = false;
    if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, RODATA_SECTION_ADDR))){
        rodata.start = (u32)tempAddr;
        // not sure what happens if size failes to be goten but is found using queryMemInfo
        if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, RODATA_SECTION_SIZE))) rodata.size = (u32)tempAddr;
        else rodata.failed = true;
        memInfo = queryMemInfo(procHandle, rodata.start);
        if (!memInfo.failed){
            // not sure what it means when they mismatch, i guess just stick with the original
            if (rodata.start != memInfo.start) rodataStartMismatch = true;
            if (rodata.size != memInfo.size) rodataSizeMismatch = true;
            rodata.perm = memInfo.perm;
            rodata.state = memInfo.state;
        }
        rodata.failed = memInfo.failed;
        rodata.readable = memInfo.perm & MEMPERM_READ; // this is not how the normal sections are computed
    }

    // get data section info

    data.failed = false;
    data.perm = 0;
    data.state = 0;
    data.readable = false;
    if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, DATA_SECTION_ADDR))){
        data.start = (u32)tempAddr;
        // not sure what happens if size failes to be goten but is found using queryMemInfo
        if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, DATA_SECTION_SIZE))) data.size = (u32)tempAddr;
        else data.failed = true;
        memInfo = queryMemInfo(procHandle, data.start);
        if (!memInfo.failed){
            // not sure what it means when they mismatch, i guess just stick with the original
            if (data.start != memInfo.start) dataStartMismatch = true;
            if (data.size != memInfo.size) dataSizeMismatch = true;
            data.perm = memInfo.perm;
            data.state = memInfo.state;
        }
        data.failed = memInfo.failed;
        data.readable = memInfo.perm & MEMPERM_READ; // this is not how the normal sections are computed
    }
    
    // get heap section info

    heap.failed = false;
    heap.perm = 0;
    heap.state = 0;
    heap.readable = false;
    memInfo = queryMemInfo(procHandle, 0x08000000); // 
    heap.start = memInfo.start; // incase the heap section is not 0x08000000 it will be set so its fine if heap.start != 0x08000000 before (i think)
    heap.size = memInfo.size;
    heap.failed = memInfo.failed;
    heap.perm = memInfo.perm;
    heap.readable = memInfo.readable;
    heap.state = memInfo.state;
    heap.readable = memInfo.perm & MEMPERM_READ; // this is not how the normal sections are computed

    // +4 for default sections
    memset(CurrentProcess.sections, 0, sizeof(CurrentProcess.sections));
    MemSectionInfo *info = CurrentProcess.sections;
    memset(CurrentProcess.buttons, 0, sizeof(CurrentProcess.buttons));
    Button *memButtons = CurrentProcess.buttons;
    for (int i = 0; i < 64 + 4; i++) memButtons[i] = _UI_CreateButton(0, 0, 4 * SPACING_X + 6, SPACING_Y, "View", BASE_COLOUR, 0x000000);
    info[0] = text;
    info[1] = rodata;
    info[2] = data;
    info[3] = heap;

    u32 memPos = 0;
    int prevFailed = 0;
    int maxCount = 1024;
    MemInfo mem; // https://libctru.devkitpro.org/structMemInfo.html
    PageInfo out; // https://libctru.devkitpro.org/structPageInfo.html
    for(int i = 4; (i < (64 + 4) && maxCount > 0); i++, maxCount--){
        Result r = svcQueryProcessMemory(&mem, &out, procHandle, memPos);
        if (R_SUCCEEDED(r)){
            if (memPos == mem.base_addr + mem.size) memPos += PAGE_SIZE; // incase mem.size is 0 go to next page
            else memPos = mem.base_addr + mem.size;
            info[i].failed = false;
            info[i].start = mem.base_addr;
            info[i].size = mem.size;
            info[i].perm = (u32)mem.perm;
            info[i].state = (u32)mem.state;
            prevFailed = false;
            Result readRes = svcReadProcessMemory(dataBuffer, debugHandle, mem.base_addr, PAGE_SIZE); // technically this wont check if the entire section is readable, so maybe thats something to change
            info[i].readable = true;
            if (R_FAILED(readRes)) {
                info[i].readable = false;
            }
        } else {
            if (prevFailed){ // group failed, merge into previous
                i--;
                info[i].size += PAGE_SIZE;
            } else {
                info[i].start = memPos;
                info[i].size = PAGE_SIZE;
            }
            info[i].failed = true;
            info[i].perm = 0;
            info[i].state = 0;
            info[i].readable = false;
            memPos += PAGE_SIZE;
            prevFailed = true;
        }
    }

    */

    static char* string1 = "Segment %d:\n  pos: [c/" GREENISH ":0x%08X]\n  size: [c/" GREENISH ":0x%X]"; // 61
    static char* string2 = "  [c/" ORANGEISH ":%s], [c/%06X:%s]"; // 29
    static char* string3 = "Segment %d: [c/FF0000:Failed]\n  pos: [c/" GREENISH ":0x%08X]\n  size: [c/" GREENISH ":0x%X]"; // 79
    //static char* codeCaveInfoStr = "[c/" PURPLE ":Code Cave]\n  pos: [c/" GREENISH ":0x%08X]\n  size: [c/" GREENISH ":0x%X]"; // 70

    static char* string4 = "[c/" PURPLE ":%s]:\n  pos: [c/" GREENISH ":0x%08X]\n  size: [c/" GREENISH ":0x%X]"; // 53
    static char* string5 = "[c/" PURPLE ":%s]: [c/FF0000:Failed]\n  pos: [c/" GREENISH ":0x%08X]\n  size: [c/" GREENISH ":0x%X]"; // 71
    
    static char* defaultSectionNames[4] = { ".text", ".rodata", ".data/.bss", "Heap" };

    AM_TitleEntry amTitleDataEntry_base[1] = { 0 };
    AM_TitleEntry amTitleDataEntry_update[1] = { 0 };
    bool showAmTitles_base = false;
    bool showAmTitles_update = false;
    u32 amTitlesType_base = false;
    u32 amTitlesType_update = false;
    bool tabDirty = true;
    bool breakpointRemoving = false;
    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Process View");
        DefaultWindowStart();
        // doing back button later for scrolling
        MENU_BCLOSE;
        
        tab.tabCount = targetProcess.debugging ? 5 : 3;
        
        u32 oldSel = tab.selectedTab;

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

        if (tab.selectedTab != oldSel) {
            tabDirty = true;
            breakpointRemoving = false;
        }

        switch (tab.selectedTab) {
        case 0: { // info
            if (tabDirty){ // update tab page
                if (R_SUCCEEDED(amInit())){
                    
                    u64 titles[1];
                    
                    titles[0] = targetProcess.titleId; // normal id
                    amTitleDataEntry_base[0].titleID = 0;
                    showAmTitles_base = true;
                    amTitlesType_base = 0;
                    if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_SD, 1, titles, amTitleDataEntry_base))){
                        amTitlesType_base = 1;
                        if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_GAME_CARD, 1, titles, amTitleDataEntry_base))){
                            amTitlesType_base = 2;
                            if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_NAND, 1, titles, amTitleDataEntry_base))){ // sketchy nand read
                                showAmTitles_base = false;
                            }
                        }
                    }
                    
                    titles[0] = (targetProcess.titleId & 0x00000000FFFFFFFF) | 0x0004000E00000000; // update id
                    amTitleDataEntry_update[0].titleID = 0;
                    showAmTitles_update = true;
                    amTitlesType_update = 0;
                    if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_SD, 1, titles, amTitleDataEntry_update))){
                        amTitlesType_update = 1;
                        if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_GAME_CARD, 1, titles, amTitleDataEntry_update))){
                            amTitlesType_update = 2;
                            if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_NAND, 1, titles, amTitleDataEntry_update))){ // sketchy nand read
                                showAmTitles_update = false;
                            }
                        }
                    }
                    
                    amExit();
                }
            }
            u32 stringX = innerX + PADDING;
            u32 stringY = innerY + PADDING;
            stringY =                 UI_DrawString(stringX, stringY, 0x000000, "[c/" ORANGEISH ":Info page]");
            stringY =           UI_DrawStringFormat(stringX, stringY, 0x000000, " pid: %d", targetProcess.pid);
            stringY =           UI_DrawStringFormat(stringX, stringY, 0x000000, " name: [c/" PURPLE ":%s]", targetProcess.name);
            if (showAmTitles_base)
                 stringY =      UI_DrawStringFormat(stringX, stringY, 0x000000, " [c/" ORANGEISH ":Base info] (%s)", amTitlesType_base == 0 ? "SD" : (amTitlesType_base == 1 ? "CARD" : "NAND"));
            else stringY =      UI_DrawStringFormat(stringX, stringY, 0x000000, " [c/" ORANGEISH ":Base info]");
            stringY =           UI_DrawStringFormat(stringX, stringY, 0x000000, "  title id: %016llX", targetProcess.titleId);
            if (showAmTitles_base){
                stringY =       UI_DrawStringFormat(stringX, stringY, 0x000000, "  size: [c/" GREENISH ":0x%08lX]", amTitleDataEntry_base[0].size);
                stringY =       UI_DrawStringFormat(stringX, stringY, 0x000000, "  version: %d", amTitleDataEntry_base[0].version);
                u8 *p = amTitleDataEntry_base[0].unk;
                stringY =       UI_DrawStringFormat(stringX, stringY, 0x000000, "  other: %02X%02X%02X%02X%02X%02X", p[0], p[1], p[2], p[3], p[4], p[5]);
                if (showAmTitles_update && amTitleDataEntry_update[0].titleID != 0){
                    stringY =   UI_DrawStringFormat(stringX, stringY, 0x000000, " [c/" ORANGEISH ":Update info] (%s)", amTitlesType_update == 0 ? "SD" : (amTitlesType_update == 1 ? "CARD" : "NAND"));
                    stringY =   UI_DrawStringFormat(stringX, stringY, 0x000000, "  title id: %016llX", amTitleDataEntry_update[0].titleID);
                    stringY =   UI_DrawStringFormat(stringX, stringY, 0x000000, "  size: [c/" GREENISH ":0x%08lX]", amTitleDataEntry_update[0].size);
                    stringY =   UI_DrawStringFormat(stringX, stringY, 0x000000, "  version: %d", amTitleDataEntry_update[0].version);
                    p = amTitleDataEntry_update[0].unk;
                    stringY =   UI_DrawStringFormat(stringX, stringY, 0x000000, "  other: %02X%02X%02X%02X%02X%02X", p[0], p[1], p[2], p[3], p[4], p[5]);
                }
            } else {
                stringY += SPACING_Y;
                stringY =             UI_DrawString(stringX, stringY, 0x400000, " Application Manager Fail");
                stringY =             UI_DrawString(stringX, stringY, 0x400000, "  Could be due to no info.");
            }
        } break;
        case 1: { // memory
            if (tabDirty){ // update memory section
                UpdateProcessMemorySections(&targetProcess);
            }

            UI_DrawPanel(memoryScroll.x, memoryScroll.y, memoryScroll.w, memoryScroll.h, true);
            // calculate scroll
            if (memoryScroll.scrollY < 0){
                memoryScroll.scrollY = 0;
            } else {
                s32 scrollMax = 8; // add 8 for padding
                // measure intended size, not the best way of doing this
                for (int i = 0; i < MAX_SECTIONS; i++) {
                    MemSectionInfo curInfo;
                    if (i == 4) scrollMax += (SPACING_Y * 1);
                    curInfo = targetProcess.sections[i];
                    if (curInfo.start == 0 && curInfo.size == 0) continue;
                    scrollMax += (SPACING_Y * 3);
                    if (!curInfo.failed) {
                        scrollMax += (SPACING_Y * 1);
                    }
                }
                if (memoryScroll.scrollY > scrollMax - memoryScroll.h){
                    memoryScroll.scrollY = scrollMax - memoryScroll.h;
                }
            }
            
            UI_SetClippingPlane(memoryScroll.x + 1, memoryScroll.y + 1, memoryScroll.w - 2, memoryScroll.h - 2);
        
            s32 posX = (s32)(memoryScroll.x + 4);
            s32 posY = (s32)(memoryScroll.y + 4) - memoryScroll.scrollY;

            targetMemory.start = 0;
            targetMemory.size = 0;
            targetMemory.active = false;
            // draw sections
            for (int i = 0; i < MAX_SECTIONS; i++) {

                MemSectionInfo curInfo;

                if (i == 4) posY += (SPACING_Y * 1);
                curInfo = targetProcess.sections[i];
                if (curInfo.start == 0 && curInfo.size == 0) continue;
                
                if (i < 4){
                    UI_DrawStringFormat(posX, posY, 0x000000, curInfo.failed ? string5 : string4, defaultSectionNames[i], curInfo.start, curInfo.size);
                } else {
                    UI_DrawStringFormat(posX, posY, 0x000000, curInfo.failed ? string3 : string1, i - 3, curInfo.start, curInfo.size);
                }
                posY += (SPACING_Y * 3);  
                
                if (!curInfo.failed) {
                    char permBuffer[4];
                    _formatMemoryPermission(permBuffer, curInfo.perm);
                    char stateBuffer[12];
                    u32 colour = 0;
                    _formatUserMemoryState(stateBuffer, curInfo.state, &colour);
                    UI_DrawStringFormat(posX, posY, 0x000000, string2, permBuffer, colour, stateBuffer);
                    posY += (SPACING_Y * 1);
                    if (targetProcess.debugging && curInfo.readable) { 
                        memButtons[i].x = posX + (17 * SPACING_X - memButtons[i].w);
                        memButtons[i].y = posY - (SPACING_Y * 4);
                        if (UI_UpdateButton(&memButtons[i])){
                            targetMemory.start = curInfo.start;
                            targetMemory.size = curInfo.size;
                            targetMemory.active = true;
                            break; // only breaks out of for loop
                        }
                        UI_DrawButton(&memButtons[i]);
                    }
                }
            } // end of for loop

            UI_UpdateScrollZone(&memoryScroll); // update scroll after incase pressing a button inside scroll

            // display mismatching sections
            s32 stringY = PADDING + 20 + PADDING;
            u32 stringX = memoryScroll.x + memoryScroll.w + PADDING;
            if (targetProcess.MismatchSections.textStartMismatch)   stringY = UI_DrawString(stringX, stringY, 0xFF0000, "text.start mismatch");
            if (targetProcess.MismatchSections.textSizeMismatch)    stringY = UI_DrawString(stringX, stringY, 0xFF0000, "text.size mismatch");
            if (targetProcess.MismatchSections.rodataStartMismatch) stringY = UI_DrawString(stringX, stringY, 0xFF0000, "rodata.start mismatch");
            if (targetProcess.MismatchSections.rodataSizeMismatch)  stringY = UI_DrawString(stringX, stringY, 0xFF0000, "rodata.size mismatch");
            if (targetProcess.MismatchSections.dataStartMismatch)   stringY = UI_DrawString(stringX, stringY, 0xFF0000, "data.start mismatch");
            if (targetProcess.MismatchSections.dataSizeMismatch)    stringY = UI_DrawString(stringX, stringY, 0xFF0000, "data.size mismatch");    
            
            UI_ClearClippingPlane(); // clear clipping plane from scroll area
            
            if (targetProcess.debugging){
                s32 totalPadding = ((s32)dumpMemoryStartButton.h - SPACING_Y);
                
                s32 btnX = dumpMemoryStartButton.x + 4 + dumpMemoryStartButton.textOffsetX;
                s32 btnY = dumpMemoryStartButton.y + (totalPadding / 2) + 1 + dumpMemoryStartButton.textOffsetY;
                UI_DrawStringFormat(memoryScroll.x + memoryScroll.w + PADDING, btnY, 0x000000, "Dump Start");
                dumpMemoryStartButton.str = "";
                MENU_DO_BUTTON(dumpMemoryStartButton){
                    char userInput[64];
                    memset(userInput, 0, sizeof(userInput));
                    if (UI_GetUserInput("Input dump start ADDRESS (32 bit address)\n Rounded downwards to the page", 64, userInput)){
                        u32 address = 0;
                        if (HexStringToNum(userInput, &address)) {
                            dumpMemoryStart = PAGE_ROUND(address);
                        } else UI_DisplayMessageFormat("Invalid address \"%s\" (%d)", userInput, address);
                    }
                }
                UI_DrawStringFormat(btnX, btnY, 0x000000, "0x%08X", dumpMemoryStart);

                btnX = dumpMemoryEndButton.x + 4 + dumpMemoryEndButton.textOffsetX;
                btnY = dumpMemoryEndButton.y + (totalPadding / 2) + 1 + dumpMemoryEndButton.textOffsetY;
                UI_DrawStringFormat(memoryScroll.x + memoryScroll.w + PADDING, btnY, 0x000000, "Dump End");
                dumpMemoryEndButton.str = "";
                MENU_DO_BUTTON(dumpMemoryEndButton){
                    char userInput[64];
                    memset(userInput, 0, sizeof(userInput));
                    if (UI_GetUserInput("Input dump end ADDRESS (32 bit address)\n Rounded downwards to the page", 64, userInput)){
                        u32 address = 0;
                        if (HexStringToNum(userInput, &address)) {
                            dumpMemoryEnd = PAGE_ROUND(address);
                        } else UI_DisplayMessageFormat("Invalid address \"%s\" (%d)", userInput, address);
                    }
                }
                UI_DrawStringFormat(btnX, btnY, 0x000000, "0x%08X", dumpMemoryEnd);
                
                MENU_DO_BUTTON(dumpMemoryButton){
                    if (dumpMemoryStart > dumpMemoryEnd){
                        UI_DisplayMessage("Dump start can not be past dump end");
                        goto cancelDoMemDump;
                    }
                    u32 size = dumpMemoryEnd - dumpMemoryStart;
                    if (size == 0){
                        UI_DisplayMessage("Dump start can not be the same as dump end");
                        goto cancelDoMemDump;
                    }
                    if (UI_CheckWithUserFormat("No", "Yes", "Are you sure you want to dump this memory?\nTotal size: (0x%08X)\n%d bytes or %d KB or %d MB\nMake sure you have enough room on the SD card.\nUnreadable sections will be filled with 0s",
                        size, size, (u32)(((double)size) / 1000), (u32)(((double)size) / 1000000))
                    ){
                        Handle file = 0;
                        FS_Archive archive;
                        u32 dumpStart = dumpMemoryStart;
                        u32 dumpEnd = dumpMemoryEnd;
                        if (!ensureDirectory("/luma/ctn")){
                            UI_DisplayMessage("Failed to create /luma/ctn");
                            goto dumpMemoryEnd;
                        }
                        if (!ensureDirectory("/luma/ctn/dump")){
                            UI_DisplayMessage("Failed to create /luma/ctn/dump");
                            goto dumpMemoryEnd;
                        }
                        if (!openArchive(&archive)){
                            UI_DisplayMessage("Failed to open archive");
                            goto dumpMemoryEnd;
                        }
                        char fileName[128];
                        // time is in file name so we dont need to check if the file already exists (although we should anyways)
                        u64 time = osGetTime();
                        sprintf(fileName, "/luma/ctn/dump/%.8s_0x%08lX_0x%08lX_%lld.bin", targetProcess.name, dumpStart, dumpEnd, time); // max length should be 72 (68 if we instead use hex for the time)
                        if (!openCreateFile(archive, &file, fileName)){
                            UI_DisplayMessage("Failed to create file");
                            goto dumpMemoryEnd;
                        }

                        u32 writtenBytes = 0;
                        u32 dumpAddr = dumpStart;
                        u32 fileOffset = 0;
                        Process *proc = &targetProcess;
                        u32 i = 65536; // counter so we dont fill the SD card with data if nothing fails (this will max at size 0x10000000, around 268MB)
                        u64 readStart = svcGetSystemTick();
                        u64 totalLoopTime = svcGetSystemTick();
                        u32 writeEnd = 0;
                        bool displayUpdate = true;
                        // draw this before so we dont redraw all this every update
                        UI_FillRect(0, 0, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT, 0x000000); // fill black
                        UI_DrawStringFormat(2, 2, 0xFF0000, "\n\n\n\nPress B to abort\nPress A to toggle live update (dumps faster when off)"); // draw under live update stuff
                        while(i--){
                            u64 readLoopStart = svcGetSystemTick();
                            // check abort
                            UI_UpdatePressedInput();
                            if (UI_GetPressedButtons() & KEY_B){
                                writeEnd = 2;
                                break;
                            }
                            if (UI_GetPressedButtons() & KEY_A){
                                displayUpdate = !displayUpdate;
                                if (!displayUpdate){
                                    UI_FillRect(0, 0, 20 * SPACING_X, (SPACING_Y * 3) + 4, 0x000000); // draw black to cover other text
                                    UI_DrawString(2, 2, 0xFF0000, "Live Off...\nDumping...");
                                    UI_FlushScreens();
                                }
                            }
                            // read memory
                            bool readRes = ReadMemory(proc, dumpAddr, dataBuffer, PAGE_SIZE);
                            if (!readRes) memset(dataBuffer, 0, PAGE_SIZE);
                            u32 written = 0;
                            // write memory
                            bool good = writeFile(file, fileOffset, dataBuffer, PAGE_SIZE, &written);
                            writtenBytes += written;
                            if (!good){
                                writeEnd = 1;
                                break;
                            }
                            if (displayUpdate){
                                // display stuff to screen
                                u64 readLoopEnd = (svcGetSystemTick() - readLoopStart) / TICKS_TO_MS;
                                UI_FillRect(0, 0, 20 * SPACING_X, (SPACING_Y * 3) + 4, 0x000000); // draw black to cover other text
                                UI_DrawStringFormat(2, 2, 0xFF0000, "0x%08X\n(%llums)\n(%llums)", dumpAddr, readLoopEnd, (svcGetSystemTick() - totalLoopTime) / TICKS_TO_MS);
                                totalLoopTime = svcGetSystemTick();
                                UI_FlushScreens();
                            }
                            fileOffset += written;
                            dumpAddr += PAGE_SIZE;
                            if (dumpAddr >= dumpEnd) break;
                            
                        }

                        UI_FillRect(0, 0, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT, 0x000000); // fill black
                        if (writeEnd == 0){
                            UI_DisplayMessageFormat("Write complete\nfrom 0x%08lX to 0x%08lX\nwrote %d bytes\ntime: %llums", dumpStart, dumpAddr, writtenBytes, (svcGetSystemTick() - readStart) / TICKS_TO_MS);
                        } else if (writeEnd == 1) {
                            UI_DisplayMessageFormat("Write fail\nfrom 0x%08lX to 0x%08lX\nwrote %d bytes\ntime: %llums", dumpStart, dumpAddr, writtenBytes, (svcGetSystemTick() - readStart) / TICKS_TO_MS);
                        } else if (writeEnd == 2) {
                            UI_DisplayMessageFormat("Write abort\nfrom 0x%08lX to 0x%08lX\nwrote %d bytes\ntime: %llums", dumpStart, dumpAddr, writtenBytes, (svcGetSystemTick() - readStart) / TICKS_TO_MS);
                        }

                        dumpMemoryEnd:
                        closeFile(file);
                        closeArchive(archive);
                    }
                    cancelDoMemDump:
                }
                MENU_DO_BUTTON(writeMemoryButton){
                    char userInput[64];
                    memset(userInput, 0, sizeof(userInput));
                    if (!UI_GetUserInput("Input ADDRESS (32 bit address)", 64, userInput)) goto endWriteMemory;
                    u32 address = 0;
                    if (!HexStringToNum(userInput, &address)) {
                        UI_DisplayMessageFormat("Invalid address \"%s\" (%d)", userInput, address);
                        goto endWriteMemory;
                    }
                    memset(userInput, 0, sizeof(userInput)); // clear before next use
                    if (!UI_GetUserInput("Input VALUE (pared as bytes from right to left)\n placed sequential from 'address' to 'address + N' ", 64, userInput)){
                        goto endWriteMemory;
                    }
                    u32 len = strlen(userInput);
                    if (len < 2 || len % 2 == 1) {
                        UI_DisplayMessageFormat("Invalid value \"%s\"", userInput);
                        goto endWriteMemory;
                    }
                    u8 buffer[64]; // 64 is more then enough, the user input can only have 31 in total
                    u32 j = 0;
                    u32 byte = 0;
                    bool parseFail = false;
                    for (u32 i = 0; i < len; i += 2) {
                        if (!HexStringToNumLimit((userInput + i), 2, &byte)){ // parse 2 chars at a time
                            parseFail = true;
                            UI_DisplayMessageFormat("Failed value \"%s\"", (userInput + i));
                            break;
                        }
                        buffer[j++] = byte & 0xFF;
                    }
                    if (parseFail) goto endWriteMemory;
                    
                    char displayBuffer[128]; // max should be (j * 3 + 1) so 128 is more then enough
                    memset(displayBuffer, 0, sizeof(displayBuffer));
                    for (u32 i = 0; i < j; i++) {
                        sprintf(displayBuffer + (i * 3), "%02X ", buffer[i]);
                    }
                    
                    if (UI_DisplayMessageFormat("Writing:\n%s\nat 0x%08X\n\nPress B to cancel.\nPress A to write.", displayBuffer, address)){
                        if (!WriteMemory(&targetProcess, address, buffer, j)){
                            UI_DisplayMessageFormat("Failed to write %d bytes memory at 0x%08X", j, address);
                        }
                    } else UI_DisplayMessage("Writing canceled");

                    endWriteMemory:
                }
                MENU_DO_BUTTON(allocateCodeCave){
                    u32 addr;
                    if (!AllocateCodeCave(&targetProcess, PAGE_SIZE, &addr)){
                        UI_DisplayMessage("Failed to allocate code cave");
                    } else {
                        UI_DisplayMessageFormat("Code cave at %08lX\nRemember this, it will not be saved\nand can not be undone.", addr);
                    }
                }
            }

        } break;
        case 2: { // Debug
            UI_DrawString(innerX + PADDING, innerY + PADDING, 0x000000, "Debug Events");
            UI_DrawPanel(debugMessageScroll.x, debugMessageScroll.y, debugMessageScroll.w, debugMessageScroll.h, true);

            // calculate scroll
            if (debugMessageScroll.scrollY < 0){
                debugMessageScroll.scrollY = 0;
            } else {
                if (targetProcess.debugging){
                    s32 maxScroll = 8;
                    char debugMessage[64];
                    u32 debugEventCount = 0;
                    const DebugEventInfo *debugEvents = GetDebugEvents(&debugEventCount);
                    for (u32 i = 0; i < debugEventCount; i++) {
                        u32 line = formatDebugEvent(&debugEvents[i], debugMessage);
                        maxScroll += (SPACING_Y * line);
                    }
                    if ((u32)maxScroll > debugMessageScroll.h){
                        if (debugMessageScroll.scrollY > maxScroll - debugMessageScroll.h){
                            debugMessageScroll.scrollY = maxScroll - debugMessageScroll.h;
                        }
                    } else {
                        debugMessageScroll.scrollY = 0;
                    }
                } else {
                    debugMessageScroll.scrollY = 0;
                }
            }

            MENU_DO_BUTTON(toggleDebuggingButton){
                if (!targetProcess.debugging){
                    if (StartProcessDebugThread(&targetProcess)){
                        debugWindow->active = true; // once set to true the window will automatically disable itself
                    } else UI_DisplayMessage("Fail to start debugging thread.");
                } else {
                    StopProcessDebugThread(0);
                }
            };
            toggleDebuggingButton.str = ShouldProcessDebugThreadBeRunning() ? 
                (IsProcessDebugThreadRunning() ? "Stop Debug" : "Starting") :
                (IsProcessDebugThreadRunning() ? "Stopping" : "Start Debug");

            if (targetProcess.debugging){
                u32 stringX = debugMessageScroll.x + PADDING;
                s32 stringY = (s32)(debugMessageScroll.y + PADDING) - debugMessageScroll.scrollY;
                MENU_DO_BUTTON(continueDebuggerButton){
                    if (!SendDebugThreadCommandWait(DEBUG_COMMAND_EVENT_CONTINUE, 50)){ // max wait for 50ms
                        UI_DisplayMessage("Failed to send CONTINUE command to thread.");
                    }
                };

                // the debug thread already does this
                // SendDebugThreadCommandWait(DEBUG_COMMAND_EVENT_READ, 0);
                
                UI_UpdateScrollZone(&debugMessageScroll);

                // clip the scrolling
                UI_SetClippingPlane(debugMessageScroll.x + 1, debugMessageScroll.y + 1, debugMessageScroll.w - 2, debugMessageScroll.h - 2);
                u32 debugEventCount = 0;
                const DebugEventInfo *debugEvents = GetDebugEvents(&debugEventCount);
                bool blocking = IsBlockingDebugEvent();
                for (u32 i = 0; i < debugEventCount; i++) {
                    char debugMessage[64];
                    u32 line = formatDebugEvent(&debugEvents[i], debugMessage);
                    u32 colour = (blocking && i == debugEventCount - 1 && debugEvents[i].flags & 1) ? 0x7F0000 : 0x000000;
                    UI_DrawStringFormat(stringX, stringY, colour, "[%X]th%d: %s ", debugEvents[i].flags, debugEvents[i].thread_id, debugMessage);
                    stringY += (SPACING_Y * line);
                }
                UI_ClearClippingPlane();
                
            }
        
        } break;
        case 3: { // Thread (debug only)
            if (!targetProcess.debugging) { tab.selectedTab = 2; break; }

            UI_DrawString(innerX + PADDING, innerY + PADDING, 0x000000, "Thread IDs");
            UI_DrawPanel(threadListScroll.x, threadListScroll.y, threadListScroll.w, threadListScroll.h, true);

            // calculate scroll
            if (threadListScroll.scrollY < 0){
                threadListScroll.scrollY = 0;
            } else {
                s32 scrollMax = 8 + (targetProcess.Debug.threadIndex * SPACING_Y); // add 8 for padding
                if ((u32)scrollMax > debugMessageScroll.h){
                    if (threadListScroll.scrollY > scrollMax - threadListScroll.h){
                        threadListScroll.scrollY = scrollMax - threadListScroll.h;
                    }
                } else {
                    threadListScroll.scrollY = 0;
                }
            }
            
            u32 stringX = threadListScroll.x + PADDING;
            s32 stringY = (s32)(threadListScroll.y + PADDING) - threadListScroll.scrollY;

            // clip the scrolling
            UI_SetClippingPlane(threadListScroll.x + 1, threadListScroll.y + 1, threadListScroll.w - 2, threadListScroll.h - 2);

            for (u32 i = 0; i < targetProcess.Debug.threadIndex; i++) {
                DebugThread t = targetProcess.Debug.threads[i];
                ThreadContext ctx;
                // svcGetDebugThreadContext should really be called when the program is stoped but rosalina pauses it so its fine
                if (R_SUCCEEDED(svcGetDebugThreadContext(&ctx, targetProcess.debugHandle, t.threadId, THREADCONTEXT_CONTROL_CPU_REGS)))
                    t.readable = true;
                else t.readable = false;
                UI_DrawStringFormat(stringX, stringY, targetProcess.Debug.selectedThread == t.threadId ? 0x007F00 : (t.readable ? 0x000000 : 0xF70000), "%d (%d)", t.threadId, t.threadPriority);
                threadButtons[i].x = stringX + (10 * SPACING_X);
                threadButtons[i].y = stringY;
                stringY += SPACING_Y;
                if (t.readable) MENU_DO_BUTTON(threadButtons[i]){
                    targetProcess.Debug.selectedThread = t.threadId;
                }
            }
            
            UI_ClearClippingPlane();

            UI_UpdateScrollZone(&threadListScroll); // update after inner buttons so those buttons get updated

            stringX = threadListScroll.x + threadListScroll.w + PADDING;
            u32 startX = stringX;
            stringY = innerY + PADDING;
            UI_DrawStringFormat(stringX, stringY, 0x000000, "Registers (%d)", targetProcess.Debug.selectedThread);
            stringY += SPACING_Y;
            
            stringX += PADDING;
            ThreadContext ctx;
            if (R_FAILED(svcGetDebugThreadContext(&ctx, targetProcess.debugHandle, targetProcess.Debug.selectedThread, THREADCONTEXT_CONTROL_CPU_REGS))){
                UI_DrawString(stringX, stringY, 0xFF0000, "Failed");
            } else {
                for (size_t i = 0; i < 10; i++) {
                    UI_DrawStringFormat(stringX, stringY, 0x000000, "r%d: 0x%lX", i, ctx.cpu_registers.r[i]);
                    stringY += SPACING_Y;
                }
                stringX = startX + (16 * SPACING_X);
                stringY = innerY + PADDING + SPACING_Y;
                for (size_t i = 10; i < 13; i++) {
                    UI_DrawStringFormat(stringX, stringY, 0x000000, "r%d: 0x%lX", i, ctx.cpu_registers.r[i]);
                    stringY += SPACING_Y;
                }
                UI_DrawStringFormat(stringX, stringY, 0x000000, "sp: 0x%lX", ctx.cpu_registers.sp);
                stringY += SPACING_Y;
                UI_DrawStringFormat(stringX, stringY, 0x000000, "lr: 0x%08lX", ctx.cpu_registers.lr);
                stringY += SPACING_Y;
                UI_DrawStringFormat(stringX, stringY, 0x000000, "pc: 0x%08lX", ctx.cpu_registers.pc);
                stringY += SPACING_Y;
                // https://developer.arm.com/documentation/ddi0601/2021-12/AArch32-Registers/CPSR--Current-Program-Status-Register?lang=en
                UI_DrawStringFormat(stringX, stringY, 0x000000, "cpsr: 0x%lX", ctx.cpu_registers.cpsr);
                stringY += SPACING_Y;
                stringX += PADDING;
                UI_DrawString(stringX, stringY, 0x000000, "NZCVQ");
                UI_DrawString(stringX + (7 * SPACING_X), stringY, 0x000000, ctx.cpu_registers.cpsr & 0x20 ? "THUMB" : "ARM");
                stringY += SPACING_Y;
                UI_DrawStringFormat(stringX, stringY, 0x000000, "%d%d%d%d%d", (ctx.cpu_registers.cpsr >> 31) & 1, (ctx.cpu_registers.cpsr >> 30) & 1, (ctx.cpu_registers.cpsr >> 29) & 1, (ctx.cpu_registers.cpsr >> 28) & 1, (ctx.cpu_registers.cpsr >> 27) & 1);
                stringY += SPACING_Y;
                switch (ctx.cpu_registers.cpsr & 0xF) {
                case 0b0000: UI_DrawString(stringX, stringY, 0x000000, "User"); break;
                case 0b0001: UI_DrawString(stringX, stringY, 0x000000, "FIQ"); break;
                case 0b0010: UI_DrawString(stringX, stringY, 0x000000, "IRQ"); break;
                case 0b0011: UI_DrawString(stringX, stringY, 0x000000, "Supervisor"); break;
                case 0b0110: UI_DrawString(stringX, stringY, 0x000000, "Monitor"); break;
                case 0b0111: UI_DrawString(stringX, stringY, 0x000000, "Abort"); break;
                case 0b1010: UI_DrawString(stringX, stringY, 0x000000, "Hyp"); break;
                case 0b1011: UI_DrawString(stringX, stringY, 0x000000, "Undefined"); break;
                case 0b1111: UI_DrawString(stringX, stringY, 0x000000, "System"); break;
                default: UI_DrawString(stringX, stringY, 0xFF0000, "Unknown "); break;
                }
                stringY += SPACING_Y;
            }

        } break;
        case 4: { // breaks (debug only)
            UI_DrawString(innerX + PADDING, innerY + PADDING, 0x000000, "Breakpoints");
            UI_DrawString(innerX + PADDING + (13 * SPACING_X), innerY + PADDING, 0xFF0000, "THUMB breakpoints not implemented.\n");
            UI_DrawString(breakListScroll.x + breakListScroll.w + PADDING, innerY + PADDING + SPACING_Y, 0xFF0000, "Breakpoints override memory\n do not write to brkpnt address.");
            UI_DrawPanel(breakListScroll.x, breakListScroll.y, breakListScroll.w, breakListScroll.h, true);

            // calculate scroll
            breakListScroll.scrollY = 0;
            s32 maxScroll = 8 + (targetProcess.Debug.breakpointCount * SPACING_Y);
            if ((u32)maxScroll > breakListScroll.h){
                if (breakListScroll.scrollY > maxScroll - breakListScroll.h){
                    breakListScroll.scrollY = maxScroll - breakListScroll.h;
                }
            }

            UI_SetClippingPlane(breakListScroll.x + 1, breakListScroll.y + 1, breakListScroll.w - 2, breakListScroll.h - 2);

            u32 stringX = breakListScroll.x + PADDING;
            s32 stringY = (s32)(breakListScroll.y + PADDING) - breakListScroll.scrollY;
            for (u32 i = 0; i < targetProcess.Debug.breakpointCount; i++) {
                Breakpoint t = targetProcess.Debug.breakpoints[i];
                UI_DrawStringFormat(stringX, stringY, 0x000000, "0x%08X", t.address);
                if (breakpointRemoving){
                    breakpointButtons[i].textColour = 0xFF0000;
                    breakpointButtons[i].x = stringX + (10 * SPACING_X) + 2;
                    breakpointButtons[i].y = stringY - 1;
                    MENU_DO_BUTTON(breakpointButtons[i]){
                        bool found = false;
                        for (u32 i = 0; i < targetProcess.Debug.breakpointCount; i++) {
                            Breakpoint *b = &targetProcess.Debug.breakpoints[i];
                            if (!found && b->address == t.address){
                                WriteMemory(&targetProcess, t.address, b->originalValue, 4);
                                found = true;
                            } else if (found){
                                targetProcess.Debug.breakpoints[i - 1] = targetProcess.Debug.breakpoints[i];
                            }
                        }
                        targetProcess.Debug.breakpointCount--;
                        break; // quit out of loop
                    }
                }
                stringY += SPACING_Y;
            }

            UI_ClearClippingPlane();

            UI_UpdateScrollZone(&breakListScroll); // update after inner buttons so those buttons get updated

            MENU_DO_BUTTON(addBreakpointButton){
                if (targetProcess.Debug.breakpointCount == MAX_BREAKPOINTS){
                    UI_DisplayMessage("Max number of breakpoints reached");
                    goto endOfAddBreak;
                }
                char userInput[64];
                memset(userInput, 0, sizeof(userInput));
                if (!UI_GetUserInput("Input breakpoint ADDRESS (32 bit address)\n THUMB is not implemented", 64, userInput)) goto endOfAddBreak;
                u32 address = 0;
                if (!HexStringToNum(userInput, &address)) {
                    UI_DisplayMessageFormat("Invalid address \"%s\" (%d)", userInput, address);
                    goto endOfAddBreak;
                }

                for (u32 i = 0; i < targetProcess.Debug.breakpointCount; i++) {
                    if (targetProcess.Debug.breakpoints[i].address == address){
                        UI_DisplayMessageFormat("Breakpoint at 0x%08X already exists", address);
                        goto endOfAddBreak;
                    }
                }
                
                u8 buffer[4];
                if (ReadMemory(&targetProcess, address, buffer, 4)){
                    if (WriteMemory(&targetProcess, address, breakpointValueBuffer, 4)){
                        Breakpoint* bk = &targetProcess.Debug.breakpoints[targetProcess.Debug.breakpointCount++];
                        bk->address = address;
                        memcpy(bk->originalValue, buffer, 4);
                    } else UI_DisplayMessageFormat("Failed to write memory at 0x%08X.", address);
                } else UI_DisplayMessageFormat("Failed to read memory at 0x%08X.", address);

                endOfAddBreak:
            } 
            if (breakpointRemoving){
                removeBreakpointButton.str = "Rmv Cancel";
                MENU_DO_BUTTON(removeBreakpointButton){
                    breakpointRemoving = false;
                }
            } else {
                removeBreakpointButton.str = "Rmv Break";
                MENU_DO_BUTTON(removeBreakpointButton){
                    breakpointRemoving = true;
                }
            }
            /*
            MENU_DO_BUTTON(removeBreakpointButton){
                if (targetProcess.Debug.breakpointCount == 0){
                    UI_DisplayMessage("No breakpoints to remove.");
                    goto endOfRemoveBreak;
                }
                char userInput[64];
                memset(userInput, 0, sizeof(userInput));
                if (!UI_GetUserInput("Input breakpoint ADDRESS (32 bit address)", 64, userInput)) goto endOfRemoveBreak;
                u32 address = 0;
                if (!HexStringToNum(userInput, &address)) {
                    UI_DisplayMessageFormat("Invalid address \"%s\" (%d)", userInput, address);
                    goto endOfRemoveBreak;
                }

                bool found = false;
                for (u32 i = 0; i < targetProcess.Debug.breakpointCount; i++) {
                    Breakpoint *b = &targetProcess.Debug.breakpoints[i];
                    if (!found && b->address == address){
                        WriteMemory(&targetProcess, address, b->originalValue, 4);
                        found = true;
                    } else if (found){
                        targetProcess.Debug.breakpoints[i - 1] = targetProcess.Debug.breakpoints[i];
                    }
                }
                if (!found){
                    UI_DisplayMessageFormat("No breakpoint at 0x%08X", address);
                    goto endOfRemoveBreak;
                }

                targetProcess.Debug.breakpointCount--;

                endOfRemoveBreak:
            }
            */

        } break;
        /*
        case 4: { // Threads (debug only)
            if (!targetProcess.debugging) { tab.selectedTab = 2; break; }

            u32 stringX = innerX + PADDING;
            u32 stringY = innerY + PADDING;
            _UI_DrawString(stringX, stringY, 0x000000, "Thread IDs");
            stringY += SPACING_Y;
            stringX += PADDING;

            u32 startStringY = stringY;
            u32 lc = 0; // line count
            for (u32 i = 0; i < targetProcess.Debug.threadIndex; i++) {
                DebugThread t = targetProcess.Debug.threads[i];
                _UI_DrawStringFormat(stringX, stringY, 0x000000, "%d (%d)", t.threadId, t.threadPriority);
    
                stringY += SPACING_Y;

                lc++;
                if (lc >= 12){
                    lc = 0;
                    stringX += (SPACING_X * 11); // hopefully enough "ABCD (XY)"
                    stringY = startStringY;
                }
            }

        } break;
        */
        default: { // error
            UI_DrawStringFormat(innerX + PADDING, innerY + PADDING, 0x000000, "Error page %d", tab.selectedTab);
        } break;
        } 

        tabDirty = false;

        if (targetMemory.active){
            MENU_CHANGE_MENU(ctn_Menu_ProcMemView);
            break;
        }
    
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();

        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
        DrawDefaultProcInfo();
    } MENU_LOOP_END();

    if (targetMemory.active){
        // dont do stuff to quit
    } else  {
        if (!targetProcess.locked && !targetProcess.debugging){
            CloseProcess(&targetProcess); // close if not debugging, we dont need to keep it open
        }
    }

    return MENU_RESULT_OK;
}

MenuResult ctn_Menu_ProcList(){
    static ProcessInfo procInfo[MAX_PROC_COUNT] = { 0 };
    static Button procButtons[MAX_PROC_COUNT] = { 0 };

    static Button backButton = { 0 };
    static Button updateButton = { 0 };

    MENU_ONCE_START{

        u32 posX = 2 * PADDING;
        u32 posY = 2 * PADDING + 20;    
        for (int i = 0; i < MAX_PROC_COUNT; i++) {
            procButtons[i] = UI_CreateButton(posX, posY, SPACING_X * 12, SPACING_Y, "\0", BASE_COLOUR, 0x000000);
            procButtons[i].textOffsetX = -3;
            procButtons[i].textOffsetY = -1;
            posY += procButtons[i].h;
            if ((i + 1) % 16 == 0){
                posX += procButtons[i].w;
                posY = 2 * PADDING + 20;

            }
        }
        backButton = UI_CreateButton(PADDING, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Back", BASE_COLOUR, 0x000000);
        updateButton = UI_CreateButton(BOTTOM_SCREEN_WIDTH - PADDING - 50, BOTTOM_SCREEN_HEIGHT - PADDING - 20, 50, 20, "Update", BASE_COLOUR, 0x000000);
        for (u32 i = 0; i < MAX_PROC_COUNT; i++) procInfo[i].pid = PID_NULL;
    } MENU_ONCE_END;
    
    bool startUpdate = true;
    s32 procCount = 0;

    u32 cardTitleId = 0;
    bool cardSuccess = false;
    bool cardInserted = false;
    FS_CardType cardType;
    if (R_SUCCEEDED(FSUSER_CardSlotIsInserted(&cardInserted)) && cardInserted && R_SUCCEEDED(FSUSER_GetCardType(&cardType))){
        if (R_SUCCEEDED(amInit())){
            u32 titleCount = 0;
            if (R_SUCCEEDED(AM_GetTitleCount(MEDIATYPE_GAME_CARD, &titleCount))){
                u64 titleIds[titleCount];
                u32 titlesRead = 0;
                if (R_SUCCEEDED(AM_GetTitleList(&titlesRead, MEDIATYPE_GAME_CARD, titleCount, titleIds))){
                    cardTitleId = titleIds[0] & 0x00000000FFFFFFFF;
                    cardSuccess = true;
                }
            }
            amExit();
        }
    }

    MENU_LOOP_START(); {
        MENU_DRAW_BACKGROUND();
        MENU_DRAW_TITLE("Process List");
        DefaultWindowStart();
        MENU_DO_BUTTON(backButton) MENU_CLOSE_MENU();
        MENU_BCLOSE;

        MENU_DO_BUTTON_OR(updateButton, startUpdate) {
            for (u32 i = 0; i < MAX_PROC_COUNT; i++) procInfo[i].pid = PID_NULL;
            procCount = enumerateProcesses(procInfo, MAX_PROC_COUNT);
            startUpdate = true;
        }

        if (cardInserted){
            if (cardSuccess) UI_DrawStringFormat(backButton.x + backButton.w + PADDING, backButton.y + 4, 0x000000, "Card: %016llX", cardTitleId);
            else UI_DrawString(backButton.x + backButton.w + PADDING, backButton.y + 4, 0x000000, "Card: fail");
        } else {
            UI_DrawStringFormat(backButton.x + backButton.w + PADDING, backButton.y + 4, 0x000000, "No Card Inserted");
        }

        targetProcessPid = PID_NULL;
        char buffer[32];
        for (int i = 0; i < procCount; i++) {
            memset(buffer, 0, sizeof(buffer)); // not sure whats faster, using memset on the buffer or redefining the buffer every loop
            sprintf(buffer, "%d-%s", i, procInfo[i].name);
            procButtons[i].str = buffer;
            procButtons[i].textColour = 0x000000;
            //if (cardSuccess && ((targetProcess.titleId & 0x00000000FFFFFFFF) == cardTitleId))
            //    procButtons[i].textColour = 0x00007F; // set dark blue if the current button is the card title
            if (targetProcess.active && (targetProcess.pid == procInfo[i].pid))
                procButtons[i].textColour = 0x007F00; // set green if the current button is the active process
            MENU_DO_BUTTON(procButtons[i]){
                if (targetProcess.active){
                    if (targetProcess.pid == procInfo[i].pid){
                        targetProcessPid = procInfo[i].pid; // make sure to set it so it actually goes to the next page
                        break;
                    } else {
                        UI_DisplayMessage("Can not open a programs while another is active.");
                    }
                } else {
                    targetProcessPid = procInfo[i].pid;
                    break;
                }
            }
        }
        if (targetProcessPid != PID_NULL){ // not null pid should mean its valid
            MENU_CHANGE_MENU(ctn_Menu_ProcView);
        }
        
        DrawDefaultUpperScreenStuff();
        DefaultWindowEnd();
    } MENU_LOOP_END();

    return MENU_RESULT_OK;
}