#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static float g_TorchSpeed = 1.65f;
static float g_EnemyDmg = 3.5f;
static float g_Aggro = 1.54f;
static bool g_Menu = false;
static bool g_Init = false;
static HWND g_Hwnd = 0;
static WNDPROC g_OrigWnd = 0;

typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
static EndScene_t oEndScene = 0;
typedef HRESULT(__stdcall* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static Reset_t oReset = 0;
typedef void(__thiscall* Move_t)(void*, float);
static Move_t oStormMove = (Move_t)0x004A6400;

static void** g_StormVT = 0;
static void* g_HookedCtrl = 0;

void LoadIni() {
    FILE* f = fopen("HardcoreMod.ini", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        float v = 0.f;
        if (sscanf(line, "ENEMY_DMG=%f", &v) == 1) g_EnemyDmg = v;
        else if (sscanf(line, "TORCH_SPEED=%f", &v) == 1) g_TorchSpeed = v;
        else if (sscanf(line, "ENEMY_ATTACK_DELAY=%f", &v) == 1 && v > 0.01f) g_Aggro = 1.0f / v;
    }
    fclose(f);
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

DWORD WINAPI Worker(LPVOID) {
    std::unordered_map<uintptr_t, float> hp, tm, mx;
    int tk = 0;
    for (;;) {
        Sleep(50);
        if ((++tk % 40) == 1) {
            MEMORY_BASIC_INFORMATION m;
            for (uintptr_t a = 0x00A00000; a < 0x7FEE0000; a = (uintptr_t)m.BaseAddress + m.RegionSize) {
                if (!VirtualQuery((void*)a, &m, sizeof(m))) break;
                if (m.State == MEM_COMMIT && m.Type == MEM_PRIVATE && m.Protect == PAGE_READWRITE) {
                    uintptr_t b = (uintptr_t)m.BaseAddress, e = b + m.RegionSize - 32;
                    for (uintptr_t p = b; p < e; p += 4) {
                        if (*(uint32_t*)p == 0x00882B60 && *(uint32_t*)(p + 0x50) == 0x96478E0A) {
                            void* c = *(void**)(p + 0x58);
                            if (c && *(uint32_t*)c == 0x00881460 && c != g_HookedCtrl) {
                                if (!g_StormVT) g_StormVT = (void**)malloc(128);
                                memcpy(g_StormVT, (void*)0x00881460, 128);
                                *(g_StormVT + 11) = (void*)hkStormMove;
                                *(void**)c = g_StormVT;
                                g_HookedCtrl = c;
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

LRESULT CALLBACK hkWndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_KEYDOWN && (wp == VK_F1 || wp == VK_INSERT)) g_Menu = !g_Menu;
    if (g_Menu && ImGui_ImplWin32_WndProcHandler(h, m, wp, lp)) return 1;
    return CallWindowProcA(g_OrigWnd, h, m, wp, lp);
}

HRESULT __stdcall hkEndScene(IDirect3DDevice9* d) {
    if (!g_Init) {
        D3DDEVICE_CREATION_PARAMETERS cp;
        d->GetCreationParameters(&cp);
        g_Hwnd = cp.hFocusWindow;
        g_OrigWnd = (WNDPROC)SetWindowLongA(g_Hwnd, GWL_WNDPROC, (LONG)hkWndProc);
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(g_Hwnd);
        ImGui_ImplDX9_Init(d);
        g_Init = true;
    }
    if (g_Menu) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("USM Hardcore Mod (F1)", &g_Menu);
        ImGui::SliderFloat("Johnny Storm Speed", &g_TorchSpeed, 1.0f, 3.0f, "%.2fx");
        ImGui::SliderFloat("Enemy Damage", &g_EnemyDmg, 1.0f, 5.0f, "%.2fx");
        ImGui::SliderFloat("Enemy Aggression", &g_Aggro, 1.0f, 3.0f, "%.2fx");
        if (ImGui::Button("Save to INI")) SaveIni();
        ImGui::End();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    return oEndScene(d);
}

HRESULT __stdcall hkReset(IDirect3DDevice9* d, D3DPRESENT_PARAMETERS* p) {
    if (g_Init) ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT r = oReset(d, p);
    if (g_Init) ImGui_ImplDX9_CreateDeviceObjects();
    return r;
}

DWORD WINAPI InitDX(LPVOID) {
    LoadIni();
    Sleep(3000);
    CreateThread(0, 0, Worker, 0, 0, 0);
    HWND gameHwnd = 0;
    while (!gameHwnd) {
        gameHwnd = FindWindowA(0, "Ultimate Spider-Man");
        Sleep(200);
    }
    IDirect3D9* d3 = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3) {
        D3DPRESENT_PARAMETERS pp = { 0 };
        pp.Windowed = 1;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = gameHwnd;
        IDirect3DDevice9* dv = 0;
        if (SUCCEEDED(d3->CreateDevice(0, D3DDEVTYPE_HAL, gameHwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dv))) {
            void** vt = *(void***)dv;
            DWORD op;
            VirtualProtect(vt, 200, PAGE_EXECUTE_READWRITE, &op);
            oEndScene = (EndScene_t)(*(vt + 42));
            oReset = (Reset_t)(*(vt + 16));
            *(vt + 42) = (void*)hkEndScene;
            *(vt + 16) = (void*)hkReset;
            dv->Release();
        }
        d3->Release();
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        CreateThread(0, 0, InitDX, 0, 0, 0);
    }
    return 1;
}
