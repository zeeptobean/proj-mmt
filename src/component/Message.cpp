#include "Message.hpp"

Message::Message(const Message& rhs) {
    *this = rhs;
}

Message& Message::operator=(const Message& rhs) {
    if(&rhs != this) {
        commandNumber = rhs.commandNumber;
        returnCode = rhs.returnCode;
        reserved = rhs.reserved;

        jsonDataSize = rhs.jsonDataSize;
        binaryDataSize = rhs.binaryDataSize;
        delete[] jsonData;
        jsonData = new char[jsonDataSize+1];
        memset(jsonData, 0, jsonDataSize);
        memcpy(jsonData, rhs.jsonData, jsonDataSize);
        delete[] binaryData;
        binaryData = new char[binaryDataSize+1];
        memset(binaryData, 0, binaryDataSize);
        memcpy(binaryData, rhs.binaryData, binaryDataSize);

        setSegmentSize();
        return *this;
    }
}

Message::~Message() {
    delete[] binaryData;
    delete[] jsonData;
}

void Message::setJsonData(const json& j) {
    delete[] jsonData;
    std::string jdump = j.dump();
    jsonDataSize = jdump.size(); 
    jsonData = new char[jsonDataSize+1];
    memset(jsonData, 0, jsonDataSize);
    memcpy(jsonData, jdump.data(), jsonDataSize);
    setSegmentSize();
}

char* Message::getJsonData() const {
    return jsonData;
}

int Message::getJsonDataSize() const {
    return jsonDataSize;
}

void Message::setBinaryData(const char *tbuf, int tsize) {
    delete[] binaryData;
    binaryData = new char[tsize+1];
    memset(binaryData, 0, tsize+1);
    memcpy(binaryData, tbuf, tsize);
    binaryDataSize = tsize;
    setSegmentSize();
}

char* Message::getBinaryData() const {
    return binaryData;
}

int Message::getBinaryDataSize() const {
    return binaryDataSize;
}

int Message::getSegmentSize() const {
    return segmentSize;
}

int Message::setSegmentSize() {
    return segmentSize = 16 + jsonDataSize + binaryDataSize;
}

int assembleMessage(const char *bindata, int size, Message& msg, std::string *errorString) {
    if (size < 16) {
        if(errorString) *errorString = "Data too smol";
        return 0;
    }

    if (memcmp(bindata, "ZZTT", 4) != 0) {
        if(errorString) *errorString = "Invalid magic number";
        return 0;
    }

    //read segment size
    int segmentSize;
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
        return 1;
    }
    
    return 1;
}

int prepareMessage(const Message& msg, char*& data, int& dataSize, std::string *errorString) {
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

/*
int proceedEncryptedMessage(const char *bindata, int size, std::vector<uint8_t>& fullData, int& lastDataRemainingSize, std::array<uint8_t, 12>& nonce, std::string *errorString) {
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
            memcpy(noncePtr, bindata+8, 12);
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
        memcpy(fullDataPtr+oldFullDataSize, bindata, size);
        lastDataRemainingSize -= size;
        if(lastDataRemainingSize <= 0) lastDataRemainingSize = 0;
        return 1;
    }
}
*/