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

    wchar_t *wstr_ptr = PathFindFileNameW(wfilename.c_str());
    std::wstring wfilenameNoPath = std::wstring(wstr_ptr);
    std::string filenameNoPath;
    (void) WideStringToString(wfilenameNoPath, filenameNoPath);


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
        outputJson["fileName"] = filename;
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
    outputJson["fileName"] = filenameNoPath;
    outputMessage.setJsonData(outputJson);
    outputMessage.setBinaryData(buffer.data(), (int) buffer.size());
    return 1;
}

/*
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
*/

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
    //mundanely checking wildcard
    while(filename.size() > 0 && filename.back() == '*') {
        filename.pop_back();
    }
    if(filename.back() == '/') filename.push_back('*');
    else filename += "/*";
    std::wstring wfilename;
    (void) StringToWideString(filename, wfilename);

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW(wfilename.c_str(), &findFileData);
    if(hFind == INVALID_HANDLE_VALUE) {
        outputMessage.commandNumber = MessageListFile;
        outputMessage.returnCode = 0;
        outputJson["fileName"] = filename;
        outputJson["errorString"] = "Fall to execute: no such folder?";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    json dirJson;
    dirJson["path"] = filename;
    dirJson["list"] = json::array();
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
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM || findFileData.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) {
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

        dirJson["list"].push_back(j);
    } while (FindNextFileW(hFind, &findFileData) != 0);

    //The content could be large so put in raw data as json data is limited to 64kb
    outputMessage.commandNumber = MessageListFile;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputMessage.setJsonData(outputJson);
    std::string jdump = dirJson.dump();
    outputMessage.setBinaryData(jdump.data(), jdump.size());
    return 1;
}
