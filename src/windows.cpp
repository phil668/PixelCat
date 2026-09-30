#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <memory>
#include <string>
#include "motion.h"
using namespace Gdiplus;
constexpr UINT TrayMessage=WM_APP+1;
constexpr UINT Pause=101, Show=102, Reset=103, Quit=104, Roam=105, AutoSleep=106, Nap=107;
HWND windowHandle; NOTIFYICONDATAW tray{};
std::unique_ptr<Bitmap> sprite;
std::unique_ptr<Bitmap> frames[4];
std::unique_ptr<Bitmap> specialImage;
std::wstring resourceDir;
Behavior behavior;
double lastTick=0;
UINT tickInterval=0;
void timer();
bool paused=false, shown=true, dragging=false;
POINT anchor{}, start{}; double tapped=-100;
int width=192,height=264, sizePercent=100;
const int sizes[]={50,75,100,125,150,200};
HDC canvas; HBITMAP surface; HGDIOBJ oldSurface; void *pixels;
double now() { return GetTickCount64()/1000.0; }
void render() {
    ZeroMemory(pixels,width*height*4);
    {
        Bitmap buffer(width,height,width*4,PixelFormat32bppPARGB,(BYTE*)pixels);
        Graphics g(&buffer); g.SetInterpolationMode(InterpolationModeNearestNeighbor);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        Pose p=pose(now(),tapped,paused);
        bool special=behavior.special>=0 && specialImage;
        if(!special) specialImage.reset();
        bool action=!special && (behavior.activity()==Activity::Walk || behavior.activity()==Activity::Sleep);
        Bitmap *img=special?specialImage.get():(action?frames[behavior.frame()].get():sprite.get());
        float factor=std::min(float(width)/img->GetWidth(),float(height-22)/img->GetHeight());
        float h=img->GetHeight()*factor,w=img->GetWidth()*factor;
        if(action && !behavior.sleeping && behavior.direction<0) { g.TranslateTransform((REAL)width,0); g.ScaleTransform(-1,1); }
        double sway=special?1.5*std::sin(behavior.clock*2.5):0;
        double stretch=special?1+0.005*std::sin(behavior.clock*1.8):(action?1:p.scaleY);
        g.DrawImage(img,RectF(float((width-w)/2+sway),float(height-3-h*stretch-((action||special)?0:p.rise)),w,float(h*stretch)));
    }
    RECT r; GetWindowRect(windowHandle,&r); POINT dst{r.left,r.top},src{0,0}; SIZE size{width,height};
    BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    UpdateLayeredWindow(windowHandle,NULL,&dst,&size,canvas,&src,0,&blend,ULW_ALPHA);
}
void timer() {
    KillTimer(windowHandle,1); lastTick=now(); tickInterval=behavior.sleeping?2000:125;
    if(!paused && shown) SetTimer(windowHandle,1,tickInterval,NULL);
}
void saveFlag(const wchar_t *name,bool value) {
    DWORD v=value; RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\PixelCat",name,REG_DWORD,&v,sizeof(v));
}
bool loadFlag(const wchar_t *name) {
    DWORD v=1,bytes=sizeof(v); RegGetValueW(HKEY_CURRENT_USER,L"Software\\PixelCat",name,RRF_RT_REG_DWORD,NULL,&v,&bytes); return v!=0;
}
void reset() {
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfo(MonitorFromWindow(windowHandle,MONITOR_DEFAULTTOPRIMARY),&mi);
    SetWindowPos(windowHandle,HWND_TOPMOST,mi.rcWork.right-width-20,mi.rcWork.bottom-height-10,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
}
bool resizeSurface(int percent) {
    int nextWidth=192*percent/100, nextHeight=264*percent/100;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=nextWidth; info.bmiHeader.biHeight=-nextHeight;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void *nextPixels=nullptr;
    HBITMAP next=CreateDIBSection(canvas,&info,DIB_RGB_COLORS,&nextPixels,NULL,0);
    if(!next) return false;
    HGDIOBJ previous=SelectObject(canvas,next);
    if(!surface) oldSurface=previous; else DeleteObject(surface);
    surface=next; pixels=nextPixels; width=nextWidth; height=nextHeight; sizePercent=percent;
    return true;
}
void changeSize(int percent) {
    RECT r; GetWindowRect(windowHandle,&r);
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfo(MonitorFromWindow(windowHandle,MONITOR_DEFAULTTONEAREST),&mi);
    if(!resizeSurface(percent)) return;
    int x=(int)clampOrigin((r.left+r.right-width)/2,mi.rcWork.left,mi.rcWork.right,width);
    int y=(int)clampOrigin(r.bottom-height,mi.rcWork.top,mi.rcWork.bottom,height);
    SetWindowPos(windowHandle,NULL,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
    render();
    DWORD saved=percent;
    RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\PixelCat",L"SizePercent",REG_DWORD,&saved,sizeof(saved));
}
void playSpecial(int index) {
    if(index<0 || index>=SpecialCount) return;
    const wchar_t *names[]={L"groom.png",L"flower.png",L"belly.png",L"downcast.png",L"stretch.png",L"doze.png"};
    Bitmap original((resourceDir+names[index]).c_str());
    if(original.GetLastStatus()!=Ok) { MessageBoxW(windowHandle,L"缺少动作素材，请完整解压新版程序。",L"毛毛",MB_ICONERROR); return; }
    double factor=512.0/std::max(original.GetWidth(),original.GetHeight());
    auto next=std::make_unique<Bitmap>((int)(original.GetWidth()*factor),(int)(original.GetHeight()*factor),PixelFormat32bppPARGB);
    if(next->GetLastStatus()!=Ok) return;
    { Graphics g(next.get()); g.SetInterpolationMode(InterpolationModeNearestNeighbor); g.DrawImage(&original,0,0,next->GetWidth(),next->GetHeight()); }
    specialImage=std::move(next); behavior.playSpecial(index); timer(); render();
}
void menu() {
    HMENU m=CreatePopupMenu();
    AppendMenuW(m,MF_STRING,Pause,paused?L"继续动画":L"暂停动画");
    AppendMenuW(m,MF_STRING|(behavior.roaming?MF_CHECKED:0),Roam,L"自动走动");
    AppendMenuW(m,MF_STRING|(behavior.autoSleep?MF_CHECKED:0),AutoSleep,L"两分钟后自动睡觉");
    AppendMenuW(m,MF_STRING,Nap,behavior.sleeping?L"唤醒毛毛":L"让毛毛睡觉");
    AppendMenuW(m,MF_STRING,Show,shown?L"隐藏毛毛":L"显示毛毛");
    HMENU poses=CreatePopupMenu();
    AppendMenuW(poses,MF_STRING,300,L"鸡腿");
    AppendMenuW(poses,MF_STRING,301,L"花花");
    AppendMenuW(poses,MF_STRING,302,L"翻肚皮");
    AppendMenuW(poses,MF_STRING,303,L"耍帅");
    AppendMenuW(poses,MF_STRING,304,L"趴着卖萌");
    AppendMenuW(poses,MF_STRING,305,L"蜷身小憩");
    AppendMenuW(m,MF_POPUP,(UINT_PTR)poses,L"动作");
    HMENU sizeMenu=CreatePopupMenu();
    for(int i=0;i<6;++i) {
        std::wstring label=std::to_wstring(sizes[i])+L"%"+(sizes[i]==100?L"（默认）":L"");
        AppendMenuW(sizeMenu,MF_STRING|(sizePercent==sizes[i]?MF_CHECKED:0),200+i,label.c_str());
    }
    AppendMenuW(m,MF_POPUP,(UINT_PTR)sizeMenu,L"调整大小");
    AppendMenuW(m,MF_STRING,Reset,L"恢复位置"); AppendMenuW(m,MF_SEPARATOR,0,NULL);
    AppendMenuW(m,MF_STRING,Quit,L"退出毛毛");
    POINT p; GetCursorPos(&p); SetForegroundWindow(windowHandle);
    UINT choice=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,windowHandle,NULL);
    DestroyMenu(m); PostMessage(windowHandle,WM_NULL,0,0);
    if(choice) SendMessage(windowHandle,WM_COMMAND,choice,0);
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    static UINT taskbar=RegisterWindowMessageW(L"TaskbarCreated");
    if(msg==taskbar) { Shell_NotifyIconW(NIM_ADD,&tray); return 0; }
    switch(msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_TIMER: {
        double t=now(),dt=t-lastTick; lastTick=t;
        RECT r; GetWindowRect(hwnd,&r); MONITORINFO mi{sizeof(mi)};
        GetMonitorInfo(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
        int x=(int)behavior.advance(dt,r.left,mi.rcWork.left,mi.rcWork.right,width,paused || GetCapture()==hwnd);
        if(x!=r.left) SetWindowPos(hwnd,NULL,x,r.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        render(); if(tickInterval!=(behavior.sleeping?2000U:125U)) timer(); return 0;
    }
    case WM_LBUTTONDOWN: { GetCursorPos(&anchor); RECT r; GetWindowRect(hwnd,&r); start={r.left,r.top}; dragging=false; SetCapture(hwnd); return 0; }
    case WM_MOUSEMOVE: if(GetCapture()==hwnd) { POINT p; GetCursorPos(&p); if(!dragging && abs(p.x-anchor.x)+abs(p.y-anchor.y)>3) { dragging=true; behavior.interact(); timer(); render(); } if(dragging) SetWindowPos(hwnd,NULL,start.x+p.x-anchor.x,start.y+p.y-anchor.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE); } return 0;
    case WM_LBUTTONUP: {
        if(GetCapture()!=hwnd) return 0; ReleaseCapture();
        if(!dragging) playSpecial(behavior.randomSpecial(GetTickCount()));
        RECT r; GetWindowRect(hwnd,&r); MONITORINFO mi{sizeof(mi)}; GetMonitorInfo(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
        SetWindowPos(hwnd,NULL,(int)clampOrigin(r.left,mi.rcWork.left,mi.rcWork.right,width),(int)clampOrigin(r.top,mi.rcWork.top,mi.rcWork.bottom,height),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        render(); return 0;
    }
    case WM_RBUTTONUP: menu(); return 0;
    case TrayMessage: if(l==WM_RBUTTONUP || l==WM_LBUTTONUP) menu(); return 0;
    case WM_COMMAND:
        if(LOWORD(w)>=300 && LOWORD(w)<300+SpecialCount) { playSpecial(LOWORD(w)-300); return 0; }
        if(LOWORD(w)>=200 && LOWORD(w)<206) { changeSize(sizes[LOWORD(w)-200]); return 0; }
        switch(LOWORD(w)) {
        case Roam: behavior.roaming=!behavior.roaming; saveFlag(L"Roaming",behavior.roaming); behavior.interact(); timer(); render(); break;
        case AutoSleep: behavior.autoSleep=!behavior.autoSleep; saveFlag(L"AutoSleep",behavior.autoSleep); break;
        case Nap: if(behavior.sleeping) behavior.interact(); else behavior.nap(); timer(); render(); break;
        case Pause: paused=!paused; timer(); render(); break;
        case Show: shown=!shown; ShowWindow(hwnd,shown?SW_SHOWNOACTIVATE:SW_HIDE); timer(); if(shown) render(); break;
        case Reset: reset(); render(); break;
        case Quit: DestroyWindow(hwnd); break;
        } return 0;
    case WM_DISPLAYCHANGE: reset(); render(); return 0;
    case WM_POWERBROADCAST: if(w==PBT_APMSUSPEND) KillTimer(hwnd,1); else if(w==PBT_APMRESUMEAUTOMATIC) timer(); return TRUE;
    case WM_DESTROY: KillTimer(hwnd,1); Shell_NotifyIconW(NIM_DELETE,&tray); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    HANDLE mutex=CreateMutexW(NULL,TRUE,L"Local\\PixelCatDesktopPet"); if(GetLastError()==ERROR_ALREADY_EXISTS) { CloseHandle(mutex); return 0; }
    SetProcessDPIAware();
    ULONG_PTR token; GdiplusStartupInput input; if(GdiplusStartup(&token,&input,NULL)!=Ok) return 1;
    wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL,path,MAX_PATH); std::wstring file(path); file=file.substr(0,file.find_last_of(L"\\/"))+L"\\cat.png";
    resourceDir=file.substr(0,file.find_last_of(L"\\/")+1);
    {
        Bitmap original(file.c_str()); if(original.GetLastStatus()!=Ok) { MessageBoxW(NULL,L"请将 cat.png 放在 PixelCat.exe 同一目录。",L"毛毛",MB_ICONERROR); GdiplusShutdown(token); return 1; }
        int h=512,w=original.GetWidth()*h/original.GetHeight(); sprite=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);
        Graphics g(sprite.get()); g.SetInterpolationMode(InterpolationModeNearestNeighbor); g.DrawImage(&original,0,0,w,h);
    }
    {
        std::wstring atlasPath=file.substr(0,file.find_last_of(L"\\/"))+L"\\actions.png";
        Bitmap original(atlasPath.c_str());
        if(original.GetLastStatus()!=Ok) { MessageBoxW(NULL,L"缺少 actions.png，请完整解压新版程序。",L"毛毛",MB_ICONERROR); return 1; }
        Bitmap atlas(512,512,PixelFormat32bppPARGB);
        { Graphics g(&atlas); g.SetInterpolationMode(InterpolationModeNearestNeighbor); g.DrawImage(&original,0,0,512,512); }
        for(int i=0;i<4;++i) {
            frames[i].reset(atlas.Clone((i%2)*256,(i/2)*256,256,256,PixelFormat32bppPARGB));
            if(!frames[i] || frames[i]->GetLastStatus()!=Ok) return 1;
        }
    }
    behavior.roaming=loadFlag(L"Roaming"); behavior.autoSleep=loadFlag(L"AutoSleep");
    WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=proc; wc.lpszClassName=L"PixelCatWindow"; wc.hCursor=LoadCursor(NULL,IDC_HAND); RegisterClassW(&wc);
    windowHandle=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,wc.lpszClassName,L"毛毛",WS_POPUP,0,0,width,height,NULL,NULL,instance,NULL);
    if(!windowHandle) return 1;
    canvas=CreateCompatibleDC(NULL);
    DWORD saved=100, bytes=sizeof(saved);
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\PixelCat",L"SizePercent",RRF_RT_REG_DWORD,NULL,&saved,&bytes);
    int initial=100; for(int value:sizes) if(saved==(DWORD)value) initial=value;
    if(!canvas || !resizeSurface(initial)) return 1;
    tray.cbSize=sizeof(tray); tray.hWnd=windowHandle; tray.uID=1; tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP; tray.uCallbackMessage=TrayMessage; tray.hIcon=LoadIcon(NULL,IDI_APPLICATION); wcscpy_s(tray.szTip,L"毛毛"); Shell_NotifyIconW(NIM_ADD,&tray);
    reset(); render(); ShowWindow(windowHandle,SW_SHOWNOACTIVATE); timer();
    MSG msg; while(GetMessageW(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    SelectObject(canvas,oldSurface); DeleteObject(surface); DeleteDC(canvas); sprite.reset(); specialImage.reset(); for(auto &frame:frames) frame.reset(); GdiplusShutdown(token); CloseHandle(mutex); return 0;
}
