#define WIN32_LEAN_AND_MEAN
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

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include <imconfig.h>

#include <bits/stdc++.h>
#include "ImGuiStdString.hpp"
#include "ThreadWrapper.hpp"
#include "Message.hpp"
#include "ImGuiScrollableText.hpp"
#include "CryptHandler.hpp"

#define DEFAULT_BUFLEN 2048
#define DEFAULT_PORT 62300
const int BufferSize = 2048;

class ClientSession {
    int socket;
    sockaddr_in clientAddress;
    std::string clientIp;
    uint16_t clientPort;
    std::chrono::system_clock::time_point connectionTime;

    void init();

    void clientListener();

    public:
    char outputBuffer[BufferSize+3];
    std::string inputBuffer;
    std::atomic<bool> active;

    ClientSession(const int& _socket, const sockaddr_in& _clientAddress);

    ClientSession(const ClientSession&) = delete;
    // ClientSession(ClientSession&& rhs) {}

    ~ClientSession();

    inline std::string getClientIp() const {
        return clientIp;
    }

    inline uint16_t getClientPort() const {
        return clientPort;
    }

    inline std::string getClientIpPort() const {
        return clientIp + ':' + std::to_string(getClientPort());
    }

    inline int getClientSocket() const {
        return socket;
    }

    inline std::string getConnectionDuration() const {
        auto now = std::chrono::system_clock::now();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - connectionTime);
        return std::to_string(seconds.count()) + "s";
    }

    inline std::string makeWidgetName(const std::string& name, const std::string& postfix = "") const {
        std::string ret;
        ret = name + "##" + this->getClientIpPort() + '_' + postfix;
        return ret;
    }
};

class ClientVector {
    private:
    std::mutex clientVecMutex;
    std::vector<ClientSession*> clientVec;

    public:
    ClientVector() = default;
    inline ~ClientVector() {
        for(auto ite = clientVec.begin(); ite != clientVec.end(); ite++) {
            delete *ite;
        }
    }

    void removeClosed();

    void pushBack(ClientSession* c);

    int size();

    ClientSession*& operator[](int index);
};

class ServerConnectionManager {
private:
    std::atomic<int> socketfile{-1};
    std::atomic<bool> running{false};

    inline bool isInvalidAddress(sockaddr_in* addr) {
        // Filter 0.0.0.0:0
        if (addr->sin_addr.s_addr == 0 && addr->sin_port == 0) return true;
        
        // Filter broadcast addresses
        if (addr->sin_addr.s_addr == INADDR_BROADCAST) return true;

        return false;
    }

    void acceptClientLoop();

public:
    void init();
    ServerConnectionManager() {
        // init();
    }

    ~ServerConnectionManager() {
        disconnect();
    }

    void disconnect();

    //should be called within a thread
    bool sendData(ClientSession *c, const Message& msg);

    //Each entity handle key sending on their own
    //should be called within a thread
    bool sendPublicKey(ClientSession *c);
};