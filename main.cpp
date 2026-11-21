#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

static float g_TorchSpeed = 1.65f;
static float g_EnemyDmg = 3.5f;
static float g_Aggro = 1.54f;
static HWND g_Panel = 0, g_sTorch = 0, g_sDmg = 0, g_sAggro = 0;
static HWND g_lTorch = 0, g_lDmg = 0, g_lAggro = 0;
static bool g_Visible = false;

typedef void(__thiscall* Move_t)(void*, float);
static Move_t oStormMove = (Move_t)0x004A6400;
static void** g_StormVT = 0;

HWND FindGameWindow() {
    HWND gw = FindWindowA(0, "Ultimate Spider-Man");
    if (!gw) gw = GetForegroundWindow();
    return gw;
}

void EnsureBorderless(HWND gw) {
    if (!gw || gw == g_Panel) return;
    LONG style = GetWindowLongA(gw, GWL_STYLE);
    if (style & WS_CAPTION) {
        SetWindowLongA(gw, GWL_STYLE, (style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU)) | WS_POPUP);
        SetWindowPos(gw, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}

void FocusGame() {
    HWND gw = FindWindowA(0, "Ultimate Spider-Man");
    if (gw) SetForegroundWindow(gw);
}

void LoadIni() {
    std::ifstream in("HardcoreMod.ini");
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        float v = 0.f;
        if (sscanf(line.c_str(), "ENEMY_DMG=%f", &v) == 1) g_EnemyDmg = v;
        else if (sscanf(line.c_str(), "TORCH_SPEED=%f", &v) == 1) g_TorchSpeed = v;
        else if (sscanf(line.c_str(), "ENEMY_ATTACK_DELAY=%f", &v) == 1 && v > 0.01f) g_Aggro = 1.0f / v;
    }
}

void SaveIni() {
    FILE* f = fopen("HardcoreMod.ini", "w");
    if (f) {
        fprintf(f, "ENEMY_DMG=%.2f\nTORCH_SPEED=%.2f\nENEMY_ATTACK_DELAY=%.2f\n", g_EnemyDmg, g_TorchSpeed, 1.0f / g_Aggro);
        fclose(f);
    }
}

void __fastcall hkStormMove(void* ecx, void*, float dt) {
    oStormMove(ecx, dt * g_TorchSpeed);
}

void UpdateLabels() {
    std::vector<char> b(128, 0);
    std::snprintf(b.data(), b.size(), "Johnny Storm Speed: %.2fx", g_TorchSpeed); SetWindowTextA(g_lTorch, b.data());
    std::snprintf(b.data(), b.size(), "Enemy Damage: %.2fx", g_EnemyDmg); SetWindowTextA(g_lDmg, b.data());
    std::snprintf(b.data(), b.size(), "Enemy Aggression: %.2fx", g_Aggro); SetWindowTextA(g_lAggro, b.data());
}

