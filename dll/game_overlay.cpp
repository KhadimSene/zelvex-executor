// Zelvex in-game overlay: Dear ImGui rendered inside the game's own D3D11
// swap chain (the game uses ANGLE -> D3D11, so hooking IDXGISwapChain::Present
// covers every frame). Hook = shared vtable page swap; copy-on-write makes it
// process-local. No code patching, no length disassembly. INSERT toggles menu.
#include "game_overlay.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstring>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {

typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(IDXGISwapChain*, UINT, UINT);

// MinGW-safe interface GUIDs (avoid __uuidof / libuuid dependency)
static const GUID kIID_ID3D11Device = {
    0xdb6f6ddb, 0xac77, 0x4e88, {0x82,0x53,0x81,0x9d,0xf9,0xbb,0xf1,0x40}};
static const GUID kIID_ID3D11Texture2D = {
    0x6f15aaf2, 0xd208, 0x4e89, {0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};

static HWND                 g_gameWnd     = nullptr;
static WNDPROC              g_origWndProc = nullptr;
static void**               g_vtable      = nullptr;
static PFN_Present          g_realPresent = nullptr;
static bool                 g_running     = false;
static volatile bool        g_visible     = true;
static bool                 g_imguiReady  = false;

static ID3D11Device*           g_dev = nullptr;
static ID3D11DeviceContext*    g_ctx = nullptr;
static IDXGISwapChain*         g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

// ---- declarative widget store ----
static SRWLOCK g_lock = SRWLOCK_INIT;
static Window  g_windows[MAX_WINDOWS];
static int     g_windowCount = 0;

struct Ev { short w, i; };
static Ev  g_events[MAX_EVENTS];
static volatile LONG g_evHead = 0;   // script thread consumes
static volatile LONG g_evTail = 0;   // render thread produces

Window* LockWindows(int* outCount) {
    AcquireSRWLockExclusive(&g_lock);
    if (outCount) *outCount = g_windowCount;
    return g_windows;
}
void UnlockWindows() { ReleaseSRWLockExclusive(&g_lock); }
void SetWindowCount(int n) {
    if (n < 0) n = 0;
    if (n > MAX_WINDOWS) n = MAX_WINDOWS;
    g_windowCount = n;
}

// Live accent recolor (gui.accent in Lua).
static float g_accent[3] = { 61.f / 255.f, 139.f / 255.f, 1.0f };
void SetAccent(float r, float g, float b) {
    g_accent[0] = r; g_accent[1] = g; g_accent[2] = b;
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4 main(r, g, b, 0.90f), hi(r * 1.25f > 1 ? 1 : r * 1.25f,
                                    g * 1.25f > 1 ? 1 : g * 1.25f,
                                    b * 1.25f > 1 ? 1 : b * 1.25f, 1.0f);
    s.Colors[ImGuiCol_Button]           = main;
    s.Colors[ImGuiCol_ButtonHovered]    = hi;
    s.Colors[ImGuiCol_CheckMark]        = hi;
    s.Colors[ImGuiCol_SliderGrab]       = main;
    s.Colors[ImGuiCol_SliderGrabActive] = hi;
    s.Colors[ImGuiCol_Separator]        = ImVec4(r, g, b, 0.30f);
    s.Colors[ImGuiCol_Border]           = ImVec4(r, g, b, 0.35f);
    s.Colors[ImGuiCol_Header]           = ImVec4(r, g, b, 0.45f);
    s.Colors[ImGuiCol_Text]             = ImVec4(0.92f, 0.94f, 0.97f, 1.0f);
}
const Window* LockWindowsRO() { AcquireSRWLockShared(&g_lock); return g_windows; }
void UnlockWindowsRO() { ReleaseSRWLockShared(&g_lock); }

void PushEvent(int wIdx, int wgt) {
    LONG t = InterlockedCompareExchange(&g_evTail, 0, 0);
    LONG h = InterlockedCompareExchange(&g_evHead, 0, 0);
    int used = (int)(t - h);
    if (used < 0) used += MAX_EVENTS;
    if (used >= MAX_EVENTS - 1) return;
    LONG slot = t % MAX_EVENTS;
    g_events[slot].w = (short)wIdx;
    g_events[slot].i = (short)wgt;
    InterlockedExchange(&g_evTail, t + 1);
}

bool PopEvent(int* wIdx, int* wgt) {
    LONG t = InterlockedCompareExchange(&g_evTail, 0, 0);
    LONG h = InterlockedCompareExchange(&g_evHead, 0, 0);
    if (h == t) return false;
    LONG slot = h % MAX_EVENTS;
    *wIdx = g_events[slot].w;
    *wgt = g_events[slot].i;
    InterlockedExchange(&g_evHead, h + 1);
    return true;
}

bool Running()  { return g_running; }
void SetVisible(bool v) { g_visible = v; }
bool Visible()  { return g_visible; }

// Swapchain size, refreshed every rendered frame (ESP math needs it).
static volatile LONG g_dispW = 0, g_dispH = 0;
int DisplayWidth()  { return InterlockedCompareExchange(&g_dispW, 0, 0); }
int DisplayHeight() { return InterlockedCompareExchange(&g_dispH, 0, 0); }

struct EnumCtx { DWORD pid; HWND best; int bestArea; };
static BOOL CALLBACK EnumProc(HWND h, LPARAM lp) {
    EnumCtx* c = (EnumCtx*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != c->pid || !IsWindowVisible(h)) return TRUE;
    if (GetWindow(h, GW_OWNER)) return TRUE;
    RECT r;
    if (!GetClientRect(h, &r)) return TRUE;
    int area = (r.right - r.left) * (r.bottom - r.top);
    if (area > c->bestArea) { c->bestArea = area; c->best = h; }
    return TRUE;
}

static HWND FindGameWindow() {
    EnumCtx c = { GetCurrentProcessId(), nullptr, 0 };
    EnumWindows(EnumProc, (LPARAM)&c);
    return c.best;
}

static bool IsMouseMsg(UINT m) {
    switch (m) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
    case WM_MOUSEMOVE:   case WM_MOUSEWHEEL:
        return true;
    }
    return false;
}
static bool IsKbMsg(UINT m) {
    switch (m) {
    case WM_KEYDOWN: case WM_KEYUP:
    case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR:
        return true;
    }
    return false;
}

