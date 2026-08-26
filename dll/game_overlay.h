#pragma once

// In-game overlay (Dear ImGui over the game's D3D11 swap chain, ANGLE backend).
// Dormant until Overlay_Start() succeeds; everything renders inside the game's
// own frame - no second window anywhere.

#include <cstdint>

namespace Overlay {

bool Start();          // capture swap-chain vtable + hook Present + WndProc
void Stop();
bool Running();
void SetVisible(bool v);
bool Visible();
int  DisplayWidth();   // swapchain size, cached each frame (0 before boot)
int  DisplayHeight();

// ---- Declarative widget store (written on script thread, drawn on render thread) ----
constexpr int MAX_WIDGETS = 96;
constexpr int MAX_WINDOWS = 5;
constexpr int MAX_EVENTS = 64;

struct Widget {
    enum Type { LABEL, BUTTON, TOGGLE, SLIDER, RADAR,
                SECTION, DROPDOWN, KEYBIND,
                INPUT, COLOR, SEPARATOR, SAMELINE, PROGRESS, COMBO,
                LISTBOX, TEXTCOLORED, SMALLBUTTON,
                PAGETAB } type = LABEL;
    char label[64] = {};     // title / text / paragraph body
    char desc[56] = {};      // secondary line under toggle titles etc.
    unsigned char flags = 0; // bit1: LABEL=paragraph-card, BUTTON=accent-colored
    bool  bval = false;      // toggle state | section open | keybind waiting
    float fval = 0.f;        // slider value | radar range | keybind vk | combo/list idx | progress
    float fmin = 0.f, fmax = 1.f;  // slider bounds | radar radius/style
    int   cbRef = 0;         // Lua registry ref for callbacks (0 = none)
    char  opts[8][16] = {};  // dropdown/combo/listbox option texts (up to 8)
    unsigned char optCount = 0;
    int   valIdx = 0;        // dropdown selected index
    char  text[64] = {};     // INPUT value buffer
    float col[4] = {};       // COLOR / TEXTCOLORED r,g,b,a (0..1) | accent button bg
};

struct Window {
    char title[48] = {};
    bool visible = true;
    int  count = 0;
    Widget widgets[MAX_WIDGETS];
};

Window*       LockWindows(int* outCount);   // script thread: add/edit
void          UnlockWindows();
void          SetWindowCount(int n);
void          SetAccent(float r, float g, float b);
const Window* LockWindowsRO();              // render thread: draw
void          UnlockWindowsRO();

// Event queue: render thread pushes widget activations, script thread drains.
void PushEvent(int windowIdx, int widgetIdx);
// Drain up to one event; returns false when empty. (called on script thread)
bool PopEvent(int* windowIdx, int* widgetIdx);

} // namespace Overlay
