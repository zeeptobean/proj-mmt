#include "Engine.hpp"

int GetFileHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;

    if(inputMessage.commandNumber !=  MessageGetFile) {
        outputMessage.commandNumber = MessageGetFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    std::string filename = JsonDataHelper::GetFileName(inputMessage);
    std::wstring wfilename;
    (void) StringToWideString(filename, wfilename);

    HANDLE hFile = CreateFileW(
        wfilename.c_str(),           // file to open
        GENERIC_READ,       // open for reading
        FILE_SHARE_READ,    // share for reading
        NULL,               // default security
        OPEN_EXISTING,      // open an existing file
        FILE_ATTRIBUTE_NORMAL, // normal file
        NULL                // no template file
    );

    if(hFile == INVALID_HANDLE_VALUE) {
        outputMessage.commandNumber = MessageGetFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Fail to open file";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    
    DWORD byteRead = 0;
    std::vector<char> buffer(fileSize);
    BOOL bResult = ReadFile(hFile, buffer.data(), fileSize, &byteRead, NULL);
    
    if(bResult == false || byteRead != fileSize) {
        CloseHandle(hFile);
        outputMessage.commandNumber = MessageGetFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Fail to read file";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    CloseHandle(hFile);
    outputMessage.commandNumber = MessageGetFile;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputJson["fileName"] = filename;
    outputMessage.setJsonData(outputJson);
    outputMessage.setBinaryData(buffer.data(), (int) buffer.size());
    return 1;
}

int DeleteFileHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;

    if(inputMessage.commandNumber !=  MessageDeleteFile) {
        outputMessage.commandNumber = MessageDeleteFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    std::string filename = JsonDataHelper::GetFileName(inputMessage);
    std::wstring wfilename;
    (void) StringToWideString(filename, wfilename);

    DWORD fileAttributes = GetFileAttributesW(wfilename.c_str());
    BOOL bres;
    if(fileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        outputMessage.commandNumber = MessageDeleteFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Target is a directory";
        outputMessage.setJsonData(outputJson);
        return 0;
    } else {
        bres = DeleteFileW(wfilename.c_str());
    }

    if(bres) {
        outputMessage.commandNumber = MessageDeleteFile;
        outputMessage.returnCode = 1;
        outputJson["errorString"] = "OK";
        outputMessage.setJsonData(outputJson);
        return 1;
    } else {
        outputMessage.commandNumber = MessageDeleteFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Delete file failed";
        outputMessage.setJsonData(outputJson);
        return 0;
    }
}

int ListFilehandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;

    if(inputMessage.commandNumber != MessageListFile) {
        outputMessage.commandNumber = MessageListFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    std::string filename = JsonDataHelper::GetFileName(inputMessage);   //folder name
    std::wstring wfilename;
    (void) StringToWideString(filename, wfilename);

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW(wfilename.c_str(), &findFileData);
    if(hFind == INVALID_HANDLE_VALUE) {
        outputMessage.commandNumber = MessageListFile;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Fall to execute";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    outputJson["files"] = json::array();
    do {
        if (wcscmp(findFileData.cFileName, L".") == 0 || wcscmp(findFileData.cFileName, L"..") == 0) {
            continue;
        }

        json j;
        std::string iteFileName;
        std::vector<std::string> iteFileAttributeList;
        (void) WideStringToString(std::wstring(findFileData.cFileName), iteFileName);

        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
            iteFileAttributeList.push_back("readonly");
        }
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            iteFileAttributeList.push_back("hidden");
        }
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM | findFileData.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) {
            iteFileAttributeList.push_back("system");
        }
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            iteFileAttributeList.push_back("directory");
        }
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) {
            iteFileAttributeList.push_back("temp");
        }
        ULARGE_INTEGER fileSize;
        fileSize.LowPart = findFileData.nFileSizeLow;
        fileSize.HighPart = findFileData.nFileSizeHigh;

        j["fileName"] = iteFileName;
        j["fileSize"] = fileSize.QuadPart;
        j["fileAttributes"] = iteFileAttributeList;

        outputJson["files"].push_back(j);
    } while (FindNextFileW(hFind, &findFileData) != 0);

    //The content could be large so put in raw data as json data is limited to 64kb
    std::string jdump = outputJson.dump();
    outputMessage.commandNumber = MessageListFile;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputMessage.setBinaryData(jdump.data(), jdump.size());
    return 1;
}
