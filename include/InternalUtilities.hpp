#ifndef internal_utility
#define internal_utility

#ifdef WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <errno.h>
#endif

#include <string>
#include <vector>
#include <array>

#ifndef UNREFERENCED_PARAMETER 
#define UNREFERENCED_PARAMETER(p) {(p) = (p);}
#endif

#ifdef WIN32
inline bool WideStringToString(const std::wstring& wstr, std::string& str) {
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

inline bool StringToWideString(const std::string& str, std::wstring& wstr) {
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
#endif

template <class T> void ComSafeRelease(T **ppT)
{
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

inline void PrintHexByteArray(const std::string& prefix, uint8_t *arr, int size, bool spacing = false) {
    printf("%s ", prefix.c_str());
    if(spacing) for(int i=0; i < size; i++) printf("%02x ", arr[i]);
    else for(int i=0; i < size; i++) printf("%02x", arr[i]);
    printf("\n");
}

inline bool memcpyToVector(std::vector<uint8_t>& dest, const void* src, size_t byte_count) {    
    size_t destByteCount = dest.size() / sizeof(uint8_t);
    if(destByteCount < byte_count) {
        ::memcpy(dest.data(), src, destByteCount);
        return false;
    } else {
        ::memcpy(dest.data(), src, byte_count);
        return true;
    }
}

template <size_t N>
bool memcpyToArray(std::array<uint8_t, N>& dest, const void* src, size_t byte_count) {    
    if(N < byte_count) {
        ::memcpy(dest.data(), src, N);
        return false;
    } else {
        ::memcpy(dest.data(), src, byte_count);
        return true;
    }
}

inline void* secureZeroMemory(void* ptr, size_t num) {
    if(!ptr) return nullptr;
    #ifdef WIN32
    return SecureZeroMemory(ptr, num);
    #else 
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (num--) *p++ = 0;
    return p;
    #endif
}

inline std::string getCurrentIsoTime() {
    using namespace std::chrono;
    
    // Get current time
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);
    
    // Convert to tm struct
    struct tm *tm = localtime(&timer);
    
    // Format as string
    char buffer[81];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H-%M-%S", tm);

    std::string tstr = std::string(buffer);
    snprintf(buffer, 80, "%s.%.3lld", tstr.c_str(), ms.count());
    tstr = std::string(buffer);
    return tstr;
}

inline bool CreateDirectoryCrossPlatform(const std::string& path) {
    std::wstring wpath;
    (void) StringToWideString(path, wpath);
    #ifdef WIN32
        if(CreateDirectoryW(wpath.c_str(), NULL)) {
            return true;
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_ALREADY_EXISTS) {
                return true;
            } else {
                return false;
            }
        }
    #else // Linux/Unix-like systems
        if (mkdir(path.c_str(), 0777) == 0) {
            return true;
        } else {
            if (errno == EEXIST) {
                return true;
            } else {
                return false;
            }
        }
    #endif
}

inline void allocateConsoleWin() {
    if(AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
}

#endif