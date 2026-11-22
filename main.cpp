#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

static float g_TorchSpeed = 1.65f, g_EnemyDmg = 3.5f, g_Aggro = 1.54f;
static HWND g_Panel = 0, g_sTorch = 0, g_sDmg = 0, g_sAggro = 0;
static HWND g_lTorch = 0, g_lDmg = 0, g_lAggro = 0, g_lDbg = 0, g_BtnSave = 0;
static bool g_Visible = false;
static uintptr_t g_StormEnt = 0;

std::wstring GetIniPathW() {
    std::vector<wchar_t> buf(MAX_PATH, 0);
    GetModuleFileNameW(0, buf.data(), MAX_PATH);
    std::wstring p(buf.data());
    size_t pos = p.find_last_of(L"\\/");
    return (pos != std::wstring::npos ? p.substr(0, pos + 1) : L"") + L"HardcoreMod.ini";
}

bool SafeReadU32(uintptr_t a, uint32_t* o) { __try { *o = *(uint32_t*)a; return true; } __except (1) { return false; } }
bool SafeReadF32(uintptr_t a, float* o) { __try { *o = *(float*)a; return true; } __except (1) { return false; } }
bool SafeWriteF32(uintptr_t a, float v) { __try { *(float*)a = v; return true; } __except (1) { return false; } }

void FocusGame() {
    HWND gw = FindWindowA(0, "Ultimate Spider-Man");
    if (gw) { SetForegroundWindow(gw); RECT rc; if (GetWindowRect(gw, &rc)) ClipCursor(&rc); }
}

void LoadIni() {
    FILE* f = _wfopen(GetIniPathW().c_str(), L"r");
    if (!f) return;
    std::vector<char> l(256, 0);
    while (fgets(l.data(), (int)l.size(), f)) {
        float v = 0.f;
        if (sscanf(l.data(), "ENEMY_DMG=%f", &v) == 1) g_EnemyDmg = v;
        else if (sscanf(l.data(), "TORCH_SPEED=%f", &v) == 1) g_TorchSpeed = v;
        else if (sscanf(l.data(), "ENEMY_ATTACK_DELAY=%f", &v) == 1 && v > 0.01f) g_Aggro = 1.0f / v;
    }
    fclose(f);
}

bool SaveIni() {
    FILE* f = _wfopen(GetIniPathW().c_str(), L"w");
    if (!f) return false;
    fprintf(f, "ENEMY_DMG=%.2f\nTORCH_SPEED=%.2f\nENEMY_ATTACK_DELAY=%.2f\n", g_EnemyDmg, g_TorchSpeed, 1.0f / g_Aggro);
    fclose(f);
    return true;
}

void UpdateLabels() {
    std::vector<char> b(256, 0);
    std::snprintf(b.data(), b.size(), "Johnny Storm Speed: %.2fx", g_TorchSpeed); SetWindowTextA(g_lTorch, b.data());
    std::snprintf(b.data(), b.size(), "Enemy & Boss Damage: %.2fx", g_EnemyDmg); SetWindowTextA(g_lDmg, b.data());
    std::snprintf(b.data(), b.size(), "Enemy & Boss Aggression: %.2fx", g_Aggro); SetWindowTextA(g_lAggro, b.data());
    uint32_t p64 = 0, p68 = 0, p6c = 0, p70 = 0, c08 = 0, c10 = 0;
    if (g_StormEnt && SafeReadU32(g_StormEnt + 0x64, &p64)) {
        SafeReadU32(g_StormEnt + 0x68, &p68);
        SafeReadU32(g_StormEnt + 0x6C, &p6c);
        SafeReadU32(g_StormEnt + 0x70, &p70);
        uint32_t p58 = 0;
        if (SafeReadU32(g_StormEnt + 0x58, &p58) && p58 >= 0x00A00000) {
            SafeReadU32(p58 + 0x08, &c08);
            SafeReadU32(p58 + 0x10, &c10);
        }
        std::snprintf(b.data(), b.size(), "64:%08X 68:%08X 6C:%08X\n70:%08X C08:%08X C10:%08X", p64, p68, p6c, p70, c08, c10);
    } else {
        std::snprintf(b.data(), b.size(), "Storm: Not in race (start Torch race)");
    }
    SetWindowTextA(g_lDbg, b.data());
}

