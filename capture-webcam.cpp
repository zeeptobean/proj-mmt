//link with -lmf -lmfplat -lmfreadwrite -lmfuuid -lshlwapi -lole32 -loleaut32 -lrpcrt4

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <shlwapi.h>
#include <codecapi.h>

#include <iostream>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <exception>
#include <chrono>
using namespace std;

#ifndef UNREFERENCED_PARAMETER 
#define UNREFERENCED_PARAMETER(p) {(p) = (p);}
#endif

//Caller cleanup cstr
/*
bool MyWideCharToMultiByte(wchar_t *wcstr, unsigned int wlen, char *cstr, unsigned int *len) {
    *len = WideCharToMultiByte(65001, 0, wcstr, wlen, NULL, 0, NULL, NULL);
    return (bool) WideCharToMultiByte(65001, 0, wcstr, wlen, cstr, *len, NULL, NULL);
}
*/

#define REFLECTION(x) #x

#define MF_VIDEO_FORMAT_ENTRY(guid_constant) {guid_constant, #guid_constant}

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

//width, height = 0 -> size from source
struct VideoConfig {
    unsigned int width, height;
    unsigned int fps;
    unsigned int bitrate;
    GUID encodingFormat = MFVideoFormat_H264;
};

class neeko {
    private:
    IMFMediaSource *mediaSource = nullptr;
    IMFAttributes *sourceAttributes = nullptr, *writerAttributes = nullptr;
    IMFActivate **ppDevices = nullptr;
    IMFSourceReader *sourceReader = nullptr;
    IMFMediaType *mediaType = nullptr, *nativeMediaType = nullptr, *sourceReaderOutput = nullptr, *sinkWriterInput = nullptr;
    IMFSinkWriter *sinkWriter = nullptr;
    DWORD streamIndex;
    unsigned int deviceCount = 0;
    int allocated = 0;
    HRESULT res;

    void invokeException(std::string functionName, int line, std::string context = "<no context>") {
        std::string str = "Exception in line " + to_string(line) + ", function " + functionName + ", context: " + context + ". ";
        char buf[201];
        memset(buf, 0, sizeof buf);
        snprintf(buf, 200, "Last HRESULT code: %X", (int) res);
        str += std::string(buf);
        throw std::runtime_error(str);
    }

    void computeRatio(const unsigned int& width, const unsigned int& height, unsigned int& widthRatio, unsigned int& heightRatio) {
        unsigned int factor = std::__gcd(width, height);
        widthRatio = width / factor;
        heightRatio = height / factor;
    }

