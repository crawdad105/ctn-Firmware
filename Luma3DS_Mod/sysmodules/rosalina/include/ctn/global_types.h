#pragma once

#include <stdbool.h>
#include <stdint.h>


typedef struct UTouchPoint UTouchPoint; // same as touchPosition 
struct UTouchPoint { // 4 bytes
    uint16_t x;
    uint16_t y;
};

typedef struct STouchPoint STouchPoint; // same as touchPosition but signed
struct STouchPoint { // 4 bytes
    int16_t x;
    int16_t y;
};

typedef struct Button Button; // 30 (32)
struct Button{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    int16_t textOffsetX;
    int16_t textOffsetY;

    union {
        struct {
            uint16_t state_pressed : 1;
            uint16_t state_onDown : 1;
            uint16_t state_onUp : 1;
        };
        uint16_t rawState;
    };

    uint32_t colour;
    uint32_t textColour;
    
    char* str;
};

typedef struct ScrollZone ScrollZone;
struct ScrollZone{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;

    int32_t scrollX;
    int32_t scrollY;

    union {
        struct {
            uint16_t state_scrolling : 1;
        };
        uint16_t rawState;
    };

};

typedef struct CheckBox CheckBox;
struct CheckBox {
    int16_t x;
    int16_t y;
    uint16_t size;
    uint32_t colour;
    uint32_t textColour;
    uint8_t checked;
    char* checkChar;

    int16_t charOffsetX;
    int16_t charOffsetY;
};

typedef struct Window Window;
struct Window{
    char* title;
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    
    union {
        struct {
            uint8_t dragging : 1;
            uint8_t mouseDown : 1;
            uint8_t mouseUp : 1;
            uint8_t pressed : 1;
            uint8_t selected : 1;  // not implemented
            uint8_t visible : 1;   // user set, if set then the window will be drawn, updating still depends on "active"
            uint8_t resizable : 1; // user implemented
            uint8_t minimized : 1;
        };
        uint8_t rawState;
    };
    
    Button button;

    // if active the window will be updated, drawing still depends on "visible"
    bool active;
    
    void (*OnDraw)(Window *window);
    void (*OnUpdate)(Window *window);

};

#define MAX_TAB_BUTTON 8
typedef struct TabPanel TabPanel;
struct TabPanel{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t btnWidth;
    uint16_t btnHeight;
    uint8_t selectedTab;
    uint8_t tabCount; // between 1 and 8, can be modified to hide and show tabs
    Button tabButtons[MAX_TAB_BUTTON];
};
