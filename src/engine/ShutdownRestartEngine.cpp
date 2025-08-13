#include "Engine.hpp"

int ShutdownEngine(const Message& inputMessage, Message& outputMessage) {
    json outputJson;
    if(!preliminaryEngineMessageCheck(MessageShutdownMachine, inputMessage, outputMessage, outputJson)) return 0;

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
    if(!preliminaryEngineMessageCheck(MessageRestartMachine, inputMessage, outputMessage, outputJson)) return 0;

    system("shutdown -r -f -t 0");
    //dead code
    outputMessage.commandNumber = MessageRestartMachine;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputMessage.setJsonData(outputJson);
    return 1;
}
