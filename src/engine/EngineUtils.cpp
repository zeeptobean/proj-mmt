#include "Engine.hpp"
using json = nlohmann::json;

bool preliminaryEngineMessageCheck(int MessageEnum, const Message& inputMessage, Message& outputMessage, json& jsonTemplate) {
    //preverse sender-specific json data
    try {
        //this would failed if an empty string
        jsonTemplate = json::parse(std::string(inputMessage.getJsonData(), inputMessage.getJsonDataSize()));
    } catch(...) {
        jsonTemplate = json();
    }
    if(inputMessage.commandNumber != MessageEnum) {
        outputMessage.commandNumber = MessageEnum;
        outputMessage.returnCode = 0;
        jsonTemplate["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(jsonTemplate);
        return 0;
    }
    jsonTemplate["errorString"] = "OK";
    return 1;
}