    void printMFAttributeInformation(IMFAttributes *attr, const std::string& extraInfo = "") {
        unsigned int stringLength, symbolicLength;
        wchar_t *wcharName = nullptr, *wcharSymbolicName = nullptr;
        std::wstring wstrName, wstrSymbolic;
        std::string strName = "<null>", strSymbolic = "<null>";

        attr->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &wcharName, &stringLength);
        attr->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &wcharSymbolicName, &symbolicLength);

        if(wcharName) {
            wstrName = std::wstring(wcharName, stringLength);
            WideStringToString(wstrName, strName);
            CoTaskMemFree(wcharName);
        }
        if(wcharSymbolicName) {
            wstrSymbolic = std::wstring(wcharSymbolicName, symbolicLength);
            WideStringToString(wstrSymbolic, strSymbolic);
            CoTaskMemFree(wcharSymbolicName);
        }
        std::cout << "Attribute information:\nName: " << strName << "\nSymbolic link: " << strSymbolic << "\nUser extra info: " << extraInfo << std::endl;
    }

    void createVideoDeviceSource() {
        // Create an attribute store to specify the enumeration parameters.
        res = MFCreateAttributes(&sourceAttributes, 1);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;

        // Source type: video capture devices
        res = sourceAttributes->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
            );
        if (FAILED(res)) {
            invokeException(__func__, __LINE__);
        }

        // Enumerate devices.
        res = MFEnumDeviceSources(sourceAttributes, &ppDevices, &deviceCount);
        if (FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;

        if(deviceCount == 0) {
            res = E_FAIL;
            invokeException(__func__, __LINE__, "No device found");
        } else {
            cout << "Detected " << deviceCount << "\n";
            for(unsigned i=0; i < deviceCount; i++) {
                printMFAttributeInformation(ppDevices[i], "Device");
            }
        }


        // Create the media source object.
        res = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&mediaSource));
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;

    }

    void getSource() {
        //SourceReader: read webcam data then convert to raw video 
        res = MFCreateSourceReaderFromMediaSource(mediaSource, sourceAttributes, &sourceReader);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;

        res = sourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nativeMediaType);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;


        GUID nativeMajorType, nativeMinorType;
        res = nativeMediaType->GetGUID(MF_MT_MAJOR_TYPE, &nativeMajorType);
        res = nativeMediaType->GetGUID(MF_MT_MAJOR_TYPE, &nativeMinorType);

        res = MFCreateMediaType(&mediaType);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;

        res = mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        //TODO: virtual camera is NV12, test if MFVideoFormat_RGB32 work on real cam otherwise...
        res = mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);    

        res = sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mediaType);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        //set then get: SourceReader will auto convert to approriate format
        res = sourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &sourceReaderOutput);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
    }

    void configOutput() {
        std::wstring wfilename;
        StringToWideString(filename, wfilename);

        res = MFCreateAttributes(&writerAttributes, 1);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;
        res = writerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);  //HW acceleration

        res = MFCreateSinkWriterFromURL(wfilename.c_str(), nullptr, writerAttributes, &sinkWriter);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;

        res = MFCreateMediaType(&sinkWriterInput);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
        allocated++;

        unsigned int videoWidth = config.width;
        unsigned int videoHeight = config.height;
        unsigned int sourceWidth, sourceHeight, videoWidthRatio, videoHeightRatio;
        res = MFGetAttributeSize(sourceReaderOutput, MF_MT_FRAME_SIZE, &sourceWidth, &sourceHeight);
        if(videoWidth == 0) videoWidth = sourceWidth;
        if(videoHeight == 0) videoHeight = sourceHeight;
        computeRatio(videoWidth, videoHeight, videoWidthRatio, videoHeightRatio);

        res = sinkWriterInput->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        res = sinkWriterInput->SetGUID(MF_MT_SUBTYPE, config.encodingFormat);
        res = sinkWriterInput->SetUINT32(MF_MT_AVG_BITRATE, config.bitrate);
        res = sinkWriterInput->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        res = sinkWriterInput->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
        res = MFSetAttributeSize(sinkWriterInput, MF_MT_FRAME_SIZE, videoWidth, videoHeight);
        res = MFSetAttributeRatio(sinkWriterInput, MF_MT_FRAME_RATE, config.fps, 1); 

        res = sinkWriter->AddStream(sinkWriterInput, &streamIndex);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }

        res = sinkWriter->SetInputMediaType(streamIndex, sourceReaderOutput, nullptr);
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
    }

    void capture(unsigned int millisecond) {
        bool firstSampleCollected = false;
        long long absoluteTimestamp = 0, timestamp = 0, relativeTimestamp = 0;
        long long totalDuration = (long long) millisecond * 10000;
        long long sampleDuration = totalDuration / config.fps;  //total unit in 100ns divide fps
        IMFSample* sample = nullptr;
        DWORD streamFlags = 0, actualStreamIndex = 0;
        int tryagain = 0;
        res = sinkWriter->BeginWriting();
        do {
            res = sourceReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                &actualStreamIndex,
                &streamFlags,
                &timestamp,
                &sample
            );

            // cout << "flag: " << streamFlags << "\n";

            if(FAILED(res)) {
                cout << "Failed reading sample. BReak!\n";
                invokeException(__func__, __LINE__);
                break;
            } else if(streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                cout << "End of stream. BReak!\n";
                break;
            }

            if(sample) {
                tryagain = 0;
                if(!firstSampleCollected) {
                    firstSampleCollected = true;
                    absoluteTimestamp = timestamp;
                }
                relativeTimestamp = timestamp - absoluteTimestamp; 
                // cout << "relativeTimestamp: " << relativeTimestamp << "\n";
                sample->SetSampleTime(relativeTimestamp);
                sample->SetSampleDuration(sampleDuration);
                res = sinkWriter->WriteSample(streamIndex, sample);
                ComSafeRelease(&sample);
                if(FAILED(res)) {
                    cout << "Writing sample failed. BReak!\n";
                    break;
                }
            } else {
                tryagain++;
                if(tryagain >= 5) {
                    invokeException(__func__, __LINE__, "Maximum tryagain exceeded");
                } else continue;
            }
        } while(relativeTimestamp < millisecond * 10000);

        res = sinkWriter->Finalize();
        if(FAILED(res)) {
            invokeException(__func__, __LINE__);
        }
    }

    VideoConfig config;
    std::string filename;
    int durationMillisecond;

    public:
    void run() {
        res = CoInitialize(NULL);
        if(FAILED(res)) invokeException(__func__, __LINE__, "CoInitialize");
        allocated++;
        res = MFStartup(MF_VERSION);
        if(FAILED(res)) invokeException(__func__, __LINE__, "MFStartup");
        allocated++;

        createVideoDeviceSource();
        getSource();
        configOutput();
        capture(durationMillisecond);
    }

    neeko() = delete;

    neeko(const std::string& filename, const int& durationMillisecond, const VideoConfig& config) {
        allocated = 0;
        this->filename = filename;
        this->durationMillisecond = durationMillisecond;
        this->config = config;
    }

    //neeko can't be copy ?!
    neeko(const neeko& rhs) = delete;

    virtual ~neeko() {
        cout << "cleaning up!...\n";
        switch(allocated) {
            case 11:    ComSafeRelease(&sinkWriterInput);
            case 10:    ComSafeRelease(&sinkWriter);
            case 9:     ComSafeRelease(&writerAttributes);
            case 8:     ComSafeRelease(&mediaType);
            case 7:     ComSafeRelease(&nativeMediaType);
            case 6:     ComSafeRelease(&sourceReader);
            case 5:     ComSafeRelease(&mediaSource);
            case 4: {
                for(unsigned int i = 0; i < deviceCount; i++) {
                    ComSafeRelease(&ppDevices[i]);
                }
                CoTaskMemFree(ppDevices);
            }
            case 3: ComSafeRelease(&sourceAttributes);
            case 2: MFShutdown();
            case 1: CoUninitialize();
            default: break;
        }

        res = MFShutdown();
    }
};


int main() {
    VideoConfig config;
    config.bitrate = 200000;
    config.width = 0;
    config.height = 0;
    config.fps = 8;
    config.encodingFormat = MFVideoFormat_H264;
    try {
        neeko obj("test.mp4", 6000, config);
        obj.run();
    } catch(std::exception& e) {
        cout << endl << "Exception caught: " << e.what() << endl;
    }
}