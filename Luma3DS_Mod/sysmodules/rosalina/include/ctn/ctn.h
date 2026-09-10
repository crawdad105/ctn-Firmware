#pragma once

#include <stdbool.h>

extern bool DEBUG_MODE;
extern bool isctnMenuOpen;
extern bool rosalinaMod_ShouldOpenCustomMenu;
extern bool StartOpenDebuggerMenu;

// 0.1: original "CDR"
// 0.2: "TMod"
// 0.2.1: "cdad mod"
// 0.2.2: "ctn Firmware"
#define PROGRAM_VERSION "0.2.2"

// UI stuff
#define PADDING 5
#define BASE_COLOUR 0xC0C0C0 // gray
#define THEME_COLOUR 0x716CCD // purpleish-blue (same as cModLoader)

// memory
#define PAGE_SIZE 0x1000

// other
#define TICKS_TO_MS 1000000LL
#define TICKS_PER_SECOND 268123480ULL
#define SECOND_IN_NANOSECOND 1000000000ULL
#define TARGET_NANOSECOND (u64)(SECOND_IN_NANOSECOND / 60)
