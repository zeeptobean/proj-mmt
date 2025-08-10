#include "PeerConnection.hpp"

//Public

bool PeerConnection::isActive() const {
    return active.load();
}

std::string PeerConnection::getPeerIp() const {
    return peerIp;
}

uint16_t PeerConnection::getPeerPort() const {
    return peerPort;
}

std::string PeerConnection::getPeerIpPort() const {
    return peerIp + ':' + std::to_string(peerPort);
}

size_t PeerConnection::getConnectionDuration() const {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - connectionTime);
    return seconds.count();
}

void PeerConnection::disconnect() {
    if (!active.load()) return;
    
    active.store(false);
    
    int currentSocket = socketfile.load();
    if (currentSocket != -1) {
        #ifdef WIN32
        shutdown(currentSocket, SD_BOTH);
        closesocket(currentSocket);
        #else
        shutdown(currentSocket, SHUT_RDWR);
        close(currentSocket);
        #endif
    }
    
    receiveBuffer.clear();
    remainingPayloadSize = 0;
    writtenPayloadSize = 0;
    socketfile.store(-1);
}

bool PeerConnection::sendData(const Message& msg) {
    if (!active.load()) return false;

    char* data;
    int dataSize;
    if (!prepareMessage(msg, data, dataSize, nullptr)) {
        return false;
    }

    if(!crypt.checkOtherPublicKeyStatus()) {
        return false;
    }

    std::vector<uint8_t> dataVec(data, data + dataSize);
    delete[] data;

    std::vector<uint8_t> cipherText;
    std::array<uint8_t, 12> nonce;
    if (!crypt.encrypt(dataVec, cipherText, nonce)) {
        return false;
    }

    std::vector<uint8_t> fullEncryptedSegment(cipherText.size() + 24, 0);
    int payloadSize = (int) cipherText.size();
    
    memcpy(fullEncryptedSegment.data(), "ZZTE", 4);
    memcpy(fullEncryptedSegment.data()+4, &payloadSize, 4);
    memcpy(fullEncryptedSegment.data()+8, nonce.data(), 12);
    memcpy(fullEncryptedSegment.data()+24, cipherText.data(), cipherText.size());

    size_t totalToSend = fullEncryptedSegment.size();
    size_t totalSent = 0;
    
    int currentSocket = socketfile.load();
    if (currentSocket == -1) return false;
    
    while (totalSent < totalToSend && active.load()) {
        int sent = send(currentSocket, 
                       reinterpret_cast<const char*>(fullEncryptedSegment.data()) + totalSent, 
                       static_cast<int>(totalToSend - totalSent), 0);
        if (sent == -1) {
            #ifdef WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                return false;
            }
            #else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return false;
            }
            #endif
            // For blocking sockets, this shouldn't happen, but handle gracefully
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (sent == 0) {
            return false; // Connection closed
        }
        totalSent += sent;
    }
    return totalSent == totalToSend;
}

bool PeerConnection::sendPublicKey() {
    if (!active.load()) return false;

    std::array<uint8_t, 40> keyPacket;
    memcpy(keyPacket.data(), "ZZTE", 4);
    memset(keyPacket.data()+4, 0xFF, 4); // -1 indicates key exchange
    std::array<uint8_t, 32> pubKey = crypt.getPublicKey();
    memcpy(keyPacket.data()+8, pubKey.data(), 32);

    int currentSocket = socketfile.load();
    if (currentSocket == -1) return false;
    
    int sent = send(currentSocket, 
                   reinterpret_cast<const char*>(keyPacket.data()), 
                   40, 0);
    return sent == 40;
}

// Protected

PeerConnection::ReceiveStatus PeerConnection::receiveData() {
    std::array<uint8_t, BufferSize> tempBuffer;
    int currentSocket = socketfile.load();
    if (currentSocket == -1) return ReceiveStatus::Error;
    
    int bytesReceived = recv(currentSocket, 
                           reinterpret_cast<char*>(tempBuffer.data()), 
                           BufferSize, 0);
    
    if (bytesReceived < 0) {
        #ifdef WIN32
        int winsockError = WSAGetLastError();
        if (winsockError == WSAEWOULDBLOCK) {
            return ReceiveStatus::NeedMoreData;
        }
        #else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ReceiveStatus::NeedMoreData;
        }
        #endif
        active.store(false);
        return ReceiveStatus::Error;
    } else if (bytesReceived == 0) {
        active.store(false);
        return ReceiveStatus::PeerDisconnected;
    }

    const uint8_t* pData = tempBuffer.data();
    int processedBytes = 0;

    if (remainingPayloadSize == 0) {
        const int HEADER_SIZE = 24; // 4 magic + 4 size + 4 reserved + 12 nonce
        const int KEY_EXCHANGE_SIZE = 40; // 4 magic + 4 size(-1) + 32 key

        if (bytesReceived < HEADER_SIZE) {
            active.store(false);
            return ReceiveStatus::Error;
        }

        if (memcmp(pData, "ZZTE", 4) != 0) {
            active.store(false);
            return ReceiveStatus::Error;
        }

        int payloadSize;
        memcpy(&payloadSize, pData + 4, 4);

        if (payloadSize == -1) {
            if (bytesReceived < KEY_EXCHANGE_SIZE) {
                active.store(false);
                return ReceiveStatus::Error;
            }
            std::array<uint8_t, 32> pubKey;
            memcpy(pubKey.data(), pData + 8, 32);
            crypt.setOtherPublicKey(pubKey);
            return ReceiveStatus::KeyExchange;
        }
        
        if (payloadSize <= 0) {
            active.store(false);
            return ReceiveStatus::Error;
        }

        memcpy(currentNonce.data(), pData + 8, 12);

        // int reserved;
        // memcpy(&reserved, pData + 20, 4);

        receiveBuffer.resize(payloadSize, 0);
        remainingPayloadSize = payloadSize;
        writtenPayloadSize = 0;

        size_t payloadInThisChunk = bytesReceived - HEADER_SIZE;
        if (payloadInThisChunk > 0) {
            memcpy(receiveBuffer.data(), pData + HEADER_SIZE, payloadInThisChunk);
            writtenPayloadSize += payloadInThisChunk;
            remainingPayloadSize -= payloadInThisChunk;
        }
        
    } else {
        int bytesToCopy = bytesReceived;
        if (bytesToCopy > remainingPayloadSize) {
            bytesToCopy = remainingPayloadSize;
        }

        memcpy(receiveBuffer.data() + writtenPayloadSize, pData, bytesToCopy);
        
        writtenPayloadSize += bytesToCopy;
        remainingPayloadSize -= bytesToCopy;
    }

    if (remainingPayloadSize <= 0) {
        remainingPayloadSize = 0;
        writtenPayloadSize = 0;
        return ReceiveStatus::Success;
    } else {
        return ReceiveStatus::NeedMoreData;
    }
}

bool PeerConnection::processCompleteMessage(Message& msg) {
    if (!crypt.checkOtherPublicKeyStatus()) {
        return false;
    }

    std::vector<uint8_t> plainText;
    if (!crypt.decrypt(receiveBuffer, currentNonce, plainText)) {
        receiveBuffer.clear();
        secureZeroMemory(currentNonce.data(), 12);
        return false;
    }

    bool success = assembleMessage(reinterpret_cast<char*>(plainText.data()), 
                                 plainText.size(), msg);

    receiveBuffer.clear();
    secureZeroMemory(currentNonce.data(), 12);
    return success;
}