LRESULT CALLBACK PanelProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_SETCURSOR) { SetCursor(LoadCursorA(0, IDC_ARROW)); return TRUE; }
    if (m == WM_HSCROLL) {
        g_TorchSpeed = SendMessageA(g_sTorch, TBM_GETPOS, 0, 0) / 100.0f;
        g_EnemyDmg = SendMessageA(g_sDmg, TBM_GETPOS, 0, 0) / 100.0f;
        g_Aggro = SendMessageA(g_sAggro, TBM_GETPOS, 0, 0) / 100.0f;
        UpdateLabels();
        if (g_BtnSave) SetWindowTextA(g_BtnSave, "Save to INI");
    } else if (m == WM_COMMAND && LOWORD(wp) == 101) {
        SetWindowTextA(g_BtnSave, SaveIni() ? "Saved!" : "Error!");
        MessageBeep(MB_OK);
    } else if (m == WM_CLOSE) {
        SaveIni(); ShowWindow(h, SW_HIDE); g_Visible = false; FocusGame(); return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

DWORD WINAPI GuiThread(LPVOID) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);
    HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    WNDCLASSA wc = { 0, PanelProc, 0, 0, GetModuleHandleA(0), 0, LoadCursorA(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), 0, "USM_HC_PANEL" };
    RegisterClassA(&wc);
    g_Panel = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "USM_HC_PANEL", "USM Hardcore Mod (F1)", WS_POPUP | WS_CAPTION | WS_SYSMENU, 30, 30, 360, 280, 0, 0, wc.hInstance, 0);
    g_lTorch = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 15, 12, 330, 18, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_lTorch, WM_SETFONT, (WPARAM)hf, 1);
    g_sTorch = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 32, 330, 28, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_sTorch, TBM_SETRANGE, 1, MAKELONG(100, 300));
    SendMessageA(g_sTorch, TBM_SETPOS, 1, (LPARAM)(g_TorchSpeed * 100));

    g_lDmg = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 15, 66, 330, 18, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_lDmg, WM_SETFONT, (WPARAM)hf, 1);
    g_sDmg = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 86, 330, 28, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_sDmg, TBM_SETRANGE, 1, MAKELONG(100, 500));
    SendMessageA(g_sDmg, TBM_SETPOS, 1, (LPARAM)(g_EnemyDmg * 100));

    g_lAggro = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 15, 120, 330, 18, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_lAggro, WM_SETFONT, (WPARAM)hf, 1);
    g_sAggro = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 140, 330, 28, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_sAggro, TBM_SETRANGE, 1, MAKELONG(100, 250));
    SendMessageA(g_sAggro, TBM_SETPOS, 1, (LPARAM)(g_Aggro * 100));

    g_lDbg = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 15, 174, 330, 34, g_Panel, 0, wc.hInstance, 0);
    SendMessageA(g_lDbg, WM_SETFONT, (WPARAM)hf, 1);

    g_BtnSave = CreateWindowA("BUTTON", "Save to INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 115, 214, 120, 26, g_Panel, (HMENU)101, wc.hInstance, 0);
    SendMessageA(g_BtnSave, WM_SETFONT, (WPARAM)hf, 1);
    UpdateLabels();

    MSG msg;
    bool prevF1 = false;
    for (;;) {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        bool curF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (curF1 && !prevF1) {
            g_Visible = !g_Visible;
            if (g_Visible) {
                UpdateLabels();
                SetWindowTextA(g_BtnSave, "Save to INI");
                ShowWindow(g_Panel, SW_SHOW);
                SetWindowPos(g_Panel, HWND_TOPMOST, 30, 30, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                SetForegroundWindow(g_Panel);
            } else {
                SaveIni(); ShowWindow(g_Panel, SW_HIDE); FocusGame();
            }
        }
        if (g_Visible) { ClipCursor(0); while (ShowCursor(TRUE) < 0) {} }
        prevF1 = curF1;
        Sleep(20);
    }
}

DWORD WINAPI Worker(LPVOID) {
    LoadIni();
    Sleep(5000);
    CreateThread(0, 0, GuiThread, 0, 0, 0);
    std::unordered_map<uintptr_t, float> hp, tmUp, tmDn, mx;
    int tk = 0;
    for (;;) {
        Sleep(50);
        if ((++tk % 40) == 1) {
            if (hp.size() > 15000) hp.clear();
            if (tmUp.size() > 12000) { tmUp.clear(); mx.clear(); }
            if (tmDn.size() > 12000) tmDn.clear();
            g_StormEnt = 0;
            MEMORY_BASIC_INFORMATION m;
            for (uintptr_t a = 0x00A00000; a < 0x7FEE0000; a = (uintptr_t)m.BaseAddress + m.RegionSize) {
                if (!VirtualQuery((void*)a, &m, sizeof(m))) break;
                if (m.State == MEM_COMMIT && m.Type == MEM_PRIVATE && m.Protect == PAGE_READWRITE && m.RegionSize >= 256) {
                    uintptr_t b = (uintptr_t)m.BaseAddress, e = b + m.RegionSize - 128;
                    for (uintptr_t p = b + 4; p < e; p += 4) {
                        uint32_t u0 = 0, h50 = 0;
                        if (!SafeReadU32(p, &u0)) break;
                        if (u0 == 0x00882B60 && SafeReadU32(p + 0x50, &h50) && h50 == 0x96478E0A) g_StormEnt = p;
                        float cur = 0.f, mn = 0.f, mxv = 0.f;
                        if (!SafeReadF32(p, &cur) || !SafeReadF32(p + 4, &mn) || !SafeReadF32(p + 8, &mxv)) continue;
                        if (!std::isfinite(cur) || !std::isfinite(mxv)) continue;
                        if (mn == 0.f && cur >= 20.f && cur <= mxv && (mxv == 100.f || mxv == 200.f) && !hp.count(p)) hp[p] = cur;
                        uint32_t z1 = 1, z2 = 1, z3 = 1;
                        if (cur > 0.f && mxv >= 1.5f && mxv <= 5.5f && cur < mxv && !tmUp.count(p)) {
                            if (SafeReadU32(p + 12, &z1) && SafeReadU32(p + 16, &z2) && SafeReadU32(p + 20, &z3) && z1 == 0 && z2 == 0 && z3 == 0) {
                                tmUp[p] = cur; mx[p] = mxv;
                            }
                        }
                        if (cur >= 1.2f && cur <= 4.5f && mn == 0.f && mxv == 0.f && !tmDn.count(p)) {
                            tmDn[p] = cur;
                        }
                    }
                }
            }
        }
        for (auto it = hp.begin(); it != hp.end();) {
            float nh = 0.f;
            if (!SafeReadF32(it->first, &nh) || !std::isfinite(nh) || nh < -100.f || nh > 205.f) { it = hp.erase(it); continue; }
            float oh = it->second;
            if (!g_StormEnt && oh - nh >= 1.5f && oh - nh < 95.f && nh >= -50.f) {
                float adj = fmaxf(0.f, oh - (oh - nh) * g_EnemyDmg);
                if (SafeWriteF32(it->first, adj)) it->second = adj;
            } else if (nh > oh && nh <= 200.f) {
                it->second = nh;
            }
            ++it;
        }
        if (g_Aggro > 1.01f && !g_StormEnt) {
            float bst = (g_Aggro - 1.f) * 0.75f;
            for (auto it = tmUp.begin(); it != tmUp.end();) {
                float nt = 0.f;
                if (!SafeReadF32(it->first, &nt) || !std::isfinite(nt) || nt < 0.f || nt > mx[it->first] + 1.f) { it = tmUp.erase(it); continue; }
                float ot = it->second, lm = mx[it->first], dt = nt - ot;
                if (dt >= 0.02f && dt <= 0.12f && nt > 0.f && nt < lm) {
                    float adv = fminf(lm, nt + dt * bst);
                    if (SafeWriteF32(it->first, adv)) it->second = adv;
                } else {
                    it->second = nt;
                }
                ++it;
            }
            for (auto it = tmDn.begin(); it != tmDn.end();) {
                float nt = 0.f;
                if (!SafeReadF32(it->first, &nt) || !std::isfinite(nt) || nt < 0.f || nt > 5.0f) { it = tmDn.erase(it); continue; }
                float ot = it->second, dt = ot - nt;
                if (dt >= 0.02f && dt <= 0.12f && nt > 0.15f) {
                    float adv = fmaxf(0.f, nt - dt * bst);
                    if (SafeWriteF32(it->first, adv)) it->second = adv;
                } else {
                    it->second = nt;
                }
                ++it;
            }
        }
    }
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); CreateThread(0, 0, Worker, 0, 0, 0); }
    return 1;
}
