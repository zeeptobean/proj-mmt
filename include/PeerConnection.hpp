#pragma once

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <atomic>
#include <vector>
#include <array>
#include <string>
#include <chrono>
#include <mutex>
#include "Message.hpp"
#include "CryptHandler.hpp"
#include "InternalUtilities.hpp"

const int BufferSize = 2048;

class PeerConnection {
protected:
    std::atomic<int> socketfile{-1};
    std::atomic<bool> active{false};
    std::string peerIp;
    uint16_t peerPort;
    std::chrono::system_clock::time_point connectionTime;
    CryptHandler crypt;
    
    std::vector<uint8_t> receiveBuffer;
    int remainingDataSize{0};
    std::array<uint8_t, 12> currentNonce;
    std::mutex socketMutex;

    void closesocket(int socketHandle) {
        #ifdef WIN32
        ::closesocket(socketHandle)
        #else
        close(socketHandle);
        #endif
    }

public:
    virtual ~PeerConnection() {
        disconnect();
    }

    bool isActive() const { return active.load(); }
    std::string getPeerIp() const { return peerIp; }
    uint16_t getPeerPort() const { return peerPort; }
    std::string getPeerIpPort() const { return peerIp + ':' + std::to_string(peerPort); }

    virtual void disconnect() {
        if (!active.load()) return;
        active.store(false);

        std::lock_guard<std::mutex> lock(socketMutex);
        int currentSocket = socketfile.load();
        if (currentSocket != -1) {
            shutdown(currentSocket, 2);
            closesocket(currentSocket);
        }
        socketfile.store(-1);
    }

    virtual bool sendData(const Message& msg) {
        if (!active.load()) return false;

        char* data;
        int dataSize;
        if (!prepareMessage(msg, data, dataSize, nullptr)) {
            return false;
        }

        std::vector<uint8_t> dataVec(data, data + dataSize);
        delete[] data;

        std::vector<uint8_t> cipherText;
        std::array<uint8_t, 12> nonce;
        crypt.encrypt(dataVec, cipherText, nonce);

        std::vector<uint8_t> fullEncryptedSegment(cipherText.size() + 24, 0);
        int fullEncryptedSegmentSize = static_cast<int>(fullEncryptedSegment.size());
        
        memcpy(fullEncryptedSegment.data(), "ZZTE", 4);
        memcpy(fullEncryptedSegment.data()+4, &fullEncryptedSegmentSize, 4);
        memcpy(fullEncryptedSegment.data()+8, nonce.data(), 12);
        memcpy(fullEncryptedSegment.data()+24, cipherText.data(), cipherText.size());

        int remaining = fullEncryptedSegment.size();
        int sent = 0;
        
        std::lock_guard<std::mutex> lock(socketMutex);
        while (remaining > 0 && active.load()) {
            sent = send(socketfile.load(), 
                       reinterpret_cast<char*>(fullEncryptedSegment.data()) + sent, 
                       remaining, 0);
            if (sent == -1) {
                return false;
            }
            remaining -= sent;
        }
        return true;
    }

    virtual bool sendPublicKey() {
        if (!active.load()) return false;

        std::array<uint8_t, 40> keyPacket;
        memcpy(keyPacket.data(), "ZZTE", 4);
        memset(keyPacket.data()+4, 0xFF, 4); // -1 indicates key exchange
        std::array<uint8_t, 32> pubKey = crypt.getPublicKey();
        memcpy(keyPacket.data()+8, pubKey.data(), 32);

        std::lock_guard<std::mutex> lock(socketMutex);
        int sent = send(socketfile.load(), 
                       reinterpret_cast<char*>(keyPacket.data()), 
                       40, 0);
        return sent == 40;
    }

protected:
    enum class ReceiveStatus {
        Success,
        NeedMoreData,
        KeyExchange,
        PeerDisconnected,
        Error
    };

    ReceiveStatus receiveData() {
        std::array<uint8_t, BufferSize> tempBuffer;
        int bytesReceived = recv(socketfile.load(), 
                               reinterpret_cast<char*>(tempBuffer.data()), 
                               BufferSize, 0);
        
        if (bytesReceived < 0) {
            return ReceiveStatus::Error;
        } else if (bytesReceived == 0) {
            return ReceiveStatus::PeerDisconnected;
        }

        if (remainingDataSize == 0) {
            // New message
            if (bytesReceived < 8) return ReceiveStatus::Error;
            
            if (memcmp(tempBuffer.data(), "ZZTE", 4) != 0) {
                return ReceiveStatus::Error;
            }

            int totalSize;
            memcpy(&totalSize, tempBuffer.data()+4, 4);
            
            if (totalSize == -1) {
                // Key exchange
                if (bytesReceived < 40) return ReceiveStatus::NeedMoreData;
                
                std::array<uint8_t, 32> pubKey;
                memcpy(pubKey.data(), tempBuffer.data()+8, 32);
                crypt.setOtherPublicKey(pubKey);
                return ReceiveStatus::KeyExchange;
            } else {
                // Regular message
                if (bytesReceived < 24) return ReceiveStatus::NeedMoreData;
                
                remainingDataSize = totalSize;
                receiveBuffer.resize(bytesReceived - 24);
                memcpy(currentNonce.data(), tempBuffer.data()+8, 12);
                memcpy(receiveBuffer.data(), tempBuffer.data()+24, bytesReceived - 24);
                remainingDataSize -= (bytesReceived - 24);
                
                if (remainingDataSize <= 0) {
                    remainingDataSize = 0;
                    return ReceiveStatus::Success;
                }
                return ReceiveStatus::NeedMoreData;
            }
        } else {
            // Continuation of existing message
            size_t oldSize = receiveBuffer.size();
            receiveBuffer.resize(oldSize + bytesReceived);
            memcpy(receiveBuffer.data() + oldSize, tempBuffer.data(), bytesReceived);
            remainingDataSize -= bytesReceived;
            
            if (remainingDataSize <= 0) {
                remainingDataSize = 0;
                return ReceiveStatus::Success;
            }
            return ReceiveStatus::NeedMoreData;
        }
    }

    bool processCompleteMessage(Message& msg) {
        if (!crypt.checkOtherPublicKeyStatus()) {
            return false;
        }

        std::vector<uint8_t> plainText;
        if (!crypt.decrypt(receiveBuffer, currentNonce, plainText)) {
            return false;
        }

        if (!assembleMessage(reinterpret_cast<char*>(plainText.data()), 
                           plainText.size(), msg)) {
            return false;
        }

        receiveBuffer.clear();
        secureZeroMemory(currentNonce.data(), 12);
        return true;
    }
};