// Coexistence model: the menu and the game UI live side by side.
//  - Messages over OUR panels -> ImGui consumes them.
//  - Everything else -> straight to the game, even with the menu open.
//  - INSERT toggles panel visibility; it never touches cursor state because
//    Mini World's own UI is cursor-driven.
static LRESULT CALLBACK HookWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_INSERT && !(lp & 0x40000000)) {
        g_visible = !g_visible;
        return 0;
    }
    if (!g_imguiReady)
        return CallWindowProc(g_origWndProc, h, msg, wp, lp);

    // Always let ImGui see everything first so its widgets update.
    LRESULT handled = ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp);
    ImGuiIO& io = ImGui::GetIO();

    if (IsMouseMsg(msg) && g_visible && io.WantCaptureMouse)
        return 1;                                    // hovering our panels
    if (msg == WM_INPUT && g_visible && io.WantCaptureMouse)
        return 1;                                    // freeze look on panels
    if (IsKbMsg(msg) && io.WantCaptureKeyboard)
        return 1;                                    // typing in a field

    // Game gets everything else - movement, camera, its own UI clicks.
    return CallWindowProc(g_origWndProc, h, msg, wp, lp);
}

// Snapshot under ONE shared acquire - SRW locks are not recursive.
static void SnapshotGui(Window local[MAX_WINDOWS]);
static void DrawRadar(const Widget& wd);

static const char* VkName(int vk) {
    static char fb[8];
    if (vk >= 0x70 && vk <= 0x7B) { snprintf(fb, sizeof(fb), "F%d", vk - 0x6F); return fb; }
    if (vk >= 'A' && vk <= 'Z')   { fb[0] = (char)vk; fb[1] = 0; return fb; }
    if (vk >= '0' && vk <= '9')   { fb[0] = (char)vk; fb[1] = 0; return fb; }
    switch (vk) {
    case VK_INSERT: return "INSERT"; case VK_DELETE: return "DELETE";
    case VK_HOME:   return "HOME";   case VK_END:    return "END";
    case VK_TAB:    return "TAB";    case VK_SHIFT:  return "SHIFT";
    case VK_CONTROL:return "CTRL";   case VK_MENU:   return "ALT";
    case VK_SPACE:  return "SPACE";  case VK_UP:     return "UP";
    case VK_DOWN:   return "DOWN";   case VK_LEFT:   return "LEFT";
    case VK_RIGHT:  return "RIGHT";
    }
    snprintf(fb, sizeof(fb), "0x%02X", vk);
    return fb;
}

static unsigned char g_prevKeys[256] = {};
static int g_pageSel[MAX_WINDOWS] = {};   // selected sidebar page per window

// ---- Redz-style custom controls -------------------------------------------

// Right-aligned iOS-like switch. Returns the NEW state.
static bool DrawSwitch(const char* id, bool v) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight() + 6.f;
    float w = h * 1.85f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (ImGui::InvisibleButton(id, ImVec2(w, h))) v = !v;
    ImU32 bg = v ? IM_COL32(61, 139, 255, 255) : IM_COL32(48, 54, 66, 255);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);
    float cx = v ? p.x + w - h * 0.5f : p.x + h * 0.5f;
    dl->AddCircleFilled(ImVec2(cx, p.y + h * 0.5f), h * 0.5f - 3.5f,
                        IM_COL32(238, 241, 246, 255), 20);
    return v;
}

