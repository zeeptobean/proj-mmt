#include "Engine.hpp"
#include <windows.h>
#include <psapi.h>

bool GetProcessesNameAndPID(std::vector<std::pair<DWORD, std::string>>& outputVec) {
    std::vector<DWORD> pidVec(2048);
    DWORD bytesReturned = 0;

    if(EnumProcesses(pidVec.data(), pidVec.size() * sizeof(DWORD), &bytesReturned)) {
        if (bytesReturned < pidVec.size() * sizeof(DWORD)) {
            pidVec.resize(bytesReturned / sizeof(DWORD));
        } else {
            pidVec.resize(bytesReturned+128);
            if(EnumProcesses(pidVec.data(), pidVec.size() * sizeof(DWORD), &bytesReturned)) {
                if(bytesReturned < pidVec.size() * sizeof(DWORD)) pidVec.resize(bytesReturned / sizeof(DWORD));
            } else return false;
        }
    } else return false;

    outputVec.clear();

    for(int i=0; i < (int) pidVec.size(); i++) {
        wchar_t processName[261] = L"<unknown>";
        std::wstring processNameWideString = L"<unknown>";
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pidVec[i]);
        if(hProcess != nullptr) {
            DWORD dwSize = 260;
            if(QueryFullProcessImageNameW(hProcess, 0, processName, &dwSize)) {
                wchar_t *p = wcsrchr(processName, '\\');
                if(p) {
                    processNameWideString = std::wstring(p+1);
                } else {
                    processNameWideString = std::wstring(processName);
                }
            }
        }
        CloseHandle(hProcess);

        std::string processNameString;
        (void) WideStringToString(processNameWideString, processNameString);

        outputVec.push_back(std::make_pair(pidVec[i], processNameString));
    }
    return true;
}

int ListProcessHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;
    if(!preliminaryEngineMessageCheck(MessageListProcess, inputMessage, outputMessage, outputJson)) return 0;

    std::vector<std::pair<DWORD, std::string>> outputVec;
    if(!GetProcessesNameAndPID(outputVec)) {
        outputMessage.commandNumber = MessageListProcess;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Can't get processes";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    json outputVecJson = json::array();
    for(auto& pp:outputVec) {
        json entry;
        entry["pid"] = pp.first;
        entry["name"] = pp.second;
        outputVecJson.push_back(entry);
    }
    std::string outputVecJsonString = outputVecJson.dump();
    outputMessage.commandNumber = MessageListProcess;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputMessage.setJsonData(outputJson);
    outputMessage.setBinaryData(outputVecJsonString.c_str(), outputVecJsonString.size());
    return 1;
}

int StopProcessHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson, inputJson;
    if(!preliminaryEngineMessageCheck(MessageStopProcess, inputMessage, outputMessage, outputJson)) return 0;

    try {
        inputJson = json::parse(std::string(inputMessage.getJsonData(), inputMessage.getJsonDataSize()));
    } catch(...) {
        outputMessage.commandNumber = MessageStopProcess;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Json parse failed";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    int dwProcessId;
    if(inputJson.contains("pid")) {
        dwProcessId = inputJson.at("pid");
    } else {
        outputMessage.commandNumber = MessageStopProcess;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "No PID supplied";
        outputMessage.setJsonData(outputJson);
        return 0;
    }
    
    DWORD dwDesiredAccess = PROCESS_TERMINATE;
    BOOL  bInheritHandle = FALSE;
    HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, (DWORD) dwProcessId);

    if(TerminateProcess(hProcess, 0)){
        CloseHandle(hProcess);
        outputMessage.commandNumber = MessageStopProcess;
        outputMessage.returnCode = 1;
        outputJson["errorString"] = "OK";
        outputMessage.setJsonData(outputJson);
        return 1;
    } else {
        CloseHandle(hProcess);
        outputMessage.commandNumber = MessageStopProcess;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Kill failed";
        outputMessage.setJsonData(outputJson);
        return 0;
    }
}

int StartProcessHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;
    if(!preliminaryEngineMessageCheck(MessageStartProcess, inputMessage, outputMessage, outputJson)) return 0;

    std::string cmdlineStr = std::string(inputMessage.getBinaryData(), inputMessage.getBinaryDataSize());
    std::wstring cmdlineWideStr;
    (void) StringToWideString(cmdlineStr, cmdlineWideStr);

    wchar_t lpCommandLine[32768];
    ZeroMemory(lpCommandLine, 32768*2);
    wcscpy(lpCommandLine, cmdlineWideStr.data());

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if(CreateProcessW(NULL,   // No module name (use command line)
        lpCommandLine,      // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        DETACHED_PROCESS,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        )
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        outputMessage.commandNumber = MessageStartProcess;
        outputMessage.returnCode = 1;
        outputJson["errorString"] = "OK";
        outputMessage.setJsonData(outputJson);
        return 1;
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        outputMessage.commandNumber = MessageStartProcess;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Start failed";
        outputMessage.setJsonData(outputJson);
        return 1;
    }
}