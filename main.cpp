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
static HWND g_lTorch = 0, g_lDmg = 0, g_lAggro = 0, g_BtnSave = 0;
static bool g_Visible = false;

typedef void(__thiscall* Move_t)(void*, float);
static Move_t oStormMove = (Move_t)0x004A6400;
static void** g_StormVT = 0;

struct StormHook {
    void __thiscall Move(float dt) {
        if (dt > 0.f && dt < 0.5f) dt *= g_TorchSpeed;
        oStormMove(this, dt);
    }
};

void* GetStormHookPtr() {
    union { void(__thiscall StormHook::*m)(float); void* p; } u;
    u.m = &StormHook::Move;
    return u.p;
}

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

void SafeHookStorm(uintptr_t ent) {
    __try {
        uint32_t vt = 0, hash = 0, ctrl = 0, cvt = 0;
        if (!SafeReadU32(ent, &vt) || vt != 0x00882B60) return;
        if (!SafeReadU32(ent + 0x50, &hash) || hash != 0x96478E0A) return;
        if (!SafeReadU32(ent + 0x58, &ctrl) || ctrl < 0x00A00000) return;
        if (!SafeReadU32(ctrl, &cvt) || cvt != 0x00881460) return;
        if (!g_StormVT) {
            void** t = (void**)VirtualAlloc(0, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (!t) return;
            memcpy(t, (void*)0x00881460, 128);
            if (*(t + 11) != (void*)0x004A6400) { VirtualFree(t, 0, MEM_RELEASE); return; }
            *(t + 11) = GetStormHookPtr();
            g_StormVT = t;
        }
        InterlockedExchangePointer((PVOID*)ctrl, (PVOID)g_StormVT);
    } __except (1) {}
}

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
    std::vector<char> b(128, 0);
    std::snprintf(b.data(), b.size(), "Johnny Storm Speed: %.2fx", g_TorchSpeed); SetWindowTextA(g_lTorch, b.data());
    std::snprintf(b.data(), b.size(), "Enemy Damage: %.2fx", g_EnemyDmg); SetWindowTextA(g_lDmg, b.data());
    std::snprintf(b.data(), b.size(), "Enemy Aggression: %.2fx", g_Aggro); SetWindowTextA(g_lAggro, b.data());
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

    g_BtnSave = CreateWindowA("BUTTON", "Save to INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 95, 175, 120, 26, g_Panel, (HMENU)101, wc.hInstance, 0);
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
                if (m.State == MEM_COMMIT && m.Type == MEM_PRIVATE && m.Protect == PAGE_READWRITE && m.RegionSize >= 256) {
                    uintptr_t b = (uintptr_t)m.BaseAddress, e = b + m.RegionSize - 128;
                    for (uintptr_t p = b + 4; p < e; p += 4) {
                        uint32_t u0 = 0;
                        if (!SafeReadU32(p, &u0)) break;
                        if (u0 == 0x00882B60) SafeHookStorm(p);
                        float cur = 0.f, mn = 0.f, mxv = 0.f;
                        if (!SafeReadF32(p, &cur) || !SafeReadF32(p + 4, &mn) || !SafeReadF32(p + 8, &mxv)) continue;
                        if (!std::isfinite(cur) || !std::isfinite(mxv)) continue;
                        if (mn == 0.f && cur >= 25.f && cur <= mxv && (mxv == 100.f || mxv == 200.f) && !hp.count(p)) {
                            uint32_t prevPtr = 0, flag = 99;
                            if (SafeReadU32(p - 4, &prevPtr) && SafeReadU32(p + 12, &flag) && prevPtr >= 0x00400000 && (flag == 0 || flag == 1)) hp[p] = cur;
                        }
                        uint32_t z1 = 1, z2 = 1, z3 = 1;
                        if (cur > 0.f && mxv >= 1.f && mxv < 8.f && cur < mxv && !tm.count(p)) {
                            if (SafeReadU32(p + 12, &z1) && SafeReadU32(p + 16, &z2) && SafeReadU32(p + 20, &z3) && z1 == 0 && z2 == 0 && z3 == 0) {
                                tm[p] = cur; mx[p] = mxv;
                            }
                        }
                    }
                }
            }
        }
        for (auto it = hp.begin(); it != hp.end();) {
            float nh = 0.f;
            if (!SafeReadF32(it->first, &nh) || !std::isfinite(nh) || nh < -100.f || nh > 205.f) { it = hp.erase(it); continue; }
            float oh = it->second;
            if (oh - nh >= 2.f && nh >= -50.f && oh - nh < 90.f) {
                float adj = fmaxf(0.f, oh - (oh - nh) * g_EnemyDmg);
                if (SafeWriteF32(it->first, adj)) it->second = adj;
            } else if (nh > oh && nh <= 200.f) it->second = nh;
            ++it;
        }
        if (g_Aggro > 1.01f) {
            float bst = g_Aggro - 1.f;
            for (auto it = tm.begin(); it != tm.end();) {
                float nt = 0.f;
                if (!SafeReadF32(it->first, &nt) || !std::isfinite(nt) || nt < 0.f || nt > mx[it->first] + 1.f) { it = tm.erase(it); continue; }
                float ot = it->second, lm = mx[it->first], dt = nt - ot;
                if (dt >= 0.015f && dt <= 0.18f && nt > 0.f && nt < lm) {
                    float adv = fminf(lm, nt + dt * bst);
                    if (SafeWriteF32(it->first, adv)) it->second = adv;
                } else it->second = nt;
                ++it;
            }
        }
    }
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); CreateThread(0, 0, Worker, 0, 0, 0); }
    return 1;
}
