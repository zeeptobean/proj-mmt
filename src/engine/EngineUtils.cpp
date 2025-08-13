#include "Engine.hpp"
using json = nlohmann::json;

bool preliminaryEngineMessageCheck(const Message& inputMessage, Message& outputMessage, json& jsonTemplate) {
    jsonTemplate = json::parse(std::string(inputMessage.getJsonData(), inputMessage.getJsonDataSize()));
    if(inputMessage.commandNumber !=  MessageEnableKeylog) {
        outputMessage.commandNumber = MessageEnableKeylog;
        outputMessage.returnCode = 0;
        jsonTemplate["errorString"] = "Wrong command sent?";
        outputMessage.setJsonData(jsonTemplate);
        return 0;
    }
    return 1;
}