// Title (+ optional gray description) on the left, control on the right edge.
template <typename F>
static void RedzRow(const Widget& wd, F&& placeControl) {
    float y0 = ImGui::GetCursorScreenPos().y;
    bool hasDesc = wd.desc[0] != 0;
    ImGui::BeginGroup();
    ImGui::TextUnformatted(wd.label);
    if (hasDesc) ImGui::TextDisabled("%s", wd.desc);
    ImGui::EndGroup();
    float ww = ImGui::GetWindowWidth();
    float pad = ImGui::GetStyle().WindowPadding.x;
    float ctlW = hasDesc ? 42.f : 0.f;
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(
        ImGui::GetWindowPos().x + ww - pad - (ctlW ? ctlW : 0.f),
        y0 + (hasDesc ? 3.f : 2.f)));
    placeControl();
}

// Paragraph card: rounded inset box with wrapped text.
static void DrawParagraph(const char* text) {
    float avail = ImGui::GetContentRegionAvail().x;
    ImVec2 ts = ImGui::CalcTextSize(text, nullptr, false, avail - 20.f);
    float hh = ts.y + 16.f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + avail, p.y + hh),
                      IM_COL32(24, 28, 36, 210), 7.f);
    ImGui::Dummy(ImVec2(avail, hh));
    ImGui::SetCursorScreenPos(ImVec2(p.x + 10.f, p.y + 8.f));
    ImGui::PushTextWrapPos(p.x + avail - 10.f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + hh + 4.f));
}

