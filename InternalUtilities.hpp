#ifndef internal_utility
#define internal_utility

#include <windows.h>
#include <string>

#ifndef UNREFERENCED_PARAMETER 
#define UNREFERENCED_PARAMETER(p) {(p) = (p);}
#endif

bool WideStringToString(const std::wstring& wstr, std::string& str) {
    unsigned int len;
    int res = 0, lasterror = 0;
    UNREFERENCED_PARAMETER(lasterror);
    len = WideCharToMultiByte(65001, 0, wstr.c_str(), (int) wstr.length(), nullptr, 0, nullptr, nullptr);
    char *buf = new char[len+3];
    memset(buf, 0, len+3);
    res = WideCharToMultiByte(65001, 0, wstr.c_str(), (int) wstr.length(), buf, len, nullptr, nullptr);
    if(res == 0) lasterror = GetLastError();
    if(res) {
        str = std::string(buf, len);
        delete[] buf;
    }
    return (bool) res;
}

bool StringToWideString(const std::string& str, std::wstring& wstr) {
    unsigned int len;
    int res = 0, lasterror = 0;
    UNREFERENCED_PARAMETER(lasterror);
    len = MultiByteToWideChar(65001, 0, str.c_str(), (int) str.length(), nullptr, 0);
    wchar_t *buf = new wchar_t[len+1];
    memset(buf, 0, len*2+2);
    res = MultiByteToWideChar(65001, 0, str.c_str(), (int) str.length(), buf, len);
    if(res == 0) lasterror = GetLastError();
    if(res) {
        wstr = std::wstring(buf, len);
        delete[] buf;
    }
    return (bool) res;
}

template <class T> void ComSafeRelease(T **ppT)
{
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

void PrintHexByteArray(const std::string& prefix, uint8_t *arr, int size, bool spacing = false) {
    printf("%s ", prefix.c_str());
    if(spacing) for(int i=0; i < size; i++) printf("%02x ", arr[i]);
    else for(int i=0; i < size; i++) printf("%02x", arr[i]);
    printf("\n");
}

#endif