#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unordered_map>
#pragma comment(lib,"comctl32.lib")

static float g_SpideySpeed=1.35f,g_TorchSpeed=1.65f,g_EnemyDmg=3.5f,g_Aggro=1.54f;
static bool g_Menu=false,g_FreeCam=false,g_PrevCam=false,g_TorchBoost=false,g_CombatActive=false;
static HWND g_Panel=0,g_Lbl0=0,g_Lbl1=0,g_Lbl2=0,g_Lbl3=0,g_Trk0=0,g_Trk1=0,g_Trk2=0,g_Trk3=0,g_ChkTorch=0,g_ChkCam=0,g_ChkCombat=0;
static std::unordered_map<uintptr_t,float> g_Hp;
static float g_PrevStormX=0,g_PrevStormY=0,g_PrevStormZ=0;
static bool g_HasStormPrev=false;

typedef void(__cdecl*AdvAI_t)(float);
static AdvAI_t oAdvAI=0;
typedef int(__thiscall*AdvAnim_t)(void*,void*,float,float,float,float);
static AdvAnim_t oAdvAnim=0;
typedef uintptr_t(__cdecl*FindEnt_t)(const uint32_t*,int,bool);
static FindEnt_t fnFindEnt=(FindEnt_t)0x004DC300;
typedef void(__cdecl*SetPos_t)(uintptr_t,const float*);
static SetPos_t fnSetPos=(SetPos_t)0x004E1230;
static uint8_t* g_TrampAnim=0;