LRESULT CALLBACK PanelProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_HSCROLL) {
        g_TorchSpeed = SendMessageA(g_sTorch, TBM_GETPOS, 0, 0) / 100.0f;
        g_EnemyDmg = SendMessageA(g_sDmg, TBM_GETPOS, 0, 0) / 100.0f;
        g_Aggro = SendMessageA(g_sAggro, TBM_GETPOS, 0, 0) / 100.0f;
        UpdateLabels();
    } else if (m == WM_COMMAND && LOWORD(wp) == 101) {
        SaveIni();
        MessageBeep(MB_OK);
    } else if (m == WM_CLOSE) {
        ShowWindow(h, SW_HIDE);
        g_Visible = false;
        FocusGame();
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

DWORD WINAPI GuiThread(LPVOID) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);
    HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    WNDCLASSA wc = { 0, PanelProc, 0, 0, GetModuleHandleA(0), 0, LoadCursorA(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), 0, "USM_HC_PANEL" };
    RegisterClassA(&wc);
    g_Panel = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "USM_HC_PANEL", "USM Hardcore Mod (F1)", WS_POPUP | WS_CAPTION | WS_SYSMENU, 30, 30, 320, 235, 0, 0, wc.hInstance, 0);

    g_lTorch = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 15, 12, 280, 18, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_lTorch, WM_SETFONT, (WPARAM)hf, 1);
    g_sTorch = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 32, 290, 28, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_sTorch, TBM_SETRANGE, 1, MAKELONG(100, 300));
    SendMessageA(g_sTorch, TBM_SETPOS, 1, (LPARAM)(g_TorchSpeed * 100));

    g_lDmg = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 15, 66, 280, 18, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_lDmg, WM_SETFONT, (WPARAM)hf, 1);
    g_sDmg = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 86, 290, 28, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_sDmg, TBM_SETRANGE, 1, MAKELONG(100, 500));
    SendMessageA(g_sDmg, TBM_SETPOS, 1, (LPARAM)(g_EnemyDmg * 100));

    g_lAggro = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 15, 120, 280, 18, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_lAggro, WM_SETFONT, (WPARAM)hf, 1);
    g_sAggro = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 140, 290, 28, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_sAggro, TBM_SETRANGE, 1, MAKELONG(100, 300));
    SendMessageA(g_sAggro, TBM_SETPOS, 1, (LPARAM)(g_Aggro * 100));

    HWND btn = CreateWindowA("BUTTON", "Save to INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 95, 175, 120, 26, g_Panel, (HMENU)101, wc.hInstance, 0);
    SendMessageA(btn, WM_SETFONT, (WPARAM)hf, 1);
    UpdateLabels();

    MSG msg;
    bool prevF1 = false;
    for (;;) {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        bool curF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (curF1 && !prevF1) {
            g_Visible = !g_Visible;
            if (g_Visible) {
                EnsureBorderless(FindGameWindow());
                ShowWindow(g_Panel, SW_SHOW);
                SetWindowPos(g_Panel, HWND_TOPMOST, 30, 30, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                ClipCursor(0);
                while (ShowCursor(TRUE) < 0) {}
                SetForegroundWindow(g_Panel);
            } else {
                ShowWindow(g_Panel, SW_HIDE);
                FocusGame();
            }
        }
        prevF1 = curF1;
        Sleep(20);
    }
}

DWORD WINAPI Worker(LPVOID) {
    LoadIni();
    Sleep(3000);
    CreateThread(0, 0, GuiThread, 0, 0, 0);
    std::unordered_map<uintptr_t, float> hp, tm, mx;
    int tk = 0;
    for (;;) {
        Sleep(50);
        if ((++tk % 40) == 1) {
            if (hp.size() > 15000) hp.clear();
            if (tm.size() > 15000) { tm.clear(); mx.clear(); }
            MEMORY_BASIC_INFORMATION m;
            for (uintptr_t a = 0x00A00000; a < 0x7FEE0000; a = (uintptr_t)m.BaseAddress + m.RegionSize) {
                if (!VirtualQuery((void*)a, &m, sizeof(m))) break;
                if (m.State == MEM_COMMIT && m.Type == MEM_PRIVATE && m.Protect == PAGE_READWRITE) {
                    uintptr_t b = (uintptr_t)m.BaseAddress, e = b + m.RegionSize - 32;
                    for (uintptr_t p = b; p < e; p += 4) {
                        if (*(uint32_t*)p == 0x00882B60 && *(uint32_t*)(p + 0x50) == 0x96478E0A) {
                            void* c = *(void**)(p + 0x58);
                            if (c && !IsBadWritePtr(c, 4) && *(uint32_t*)c == 0x00881460) {
                                if (!g_StormVT) {
                                    g_StormVT = (void**)malloc(128);
                                    memcpy(g_StormVT, (void*)0x00881460, 128);
                                    *(g_StormVT + 11) = (void*)hkStormMove;
                                }
                                *(void**)c = g_StormVT;
                            }
                        }
                        float cur = *(float*)p, mn = *(float*)(p + 4), mxv = *(float*)(p + 8);
                        if (mn == 0.f && cur >= 25.f && cur <= mxv && (mxv == 100.f || mxv == 200.f) && !hp.count(p))
                            hp[p] = cur;
                        if (*(int*)(p + 12) == 0 && *(int*)(p + 16) == 0 && *(int*)(p + 20) == 0 && cur > 0.f && mxv >= 1.f && mxv < 8.f && cur < mxv && !tm.count(p)) {
                            tm[p] = cur;
                            mx[p] = mxv;
                        }
                    }
                }
            }
        }
        for (auto it = hp.begin(); it != hp.end();) {
            if (IsBadWritePtr((void*)it->first, 4)) { it = hp.erase(it); continue; }
            float nh = *(float*)it->first, oh = it->second;
            if (oh - nh >= 2.f && nh >= -50.f && oh - nh < 90.f) {
                float adj = fmaxf(0.f, oh - (oh - nh) * g_EnemyDmg);
                *(float*)it->first = adj;
                it->second = adj;
            } else if (nh > oh && nh <= 200.f) {
                it->second = nh;
            }
            ++it;
        }
        if (g_Aggro > 1.01f) {
            float bst = g_Aggro - 1.f;
            for (auto it = tm.begin(); it != tm.end();) {
                if (IsBadWritePtr((void*)it->first, 4)) { it = tm.erase(it); continue; }
                float nt = *(float*)it->first, ot = it->second, lm = mx[it->first], dt = nt - ot;
                if (dt >= 0.015f && dt <= 0.18f && nt > 0.f && nt < lm) {
                    float adv = fminf(lm, nt + dt * bst);
                    *(float*)it->first = adv;
                    it->second = adv;
                } else {
                    it->second = nt;
                }
                ++it;
            }
        }
    }
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        CreateThread(0, 0, Worker, 0, 0, 0);
    }
    return 1;
}