static void DrawGui() {
    Window local[MAX_WINDOWS];
    {
        const Window* ro = LockWindowsRO();
        memcpy(local, ro, sizeof(local));
        UnlockWindowsRO();
    }
    for (int w = 0; w < MAX_WINDOWS; w++) {
        if (!local[w].title[0] || !local[w].visible || local[w].count <= 0) continue;

        // Does this window use sidebar pages?
        bool paged = false;
        for (int i = 0; i < local[w].count && i < MAX_WIDGETS; i++)
            if (local[w].widgets[i].type == Widget::PAGETAB) { paged = true; break; }

        if (paged) {
            ImGui::SetNextWindowSize(ImVec2(560, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(430, 300), ImVec2(1200, 900));
            if (!ImGui::Begin(local[w].title, nullptr, 0)) { ImGui::End(); continue; }

            // ---- left sidebar with page tabs ----
            ImGui::BeginChild("##_sb", ImVec2(118, 0), true);
            int t = 0;
            for (int i = 0; i < local[w].count && i < MAX_WIDGETS; i++) {
                const Widget& wd = local[w].widgets[i];
                if (wd.type != Widget::PAGETAB) continue;
                if (ImGui::Selectable(wd.label, g_pageSel[w] == t,
                                      ImGuiSelectableFlags_SpanAllColumns))
                    g_pageSel[w] = t;
                t++;
            }
            ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 26);
            ImGui::TextDisabled("zelvex");
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##_ct", ImVec2(0, 0), false);
        } else {
            ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin(local[w].title, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::End();
                continue;
            }
        }
        int curPage = -1;   // -1 = header zone (before any PAGETAB): always drawn
        for (int i = 0; i < local[w].count && i < MAX_WIDGETS; i++) {
            const Widget& wd = local[w].widgets[i];

            // Sidebar page filter: PAGETAB widgets advance the page counter;
            // header-zone widgets (before any tab) always render.
            if (wd.type == Widget::PAGETAB) { curPage++; continue; }
            if (curPage >= 0 && curPage != g_pageSel[w]) continue;

            // Section gating: find the nearest SECTION before this widget;
            // widgets before any section are always shown.
            bool sectionOpen = true;
            if (i > 0) {
                int s = i - 1;
                while (s >= 0 && local[w].widgets[s].type != Widget::SECTION &&
                       local[w].widgets[s].type != Widget::PAGETAB) s--;
                if (s >= 0 && local[w].widgets[s].type == Widget::SECTION)
                    sectionOpen = local[w].widgets[s].bval;
            }
            if (!sectionOpen && wd.type != Widget::SECTION) continue;

            switch (wd.type) {
            case Widget::LABEL:
                if (wd.flags & 1) DrawParagraph(wd.label);
                else ImGui::TextUnformatted(wd.label);
                break;
            case Widget::BUTTON:
                if (wd.flags & 1) {           // accent-colored full-width button
                    ImGui::PushStyleColor(ImGuiCol_Button,
                        ImVec4(wd.col[0], wd.col[1], wd.col[2], 1.f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(wd.col[0] * 0.85f, wd.col[1] * 0.85f, wd.col[2] * 0.85f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                    if (ImGui::Button(wd.label, ImVec2(-1, 0))) PushEvent(w, i);
                    ImGui::PopStyleColor(3);
                } else if (ImGui::Button(wd.label)) PushEvent(w, i);
                break;
            case Widget::TOGGLE:
                RedzRow(wd, [&w, &i, &local]() {
                    const Widget& wdr = local[w].widgets[i];
                    bool b = DrawSwitch("##sw", wdr.bval);
                    if (b != wdr.bval) {
                        Window* ex = LockWindows(nullptr);
                        ex[w].widgets[i].bval = b;
                        UnlockWindows();
                        PushEvent(w, i);
                    }
                });
                break;
            case Widget::SLIDER: {
                float f = wd.fval;
                ImGui::PushItemWidth(-1);
                if (ImGui::SliderFloat(wd.label, &f, wd.fmin, wd.fmax, "%.0f")) {
                    Window* ex = LockWindows(nullptr);
                    ex[w].widgets[i].fval = f;
                    UnlockWindows();
                }
                ImGui::PopItemWidth();
                break;
            }
            case Widget::SECTION: {
                bool open = wd.bval;
                int flags = ImGuiTreeNodeFlags_CollapsingHeader |
                            ImGuiTreeNodeFlags_FramePadding;
                if (open) flags |= ImGuiTreeNodeFlags_DefaultOpen;
                bool now = ImGui::CollapsingHeader(wd.label, flags);
                if (now != wd.bval) {
                    Window* ex = LockWindows(nullptr);
                    ex[w].widgets[i].bval = now;
                    UnlockWindows();
                }
                break;
            }
            case Widget::DROPDOWN: {
                const char* items[8] = {};
                int n2 = wd.optCount > 8 ? 8 : wd.optCount;
                for (int k = 0; k < n2; k++) items[k] = wd.opts[k];
                int idx = wd.valIdx;
                if (ImGui::Combo(wd.label, &idx, items, n2) && idx != wd.valIdx) {
                    Window* ex = LockWindows(nullptr);
                    ex[w].widgets[i].valIdx = idx;
                    UnlockWindows();
                    PushEvent(w, i);
                }
                break;
            }
            case Widget::LISTBOX: {
                const char* items[8] = {};
                int n2 = wd.optCount > 8 ? 8 : wd.optCount;
                for (int k = 0; k < n2; k++) items[k] = wd.opts[k];
                int idx = (int)wd.fval;
                if (idx < 0) idx = 0;
                if (idx >= n2) idx = n2 - 1;
                ImGui::PushItemWidth(-1);
                if (ImGui::ListBox(wd.label, &idx, items, n2, n2 < 5 ? n2 : 5) &&
                    idx != (int)wd.fval) {
                    Window* ex = LockWindows(nullptr);
                    ex[w].widgets[i].fval = (float)idx;
                    UnlockWindows();
                    PushEvent(w, i);
                }
                ImGui::PopItemWidth();
                break;
            }
            case Widget::TEXTCOLORED:
                ImGui::TextColored(ImVec4(wd.col[0], wd.col[1], wd.col[2],
                                          wd.col[3] > 0 ? wd.col[3] : 1.0f),
                                   "%s", wd.label);
                break;
            case Widget::SMALLBUTTON:
                if (ImGui::SmallButton(wd.label)) PushEvent(w, i);
                break;
            case Widget::KEYBIND: {
                RedzRow(wd, [&w, &i]() {
                    char btn[24];
                    int vkCur;
                    {
                        const Window* ro = LockWindowsRO();
                        vkCur = (int)ro[w].widgets[i].fmin;
                        bool waitingRO = ro[w].widgets[i].bval;
                        UnlockWindowsRO();
                        if (waitingRO) { ImGui::Button("press..."); }
                        else if (vkCur > 0) {
                            snprintf(btn, sizeof(btn), "[ %s ]", VkName(vkCur));
                            ImGui::Button(btn);
                        } else {
                            ImGui::Button("[ ... ]");
                        }
                    }
                    bool waiting = false;
                    {
                        const Window* ro = LockWindowsRO();
                        waiting = ro[w].widgets[i].bval;
                        UnlockWindowsRO();
                    }
                    if (waiting) {
                        bool capturedNow = false;
                        for (int vk = 8; vk < 254 && !capturedNow; vk++) {
                            if (vk >= 1 && vk <= 6) continue;      // mouse buttons
                            if (vk == VK_INSERT) continue;          // reserved
                            unsigned char down =
                                (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
                            if (down && !g_prevKeys[vk]) {
                                Window* ex = LockWindows(nullptr);
                                ex[w].widgets[i].fmin = (float)vk;
                                ex[w].widgets[i].bval = false;
                                UnlockWindows();
                                PushEvent(w, i);                   // notify new bind
                                capturedNow = true;
                            }
                        }
                    }
                });
                break;
            }
            case Widget::INPUT: {
                char buf[64];
                strncpy(buf, wd.text, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText(wd.label, buf, sizeof(buf))) {
                    Window* ex = LockWindows(nullptr);
                    strncpy(ex[w].widgets[i].text, buf, sizeof(ex[w].widgets[i].text) - 1);
                    ex[w].widgets[i].text[sizeof(ex[w].widgets[i].text) - 1] = '\0';
                    UnlockWindows();
                }
                ImGui::PopItemWidth();
                if (ImGui::IsItemDeactivatedAfterEdit()) PushEvent(w, i);
                break;
            }
            case Widget::COLOR: {
                float c[3] = { wd.col[0], wd.col[1], wd.col[2] };
                if (ImGui::ColorEdit3(wd.label, c)) {
                    Window* ex = LockWindows(nullptr);
                    ex[w].widgets[i].col[0] = c[0];
                    ex[w].widgets[i].col[1] = c[1];
                    ex[w].widgets[i].col[2] = c[2];
                    UnlockWindows();
                    PushEvent(w, i);
                }
                break;
            }
            case Widget::SEPARATOR:
                ImGui::Separator();
                break;
            case Widget::SAMELINE:
                ImGui::SameLine();
                break;
            case Widget::PROGRESS: {
                float frac = wd.fval;
                if (frac < 0) frac = 0; if (frac > 1) frac = 1;
                char overlay[32] = "";
                if (wd.label[0]) snprintf(overlay, sizeof(overlay), "%s %.0f%%", wd.label, frac * 100);
                ImGui::ProgressBar(frac, ImVec2(-1, 0), overlay[0] ? overlay : nullptr);
                break;
            }
            case Widget::COMBO: {
                const char* items[8] = {};
                int n2 = wd.optCount > 8 ? 8 : wd.optCount;
                for (int k = 0; k < n2; k++) items[k] = wd.opts[k];
                int idx = (int)wd.fval;
                if (idx < 0) idx = 0; if (idx >= n2) idx = n2 - 1;
                if (ImGui::Combo(wd.label, &idx, items, n2) && idx != (int)wd.fval) {
                    Window* ex = LockWindows(nullptr);
                    ex[w].widgets[i].fval = (float)idx;
                    UnlockWindows();
                    PushEvent(w, i);
                }
                break;
            }
            case Widget::RADAR:
                DrawRadar(wd);
                break;
            default: break;
            }
        }

        // keybind press-edge callbacks fire once per fresh press of bound key
        for (int i = 0; i < local[w].count && i < MAX_WIDGETS; i++) {
            const Widget& wd = local[w].widgets[i];
            if (wd.type == Widget::KEYBIND && !wd.bval && (int)wd.fmin > 0 &&
                wd.cbRef != 0) {
                int vk = (int)wd.fmin;
                unsigned char down = (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
                if (down && !g_prevKeys[vk]) PushEvent(w, i);
            }
        }

        if (paged) ImGui::EndChild();   // ##_ct
        ImGui::End();
    }

    // advance global key-edge snapshot AFTER all windows processed
    for (int vk = 8; vk < 254; vk++)
        g_prevKeys[vk] = (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
}

// ---- Radar ESP ------------------------------------------------------------
// Self-contained game-memory scan (same layout as lua_dll's IteratePlayers):
// player table [[libMiniBaseGame.dll+B36C]+78]+68, 40 slots,
// uid@+0 pos@+14/18/1C team@+B0; own yaw at [[pc+0x950]+4].
static bool RdrReadable(const void* p, size_t n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect & 0xFF;
    if (prot == PAGE_NOACCESS || prot == PAGE_GUARD) return false;
    return true;
}

struct RdrPlayer { float x, y, z; int team; };

static int RdrScan(RdrPlayer* out, int maxOut, float* outYaw,
                   float* outMx, float* outMy, float* outMz) {
    HMODULE mb = GetModuleHandleA("libMiniBaseGame.dll");
    if (!mb || !RdrReadable((BYTE*)mb + 0xB36C, 4)) return 0;
    BYTE* c1 = *(BYTE**)((BYTE*)mb + 0xB36C);
    if (!RdrReadable(c1, 0x80) || !RdrReadable(c1 + 0x78, 4)) return 0;
    BYTE* c2 = *(BYTE**)(c1 + 0x78);
    if (!RdrReadable(c2, 0x70) || !RdrReadable(c2 + 0x68, 4)) return 0;
    BYTE* list = *(BYTE**)(c2 + 0x68);

    // own position via g_pPlayerCtrl -> attr chain (block ints)
    HMODULE se = GetModuleHandleA("libSandboxEngine.dll");
    if (!se || !RdrReadable((BYTE*)se + 0x27772F8, 4)) return 0;
    BYTE* pc = *(BYTE**)((BYTE*)se + 0x27772F8);
    if (!pc || !RdrReadable(pc + 0x280, 8)) return 0;
    BYTE* attr = *(BYTE**)(pc + 0x280);
    if (!attr || !RdrReadable(attr + 0xDC, 12)) return 0;
    float mx = (float)*(int*)(attr + 0xDC);
    float my = (float)*(int*)(attr + 0xE0);
    float mz = (float)*(int*)(attr + 0xE4);
    *outMx = mx; *outMy = my; *outMz = mz;

    *outYaw = 0.f;
    if (RdrReadable(pc + 0x950, 4)) {
        BYTE* aim = *(BYTE**)(pc + 0x950);
        if (aim && RdrReadable(aim + 8, 4)) *outYaw = *(float*)(aim + 4);
    }

    int n = 0;
    for (int i = 0; i < 40 && n < maxOut; i++) {
        if (!RdrReadable(list + i * 4, 4)) continue;
        BYTE* p = *(BYTE**)(list + i * 4);
        if (!p || !RdrReadable(p, 0xC0)) continue;
        uint32_t uid = *(uint32_t*)p;
        if (uid == 0) continue;
        int fac = *(int*)(p + 0xB0);
        if (fac < 1 || fac > 64) continue;
        int x = *(int*)(p + 0x14), y = *(int*)(p + 0x18), z = *(int*)(p + 0x1C);
        if (x < -30000000 || x > 30000000 || y < -30000000 ||
            y > 30000000 || z < -30000000 || z > 30000000) continue;
        float dx = (float)x - mx, dz = (float)z - mz;
        if (dx * dx + dz * dz < 2.25f) continue;   // self / same-spot filter
        out[n].x = (float)x; out[n].y = (float)y; out[n].z = (float)z;
        out[n].team = fac;
        n++;
    }
    return n;
}

// Foreground-HUD radar drawn from live game memory.
// wd.fmin = radius px, wd.fmax = style (0 dots, 1 boxes, 2 crosses),
// wd.fval = world range (blocks) mapped to the radius.
static void DrawRadar(const Widget& wd) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 vp = ImGui::GetIO().DisplaySize;
    float radius = wd.fmin > 20.f ? wd.fmin : 110.f;
    ImVec2 center(30 + radius, vp.y - 30 - radius);

    float yaw = 0.f, mx = 0, my = 0, mz = 0;
    RdrPlayer players[40];
    int n = RdrScan(players, 40, &yaw, &mx, &my, &mz);

    dl->AddCircleFilled(center, radius, IM_COL32(10, 13, 18, 170));
    dl->AddCircle(center, radius, IM_COL32(61, 139, 255, 200), 48, 1.5f);
    dl->AddCircle(center, radius * 0.5f, IM_COL32(61, 139, 255, 90), 40, 1.0f);
    dl->AddLine(ImVec2(center.x - radius, center.y),
                ImVec2(center.x + radius, center.y), IM_COL32(61, 139, 255, 60));
    dl->AddLine(ImVec2(center.x, center.y - radius),
                ImVec2(center.x, center.y + radius), IM_COL32(61, 139, 255, 60));

    double yr = yaw * 3.14159265358979323846 / 180.0;
    double fx = sin(yr), fz = cos(yr);          // facing dir (yaw measured from +Z)
    double rx = cos(yr), rz = -sin(yr);         // right dir
    float range = wd.fval > 100.f ? wd.fval : 6000.f;

    for (int i = 0; i < n; i++) {
        double dx = players[i].x - mx;
        double dz = players[i].z - mz;
        double sx = dx * rx + dz * rz;
        double sy = -(dx * fx + dz * fz);
        double d = sqrt(sx * sx + sy * sy);
        double sc = d / range * radius;
        if (sc > radius - 4.0) { sx *= (radius - 4.0) / sc; sy *= (radius - 4.0) / sc; }
        ImVec2 pt(center.x + (float)sx, center.y + (float)sy);
        ImU32 col = players[i].team == 1 ? IM_COL32(90, 163, 255, 255)
                  : players[i].team == 2 ? IM_COL32(140, 205, 255, 255)
                                         : IM_COL32(200, 235, 255, 255);
        int style = (int)wd.fmax;
        if (style == 1) {          // boxes
            dl->AddRect(ImVec2(pt.x - 4, pt.y - 4), ImVec2(pt.x + 4, pt.y + 4), col, 0.f, 0, 1.6f);
        } else if (style == 2) {   // crosses
            dl->AddLine(ImVec2(pt.x - 4, pt.y - 4), ImVec2(pt.x + 4, pt.y + 4), col, 1.6f);
            dl->AddLine(ImVec2(pt.x - 4, pt.y + 4), ImVec2(pt.x + 4, pt.y - 4), col, 1.6f);
        } else {                   // dots
            dl->AddCircleFilled(pt, 3.2f, col);
        }
    }

    // self arrow (always up)
    dl->AddTriangleFilled(ImVec2(center.x, center.y - 6), ImVec2(center.x - 4.5f, center.y + 5),
                          ImVec2(center.x + 4.5f, center.y + 5), IM_COL32(230, 233, 238, 255));
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", n);
    dl->AddText(ImVec2(center.x - 4, center.y - radius - 16), IM_COL32(154, 163, 178, 255), buf);
}


// Zelvex brand: near-black panels, blue accent, rounded frames.
static void ApplyZelvexTheme();

static bool BootImGui(IDXGISwapChain* sc) {
    if (g_imguiReady) return true;
    if (!sc) return false;
    ID3D11Device* dev = nullptr;
    if (FAILED(sc->GetDevice(kIID_ID3D11Device, (void**)&dev)) || !dev) return false;
    ID3D11DeviceContext* ctx = nullptr;
    dev->GetImmediateContext(&ctx);
    if (!ctx) { dev->Release(); return false; }
    ImGui::CreateContext();
    ApplyZelvexTheme();
    ImGui_ImplWin32_Init(g_gameWnd);
    ImGui_ImplDX11_Init(dev, ctx);
    g_dev = dev;              // keep refs alive for the render loop
    g_ctx = ctx;
    g_imguiReady = true;
    return true;
}

// Redz-hub style: near-black rounded window, dark-gray cards, blue accent.
static void ApplyZelvexTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding   = 10.0f;
    s.ChildRounding    = 8.0f;
    s.FrameRounding    = 6.0f;
    s.GrabRounding     = 6.0f;
    s.PopupRounding    = 7.0f;
    s.ScrollbarRounding= 8.0f;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize  = 0.0f;
    s.FrameBorderSize  = 0.0f;
    s.WindowPadding    = ImVec2(14, 12);
    s.FramePadding     = ImVec2(9, 5);
    s.ItemSpacing      = ImVec2(9, 8);
    s.ItemInnerSpacing = ImVec2(7, 5);

    ImVec4* c = s.Colors;
    auto rgb = [](int r, int g, int b, float a = 1.0f) {
        return ImVec4(r / 255.f, g / 255.f, b / 255.f, a);
    };
    c[ImGuiCol_WindowBg]        = rgb(13, 14, 17, 0.97f);   // near-black window
    c[ImGuiCol_ChildBg]         = rgb(22, 24, 30, 0.85f);   // dark-gray card
    c[ImGuiCol_TitleBg]         = rgb(10, 11, 14, 1.0f);
    c[ImGuiCol_TitleBgActive]   = rgb(16, 18, 23, 1.0f);
    c[ImGuiCol_Border]          = rgb(40, 44, 54, 0.9f);
    c[ImGuiCol_Text]            = rgb(232, 235, 240);
    c[ImGuiCol_TextDisabled]    = rgb(122, 129, 141);
    c[ImGuiCol_FrameBg]         = rgb(28, 31, 39, 1.0f);    // input/slider well
    c[ImGuiCol_FrameBgHovered]  = rgb(36, 40, 50, 1.0f);
    c[ImGuiCol_FrameBgActive]   = rgb(43, 48, 60, 1.0f);
    c[ImGuiCol_Button]          = rgb(32, 36, 45, 1.0f);    // neutral row-button
    c[ImGuiCol_ButtonHovered]   = rgb(42, 47, 58, 1.0f);
    c[ImGuiCol_ButtonActive]    = rgb(26, 29, 37, 1.0f);
    c[ImGuiCol_CheckMark]       = rgb(61, 139, 255);
    c[ImGuiCol_SliderGrab]      = rgb(61, 139, 255);
    c[ImGuiCol_SliderGrabActive]= rgb(120, 175, 255);
    c[ImGuiCol_Separator]       = rgb(45, 50, 62, 0.6f);
    c[ImGuiCol_ScrollbarBg]     = rgb(13, 14, 17, 0.4f);
    c[ImGuiCol_ScrollbarGrab]   = rgb(46, 52, 64);
    c[ImGuiCol_PopupBg]         = rgb(18, 20, 25, 0.98f);
    c[ImGuiCol_Header]          = rgb(61, 139, 255, 0.35f); // sidebar selection
    c[ImGuiCol_HeaderHovered]   = rgb(61, 139, 255, 0.55f);
    c[ImGuiCol_HeaderActive]    = rgb(61, 139, 255, 0.75f);
    c[ImGuiCol_ModalWindowDimBg]= ImVec4(0, 0, 0, 0.5f);
}

static HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    BootImGui(sc);
    // Hidden menu = render nothing at all. (Previously we kept drawing,
    // so INSERT left the menu painted while the game took input back.)
    if (!g_visible) {
        return g_realPresent ? g_realPresent(sc, sync, flags) : S_OK;
    }
    if (g_imguiReady && sc) {
        if (g_swap != sc) {   // swap chain changed (resize/recreate)
            if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
            g_swap = sc;
        }
        if (!g_rtv) {
            ID3D11Texture2D* bb = nullptr;
            if (SUCCEEDED(g_swap->GetBuffer(0, kIID_ID3D11Texture2D, (void**)&bb))) {
                g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
                bb->Release();
            }
        }
        if (g_rtv) {
            ID3D11DeviceContext* ctx = g_ctx;

            // ---- backup the game's pipeline state (ANGLE is picky) ----
            ID3D11RenderTargetView*  bkRTV = nullptr;
            ID3D11DepthStencilView*  bkDSV = nullptr;
            ctx->OMGetRenderTargets(1, &bkRTV, &bkDSV);
            UINT vpCount = 0;
            ctx->RSGetViewports(&vpCount, nullptr);
            D3D11_VIEWPORT bkVP[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            if (vpCount) ctx->RSGetViewports(&vpCount, bkVP);
            ID3D11RasterizerState*   bkRS  = nullptr; ctx->RSGetState(&bkRS);
            ID3D11BlendState*        bkBS  = nullptr;
            FLOAT bkBlend[4] = {}; UINT bkMask = 0;
            ctx->OMGetBlendState(&bkBS, bkBlend, &bkMask);
            ID3D11DepthStencilState* bkDSS = nullptr; UINT bkStencil = 0;
            ctx->OMGetDepthStencilState(&bkDSS, &bkStencil);

            // ---- render ImGui into the back buffer ----
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            {
                ImVec2 ds = ImGui::GetIO().DisplaySize;
                InterlockedExchange(&g_dispW, (LONG)ds.x);
                InterlockedExchange(&g_dispH, (LONG)ds.y);
            }
            DrawGui();
            ImGui::Render();
            ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // ---- restore everything so ANGLE never sees our state ----
            ctx->RSSetState(bkRS);
            if (vpCount) ctx->RSSetViewports(vpCount, bkVP);
            ctx->OMSetBlendState(bkBS, bkBlend, bkMask);
            ctx->OMSetDepthStencilState(bkDSS, bkStencil);
            ctx->OMSetRenderTargets(1, bkRTV ? &bkRTV : nullptr, bkDSV);
            if (bkRTV) bkRTV->Release();
            if (bkDSV) bkDSV->Release();
            if (bkRS)  bkRS->Release();
            if (bkBS)  bkBS->Release();
            if (bkDSS) bkDSS->Release();
        }
    }
    return g_realPresent ? g_realPresent(sc, sync, flags) : S_OK;
}

bool Start() {
    if (g_running) return true;
    g_gameWnd = FindGameWindow();
    if (!g_gameWnd) return false;

    // Dummy device+swapchain purely to obtain the shared Present vtable entry.
    WNDCLASSEXA wc = { sizeof(wc), CS_OWNDC, DefWindowProcA, 0, 0,
                       GetModuleHandleA(nullptr), nullptr,
                       LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr,
                       "ZelvexTmp", nullptr };
    RegisterClassExA(&wc);
    HWND tmp = CreateWindowExA(0, "ZelvexTmp", "", WS_OVERLAPPEDWINDOW,
                               0, 0, 8, 8, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = tmp;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain* dummySC = nullptr;
    ID3D11Device* dummyDev = nullptr;
    ID3D11DeviceContext* dummyCtx = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd,
        &dummySC, &dummyDev, nullptr, &dummyCtx);
    if (FAILED(hr)) { DestroyWindow(tmp); return false; }

    g_vtable      = *(void***)dummySC;
    g_realPresent = (PFN_Present)g_vtable[8];    // IDXGISwapChain::Present

    DWORD old = 0;
    VirtualProtect(&g_vtable[8], sizeof(void*), PAGE_READWRITE, &old);
    g_vtable[8] = (void*)&HookPresent;
    VirtualProtect(&g_vtable[8], sizeof(void*), old, &old);

    dummySC->Release(); dummyCtx->Release(); dummyDev->Release();
    DestroyWindow(tmp);                          // dxgi.dll stays loaded

    g_origWndProc = (WNDPROC)SetWindowLongPtrW(g_gameWnd, GWLP_WNDPROC, (LONG_PTR)HookWndProc);
    g_running = true;
    return true;
}

void Stop() {
    if (!g_running) return;
    if (g_vtable && g_realPresent) {
        DWORD old = 0;
        VirtualProtect(&g_vtable[8], sizeof(void*), PAGE_READWRITE, &old);
        g_vtable[8] = (void*)g_realPresent;
        VirtualProtect(&g_vtable[8], sizeof(void*), old, &old);
    }
    if (g_origWndProc)
        SetWindowLongPtrW(g_gameWnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
    if (g_imguiReady) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiReady = false;
    }
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
    g_swap = nullptr;
    g_running = false;
}

} // namespace Overlay


