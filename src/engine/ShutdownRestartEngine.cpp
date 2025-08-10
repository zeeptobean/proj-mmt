#include "Engine.hpp"

int ShutdownEngine(const Message& inputMessage, Message& outputMessage) {
    json outputJson;

    if(inputMessage.commandNumber !=  MessageShutdownMachine) {
        outputMessage.commandNumber = MessageShutdownMachine;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    system("shutdown -s -f -t 0");
    //dead code
    outputMessage.commandNumber = MessageShutdownMachine;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputMessage.setJsonData(outputJson);
    return 1;
}


int RestartEngine(const Message& inputMessage, Message& outputMessage) {
    json outputJson;

    if(inputMessage.commandNumber !=  MessageRestartMachine) {
        outputMessage.commandNumber = MessageRestartMachine;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(outputJson);
        return 0;
    }

    system("shutdown -r -f -t 0");
    //dead code
    outputMessage.commandNumber = MessageRestartMachine;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputMessage.setJsonData(outputJson);
    return 1;
}
