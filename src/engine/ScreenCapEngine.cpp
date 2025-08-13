#include "Engine.hpp"
#include <ShlObj.h>
#include <Gdiplus.h>

/*Return -1 if fail, non-negative if success*/
int ScreenCapEngine::GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0;     
    UINT  size = 0;
    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if(size == 0) return -1; 

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if(pImageCodecInfo == NULL) return -1; 

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for(UINT j = 0; j < num; ++j) {
        if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 ) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }    
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

ScreenCapEngine::~ScreenCapEngine() {
    if(freelim >= 1) ReleaseDC(NULL, hDC);
    if(freelim >= 2) DeleteDC(hMemDC);
    if(freelim >= 3) DeleteObject(hBitmap);
    if(freelim >= 4) free(memblock);
    if(freelim >= 5) free(jpg_stat);
    if(freelim >= 6) rawbitmap_stream->Release();
    if(freelim >= 7) jpg_stream->Release();
}

/// @brief Capture screenshot
/// @param allocatedJPG [out] Pointer to the pointer contain allocated memory. Caller clean-up
/// @param allocatedsize [out] size of the allocated memory
/// @return 0 is success
/// Buffer must be cleaned up using C function free()
int WINAPI ScreenCapEngine::MakeBitmap(char **allocatedJPG, unsigned int* allocatedsize) {
    freelim = 0;

    BITMAPFILEHEADER bfHeader;
    BITMAPINFOHEADER biHeader;
    BITMAPINFO bInfo;
    HGDIOBJ hTempBitmap;
    BITMAP bAllDesktops;
    LONG width, height;
    BYTE *bBits = NULL;
    DWORD cbBits, dwWritten = 0;
    INT x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    INT y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int returncode = 0;

    ZeroMemory(&bfHeader, sizeof(BITMAPFILEHEADER));
    ZeroMemory(&biHeader, sizeof(BITMAPINFOHEADER));
    ZeroMemory(&bInfo, sizeof(BITMAPINFO));
    ZeroMemory(&bAllDesktops, sizeof(BITMAP));

    hDC = GetDC(NULL);
    if(hDC == NULL) {
        return 1;
    }
    freelim++;

    hTempBitmap = GetCurrentObject(hDC, OBJ_BITMAP);
    if(hTempBitmap == NULL) {
        return 2;
    }

    if(GetObjectW(hTempBitmap, sizeof(BITMAP), &bAllDesktops) == 0) {
        return 3;
    }

    width = bAllDesktops.bmWidth;
    height = bAllDesktops.bmHeight;

    DeleteObject(hTempBitmap);

    bfHeader.bfType = (WORD)('B' | ('M' << 8));
    bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biBitCount = 24;
    biHeader.biCompression = BI_RGB;
    biHeader.biPlanes = 1;
    biHeader.biWidth = width;
    biHeader.biHeight = height;

    bInfo.bmiHeader = biHeader;

    cbBits = (((24 * width + 31)&~31) / 8) * height;

    hMemDC = CreateCompatibleDC(hDC);
    if(hMemDC == NULL) {
        return 4;
    }
    freelim++;

    hBitmap = CreateDIBSection(hDC, &bInfo, DIB_RGB_COLORS, (VOID **)&bBits, NULL, 0);
    if(hBitmap == NULL) {
        return 5;    
    }
    freelim++;

    SelectObject(hMemDC, hBitmap);
    if(BitBlt(hMemDC, 0, 0, width, height, hDC, x, y, SRCCOPY) == 0) {
        return 6;
    }

    unsigned int memblocksize = (unsigned int) sizeof(BITMAPFILEHEADER) + (unsigned int) sizeof(BITMAPINFOHEADER) + cbBits;
    memblock = (char*) malloc(memblocksize);
    memcpy(memblock, &bfHeader, sizeof(BITMAPFILEHEADER));
    memcpy(memblock + sizeof(BITMAPFILEHEADER), &biHeader, sizeof(BITMAPINFOHEADER));
    memcpy(memblock + (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)), bBits, cbBits);
    freelim++;

    //compress

    CLSID pngClsid;

    LARGE_INTEGER __TEMP_SEEK_OFFSET;
    __TEMP_SEEK_OFFSET.QuadPart = 0;

    jpg_stat = (STATSTG*) malloc(sizeof(STATSTG));
    HRESULT hr;

    freelim++;

    rawbitmap_stream = NULL; 
    hr = CreateStreamOnHGlobal(NULL, TRUE, &rawbitmap_stream);
    if(hr == E_INVALIDARG) {
        return -5;
    } else if(hr == E_OUTOFMEMORY) {
        return -6;
    }
    freelim++;

    jpg_stream = NULL;
    hr = CreateStreamOnHGlobal(NULL, TRUE, &jpg_stream);
    if(hr == E_INVALIDARG) {
        return -7;
    } else if(hr == E_OUTOFMEMORY) {
        return -8;
    }
    freelim++;
    
    if(rawbitmap_stream->Write(memblock, memblocksize, &dwWritten) != S_OK) {
        return -1;
    }   

    Gdiplus::Image image(rawbitmap_stream);

    if(GetEncoderClsid(L"image/jpeg", &pngClsid) == -1) {
        return -2;
    }

    if(image.Save(jpg_stream, &pngClsid, NULL) != Gdiplus::Ok) {
        return -3;
    }

    jpg_stream->Stat(jpg_stat, 1); //specified no name
    *allocatedsize = (unsigned int) jpg_stat->cbSize.QuadPart;
    *allocatedJPG = (char*) malloc(*allocatedsize);

    //seek back to beginning to read
    if(jpg_stream->Seek(__TEMP_SEEK_OFFSET, STREAM_SEEK_SET, NULL) != S_OK) {
        free(allocatedJPG);
        return -4;
    }
    if(jpg_stream->Read(*allocatedJPG, *allocatedsize, &dwWritten) != S_OK) {
        free(*allocatedJPG);
        *allocatedJPG = nullptr;
        return -5;
    }

    return returncode;
}

int ScreenCapHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;
    if(!preliminaryEngineMessageCheck(MessageScreenCap, inputMessage, outputMessage, outputJson)) return 0;

    char *jpgData;
    unsigned int jpgDataSize;

    ScreenCapEngine screenCapEngine;
    int retcode = screenCapEngine.MakeBitmap(&jpgData, &jpgDataSize);
    if(retcode == 0) {
        outputMessage.commandNumber = MessageScreenCap;
        outputMessage.returnCode = 1;
        outputJson["errorString"] = "OK";
        outputMessage.setJsonData(outputJson);
        outputMessage.setBinaryData(jpgData, jpgDataSize);
        return 1;
    } else {
        outputMessage.commandNumber = MessageScreenCap;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = std::to_string(retcode);
        outputMessage.setJsonData(outputJson);
        return 0;
    }
}
