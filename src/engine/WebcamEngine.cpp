//link with -lmf -lmfplat -lmfreadwrite -lmfuuid -lshlwapi -lole32 -loleaut32 -lrpcrt4

#include "Engine.hpp"

class WebcamEngine::neeko {
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
        std::string str = "Exception in line " + std::to_string(line) + ", function " + functionName + ", context: " + context + ". ";
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

        unsigned int sourceWidth, sourceHeight;
        unsigned int sourceFps, sourceFps2;
        res = MFGetAttributeSize(sourceReaderOutput, MF_MT_FRAME_RATE, &sourceFps, &sourceFps2);
        sourceFps = (unsigned int) ((double) sourceFps / sourceFps2);
        res = MFGetAttributeSize(sourceReaderOutput, MF_MT_FRAME_SIZE, &sourceWidth, &sourceHeight);
        if(config.width <= 0) config.width = sourceWidth;
        if(config.height <= 0) config.height = sourceHeight;
        if(config.fps <= 0) {
            if(sourceFps != 0) config.fps = sourceFps;
            else config.fps = 30;
        }
        if(config.bitrate <= 0) {
            // config.bitrate = 2000000;
            config.bitrate = (int) ( (double) config.width * config.height * config.fps * 0.085);
        }

        res = sinkWriterInput->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        res = sinkWriterInput->SetGUID(MF_MT_SUBTYPE, config.encodingFormat);
        res = sinkWriterInput->SetUINT32(MF_MT_AVG_BITRATE, config.bitrate);
        res = sinkWriterInput->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        res = sinkWriterInput->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
        res = MFSetAttributeSize(sinkWriterInput, MF_MT_FRAME_SIZE, config.width, config.height);
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
                invokeException(__func__, __LINE__, "failed reading sample");
                break;
            } else if(streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                invokeException(__func__, __LINE__, "end of stream");
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
                    invokeException(__func__, __LINE__, "failed writing to file");
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
            invokeException(__func__, __LINE__, "failed finalize");
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
        switch(allocated) {
            case 11:    ComSafeRelease(&sinkWriterInput);
            [[fallthrough]];
            case 10:    ComSafeRelease(&sinkWriter);
            [[fallthrough]];
            case 9:     ComSafeRelease(&writerAttributes);
            [[fallthrough]];
            case 8:     ComSafeRelease(&mediaType);
            [[fallthrough]];
            case 7:     ComSafeRelease(&nativeMediaType);
            [[fallthrough]];
            case 6:     ComSafeRelease(&sourceReader);
            [[fallthrough]];
            case 5:     ComSafeRelease(&mediaSource);
            [[fallthrough]];
            case 4: {
                for(unsigned int i = 0; i < deviceCount; i++) {
                    ComSafeRelease(&ppDevices[i]);
                }
                CoTaskMemFree(ppDevices);
                [[fallthrough]];
            }
            case 3: ComSafeRelease(&sourceAttributes);
            [[fallthrough]];
            case 2: MFShutdown();
            [[fallthrough]];
            case 1: CoUninitialize();
            [[fallthrough]];
            default: break;
        }

        res = MFShutdown();
    }
};

bool WebcamEngine::run(int millisecond, int fps, std::string *errorString = nullptr) {
    VideoConfig config;
    config.fps = fps;
    config.height = 0;
    config.width = 0;
    config.bitrate = 0;
    
    try {
        neeko obj("rekord.mp4", millisecond, config);
        obj.run();
    } catch(std::exception& e) {
        if(errorString) *errorString = e.what();
        return 0;
    }
    return 1;
}

int InvokeWebcamHandler(const Message& inputMessage, Message& outputMessage) {
    json inputJsonData = json::parse(std::string(inputMessage.getJsonData(), inputMessage.getJsonDataSize()));
    json outputJsonData;

    if(inputMessage.commandNumber !=  MessageInvokeWebcam) {
        outputMessage.commandNumber = MessageInvokeWebcam;
        outputMessage.returnCode = 0;
        outputJsonData["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(outputJsonData);
        return 0;
    }

    WebcamEngine e;
    int millisecond, fps = 0;
    if(inputJsonData.contains("millisecond")) {
        millisecond = inputJsonData.at("millisecond");
    } else {
        outputMessage.commandNumber = MessageInvokeWebcam;
        outputMessage.returnCode = 0;
        outputJsonData["errorString"] = "No millisecond supplied";
        outputMessage.setJsonData(outputJsonData);
        return 0;
    }
    if(inputJsonData.contains("fps")) {
        fps = inputJsonData.at("fps");
    }

    std::string errorString;
    if(e.run(millisecond, fps, &errorString)) {     //block here
        FILE *fileptr = fopen("rekord.mp4", "rb");
        if(fileptr == nullptr) {
            outputMessage.commandNumber = MessageInvokeWebcam;
            outputMessage.returnCode = 0;
            outputJsonData["errorString"] = "Can't send file back";
            outputMessage.setJsonData(outputJsonData);
            return 0;
        } else {
            fseek(fileptr, 0, SEEK_END);
            int filesize = ftell(fileptr);
            fseek(fileptr, 0, SEEK_SET);
            std::vector<char> datavec(filesize, 'A');
            fread(datavec.data(), 1, filesize, fileptr);
            fclose(fileptr);
            DeleteFileW(L"rekord.mp4");

            outputMessage.commandNumber = MessageInvokeWebcam;
            outputMessage.returnCode = 1;
            outputJsonData["errorString"] = "OK";
            outputMessage.setJsonData(outputJsonData);
            outputMessage.setBinaryData(datavec.data(), filesize);
            return 1;
        }
    } else {
        outputMessage.commandNumber = MessageInvokeWebcam;
        outputMessage.returnCode = 0;
        outputJsonData["errorString"] = errorString;
        outputMessage.setJsonData(outputJsonData);
        return 0;
    }
}