void SaveIni(){
    FILE* f=fopen("HardcoreMod.ini","w");
    if(f){
        fprintf(f,"SPIDEY_SPEED=%.2f\nENEMY_DMG=%.2f\nTORCH_SPEED=%.2f\nENEMY_AGGRO=%.2f\n",
                g_SpideySpeed,g_EnemyDmg,g_TorchSpeed,g_Aggro);
        fclose(f);
    }
}
inline bool CanRead(uintptr_t p,size_t sz){return p>=0x00400000&&p<0x7FEE0000&&!IsBadReadPtr((void*)p,sz);}
inline bool CanWrite(uintptr_t p,size_t sz){return p>=0x00A00000&&p<0x7FEE0000&&!IsBadWritePtr((void*)p,sz);}
uintptr_t GetHero(){
    if(!CanRead(0x0095C770,4))return 0;
    uintptr_t w=*(uintptr_t*)0x0095C770;
    if(!CanRead(w+0x234,4))return 0;
    return *(uintptr_t*)(w+0x230);
}
void UpdateCam(){
    if(!g_FreeCam&&!g_PrevCam)return;
    __try{
        uintptr_t g=*(uintptr_t*)0x009682E0;
        if(CanWrite(g+0x172,1)){*(uint8_t*)(g+0x172)=g_FreeCam?1:0;g_PrevCam=g_FreeCam;}
        if(!g_FreeCam)return;
        uintptr_t ml=*(uintptr_t*)0x0095C17C;
        uintptr_t cam=CanRead(ml,0x18)?*(uintptr_t*)(ml+0x10):0;
        if(!cam&&CanRead(g+0x64,4))cam=*(uintptr_t*)(g+0x60);
        if(!CanRead(cam,0x18))return;
        uintptr_t po=*(uintptr_t*)(cam+0x14);
        if(!CanWrite(po,0x40))return;
        float spd=(GetAsyncKeyState(VK_SHIFT)&0x8000)?4.5f:1.1f;
        float fwd=0,str=0,up=0,yaw=0,pit=0;
        if(GetAsyncKeyState(0x49)&0x8000)fwd+=spd;
        if(GetAsyncKeyState(0x4B)&0x8000)fwd-=spd;
        if(GetAsyncKeyState(0x4C)&0x8000)str+=spd;
        if(GetAsyncKeyState(0x4A)&0x8000)str-=spd;
        if((GetAsyncKeyState(0x55)&0x8000)||(GetAsyncKeyState(VK_PRIOR)&0x8000))up+=spd;
        if((GetAsyncKeyState(0x4F)&0x8000)||(GetAsyncKeyState(VK_NEXT)&0x8000))up-=spd;
        if(GetAsyncKeyState(VK_LEFT)&0x8000)yaw-=0.045f;
        if(GetAsyncKeyState(VK_RIGHT)&0x8000)yaw+=0.045f;
        if(GetAsyncKeyState(VK_UP)&0x8000)pit-=0.035f;
        if(GetAsyncKeyState(VK_DOWN)&0x8000)pit+=0.035f;
        if(yaw!=0){
            float c=cosf(yaw),s=sinf(yaw);
            for(int r=0;r<3;++r){
                float x=*(float*)(po+r*16),z=*(float*)(po+r*16+8);
                *(float*)(po+r*16)=x*c-z*s;*(float*)(po+r*16+8)=x*s+z*c;
            }
        }
        if(pit!=0){
            float c=cosf(pit),s=sinf(pit);
            float y1=*(float*)(po+0x14),y2=*(float*)(po+0x24);
            *(float*)(po+0x14)=y1*c-y2*s;*(float*)(po+0x24)=y1*s+y2*c;
        }
        float rx=*(float*)(po+0x00),ry=*(float*)(po+0x04),rz=*(float*)(po+0x08);
        float fx=*(float*)(po+0x20),fy=*(float*)(po+0x24),fz=*(float*)(po+0x28);
        float xyz_buf_0=*(float*)(po+0x30)+fx*fwd+rx*str;
        float xyz_buf_1=*(float*)(po+0x34)+fy*fwd+ry*str+up;
        float xyz_buf_2=*(float*)(po+0x38)+fz*fwd+rz*str;
        fnSetPos(cam,&xyz_buf_0);
    }__except(1){}
}
void UpdateStorm(){
    if(!g_TorchBoost){g_HasStormPrev=false;return;}
    __try{
        uint32_t sh=0x96478E0A;
        uintptr_t act=fnFindEnt(&sh,29,true);
        if(!CanRead(act,0x18)){g_HasStormPrev=false;return;}
        uintptr_t po=*(uintptr_t*)(act+0x14);
        if(CanWrite(po,0x40)){
            float x=*(float*)(po+0x30),y=*(float*)(po+0x34),z=*(float*)(po+0x38);
            if(g_HasStormPrev&&g_TorchSpeed>1.01f){
                float dx=x-g_PrevStormX,dy=y-g_PrevStormY,dz=z-g_PrevStormZ,d2=dx*dx+dy*dy+dz*dz;
                if(d2>0.0001f&&d2<400.f){
                    float m=g_TorchSpeed-1.f;
                    float nx=x+dx*m,ny=y+dy*m,nz=z+dz*m;
                    fnSetPos(act,&nx);
                    x=nx;y=ny;z=nz;
                }
            }
            g_PrevStormX=x;g_PrevStormY=y;g_PrevStormZ=z;g_HasStormPrev=true;
        }
    }__except(1){g_HasStormPrev=false;}
}
void UpdateDamage(){
    if(!g_CombatActive||g_EnemyDmg<=1.01f)return;
    __try{
        uintptr_t hero=GetHero();
        if(!hero)return;
        uintptr_t vec=*(uintptr_t*)0x0095A660;
        if(!CanRead(vec,16))return;
        uintptr_t beg=*(uintptr_t*)(vec+4),end=*(uintptr_t*)(vec+8);
        if(beg>=end||(end-beg)>4096||!CanRead(beg,end-beg))return;
        for(uintptr_t it=beg;it<end;it+=4){
            uintptr_t dam=*(uintptr_t*)it;
            if(!CanWrite(dam,0x210))continue;
            if(*(uintptr_t*)(dam+0x04)==hero){
                float cur=*(float*)(dam+0x1FC),mn=*(float*)(dam+0x200),mx=*(float*)(dam+0x204);
                if(mx>=50.f&&mx<=1000.f&&cur>=mn&&cur<=mx){
                    if(!g_Hp.count(dam))g_Hp[dam]=cur;
                    float oh=g_Hp[dam];
                    if(oh-cur>=1.f&&oh-cur<300.f){
                        float adj=fmaxf(mn,oh-(oh-cur)*g_EnemyDmg);
                        *(float*)(dam+0x1FC)=adj;g_Hp[dam]=adj;
                    }else g_Hp[dam]=cur;
                }
            }
        }
    }__except(1){}
}
void __cdecl hkAdvAI(float dt){
    UpdateCam();
    UpdateStorm();
    UpdateDamage();
    if(g_CombatActive&&g_Aggro>1.01f)dt*=g_Aggro;
    oAdvAI(dt);
}
float ScaleAnimDt(void* self,float dt){
    if(!g_CombatActive&&!g_TorchBoost)return dt;
    __try{
        if(!CanRead((uintptr_t)self,8))return dt;
        uintptr_t ctrl=*(uintptr_t*)((uintptr_t)self+4);
        if(!CanRead(ctrl,12))return dt;
        uintptr_t act=*(uintptr_t*)(ctrl+8);
        if(!CanRead(act,0x14))return dt;
        uintptr_t hero=GetHero();
        uint32_t hash=*(uint32_t*)(act+0x10);
        if(act==hero){if(g_CombatActive&&g_SpideySpeed>1.01f)return dt*g_SpideySpeed;}
        else if(hash==0x96478E0A){if(g_TorchBoost&&g_TorchSpeed>1.01f)return dt*g_TorchSpeed;}
        else{if(g_CombatActive&&g_Aggro>1.01f)return dt*g_Aggro;}
    }__except(1){}
    return dt;
}
int __fastcall hkAdvAnim(void* self,void*,void* inst,float dt,float a4,float a5,float a6){
    dt=ScaleAnimDt(self,dt);
    return oAdvAnim(self,inst,dt,a4,a5,a6);
}
void InstallHooks(){
    DWORD op;
    if(VirtualProtect((void*)0x00558442,8,PAGE_EXECUTE_READWRITE,&op)){
        int32_t rel=*(int32_t*)0x00558443;
        oAdvAI=(AdvAI_t)(0x00558447+rel);
        *(int32_t*)0x00558443=(int32_t)((uintptr_t)hkAdvAI-0x00558447);
        VirtualProtect((void*)0x00558442,8,op,&op);
    }
    g_TrampAnim=(uint8_t*)VirtualAlloc(0,32,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(g_TrampAnim&&VirtualProtect((void*)0x0049C090,8,PAGE_EXECUTE_READWRITE,&op)){
        memcpy(g_TrampAnim,(void*)0x0049C090,7);
        *(g_TrampAnim+7)=0xE9;
        *(int32_t*)(g_TrampAnim+8)=(int32_t)(0x0049C097-(uintptr_t)(g_TrampAnim+12));
        oAdvAnim=(AdvAnim_t)g_TrampAnim;
        *(uint8_t*)0x0049C090=0xE9;
        *(int32_t*)0x0049C091=(int32_t)((uintptr_t)hkAdvAnim-0x0049C095);
        *(uint8_t*)0x0049C095=0x90;*(uint8_t*)0x0049C096=0x90;
        VirtualProtect((void*)0x0049C090,8,op,&op);
    }
}
void UpdateLabels(){
    char* b=(char*)malloc(64);
    sprintf(b,"Spider-Man Speed: %.2fx",g_SpideySpeed);SetWindowTextA(g_Lbl0,b);
    sprintf(b,"Johnny Storm Speed: %.2fx",g_TorchSpeed);SetWindowTextA(g_Lbl1,b);
    sprintf(b,"Enemy & Boss Damage: %.2fx",g_EnemyDmg);SetWindowTextA(g_Lbl2,b);
    sprintf(b,"Enemy & Boss Aggression: %.2fx",g_Aggro);SetWindowTextA(g_Lbl3,b);
    free(b);
}
LRESULT CALLBACK PanelProc(HWND h,UINT m,WPARAM wp,LPARAM lp){
    if(m==WM_HSCROLL){
        g_SpideySpeed=SendMessageA(g_Trk0,TBM_GETPOS,0,0)/100.f;
        g_TorchSpeed=SendMessageA(g_Trk1,TBM_GETPOS,0,0)/100.f;
        g_EnemyDmg=SendMessageA(g_Trk2,TBM_GETPOS,0,0)/100.f;
        g_Aggro=SendMessageA(g_Trk3,TBM_GETPOS,0,0)/100.f;
        UpdateLabels();
    }
    if(m==WM_COMMAND){
        int id=LOWORD(wp);
        if(id==101)g_TorchBoost=(SendMessageA(g_ChkTorch,BM_GETCHECK,0,0)==BST_CHECKED);
        if(id==102)g_FreeCam=(SendMessageA(g_ChkCam,BM_GETCHECK,0,0)==BST_CHECKED);
        if(id==104){g_CombatActive=(SendMessageA(g_ChkCombat,BM_GETCHECK,0,0)==BST_CHECKED);if(!g_CombatActive)g_Hp.clear();}
        if(id==103){SaveIni();SetWindowTextA((HWND)lp,"Saved!");}
    }
    if(m==WM_CLOSE){g_Menu=false;ShowWindow(h,SW_HIDE);return 0;}
    return DefWindowProcA(h,m,wp,lp);
}
void InitPanel(){
    INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_BAR_CLASSES};
    InitCommonControlsEx(&ic);
    WNDCLASSA wc={0,PanelProc,0,0,GetModuleHandleA(0),0,LoadCursorA(0,IDC_ARROW),(HBRUSH)(COLOR_WINDOW+1),0,"USM_HC_PANEL"};
    RegisterClassA(&wc);
    g_Panel=CreateWindowExA(WS_EX_TOPMOST,"USM_HC_PANEL","USM Hardcore Mod (F1)",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,40,40,340,415,0,0,wc.hInstance,0);
    g_ChkCombat=CreateWindowA("BUTTON","Enable Combat & Speed Boost (In-City)",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,15,10,300,22,g_Panel,(HMENU)104,wc.hInstance,0);
    g_Lbl0=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,15,38,300,20,g_Panel,0,wc.hInstance,0);
    g_Trk0=CreateWindowA("msctls_trackbar32","",WS_CHILD|WS_VISIBLE|TBS_HORZ,15,58,300,28,g_Panel,0,wc.hInstance,0);
    SendMessageA(g_Trk0,TBM_SETRANGE,1,MAKELONG(100,500));SendMessageA(g_Trk0,TBM_SETPOS,1,135);
    g_Lbl1=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,15,92,300,20,g_Panel,0,wc.hInstance,0);
    g_Trk1=CreateWindowA("msctls_trackbar32","",WS_CHILD|WS_VISIBLE|TBS_HORZ,15,112,300,28,g_Panel,0,wc.hInstance,0);
    SendMessageA(g_Trk1,TBM_SETRANGE,1,MAKELONG(100,1000));SendMessageA(g_Trk1,TBM_SETPOS,1,165);
    g_ChkTorch=CreateWindowA("BUTTON","Torch Speed Boost (Race)",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,15,144,280,22,g_Panel,(HMENU)101,wc.hInstance,0);
    g_Lbl2=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,15,174,300,20,g_Panel,0,wc.hInstance,0);
    g_Trk2=CreateWindowA("msctls_trackbar32","",WS_CHILD|WS_VISIBLE|TBS_HORZ,15,194,300,28,g_Panel,0,wc.hInstance,0);
    SendMessageA(g_Trk2,TBM_SETRANGE,1,MAKELONG(100,500));SendMessageA(g_Trk2,TBM_SETPOS,1,350);
    g_Lbl3=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,15,228,300,20,g_Panel,0,wc.hInstance,0);
    g_Trk3=CreateWindowA("msctls_trackbar32","",WS_CHILD|WS_VISIBLE|TBS_HORZ,15,248,300,28,g_Panel,0,wc.hInstance,0);
    SendMessageA(g_Trk3,TBM_SETRANGE,1,MAKELONG(100,1000));SendMessageA(g_Trk3,TBM_SETPOS,1,154);
    g_ChkCam=CreateWindowA("BUTTON","Free Cam (IJKL + UO + Arrows + Shift)",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,15,282,300,22,g_Panel,(HMENU)102,wc.hInstance,0);
    CreateWindowA("BUTTON","Save to INI",WS_CHILD|WS_VISIBLE,15,315,120,28,g_Panel,(HMENU)103,wc.hInstance,0);
    UpdateLabels();
}
DWORD WINAPI Worker(LPVOID){
    InitPanel();bool wasKey=false;MSG msg;
    for(;;){
        while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageA(&msg);}
        Sleep(25);
        bool key=(GetAsyncKeyState(VK_F1)&0x8000)||(GetAsyncKeyState(VK_INSERT)&0x8000);
        if(key&&!wasKey){g_Menu=!g_Menu;ShowWindow(g_Panel,g_Menu?SW_SHOW:SW_HIDE);if(g_Menu)SetForegroundWindow(g_Panel);}
        wasKey=key;
    }
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID){
    if(r==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(h);
        InstallHooks();
        CreateThread(0,1048576,Worker,0,0,0);
    }
    return 1;
}
