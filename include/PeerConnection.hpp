#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
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

const int BufferSize = 4096;

class PeerConnection {
protected:
    std::atomic<int> socketfile{-1};
    std::atomic<bool> active{false};
    std::string peerIp;
    uint16_t peerPort;
    std::chrono::system_clock::time_point connectionTime;
    
    std::vector<uint8_t> receiveBuffer;
    int remainingDataSize{0};   
    std::array<uint8_t, 12> currentNonce;
    // std::mutex socketMutex;

    inline void closesocket(int socketHandle) {
        #ifdef WIN32
        ::closesocket(socketHandle);
        #else
        close(socketHandle);
        #endif
    }

public:
    PeerConnection() = default;

    virtual ~PeerConnection() {
        disconnect();
    }

    CryptHandler crypt;

    bool isActive() const;
    std::string getPeerIp() const;
    uint16_t getPeerPort() const;
    std::string getPeerIpPort() const;

    size_t getConnectionDuration() const;

    virtual void disconnect();

    virtual bool sendData(const Message& msg);

    virtual bool sendPublicKey();

protected:
    enum class ReceiveStatus {
        Success,
        NeedMoreData,
        KeyExchange,
        PeerDisconnected,
        Error
    };

    ReceiveStatus receiveData();

    bool processCompleteMessage(Message& msg);
};