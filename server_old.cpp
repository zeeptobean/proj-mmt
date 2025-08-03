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
#include "ShutdownController.hpp"

using namespace std;

#define DEFAULT_BUFLEN 2048
#define DEFAULT_PORT 62300
const int BufferSize = 2048;

std::map<std::string, uint32_t> clientNamingPool;

class ClientSession {
    int socket;
    sockaddr_in clientAddress;
    char *oldbuf = nullptr;
    std::string clientIp;
    uint16_t clientPort;
    std::chrono::system_clock::time_point connectionTime;

    void init() {
        active = 1;
        char *tmp = inet_ntoa(clientAddress.sin_addr);
        clientIp = std::string(tmp);
        clientPort = ntohs(clientAddress.sin_port);
        connectionTime = std::chrono::system_clock::now();
    }

    public:
    char buf[BufferSize+3];
    std::atomic<bool> active;

    ClientSession(const int& _socket, const sockaddr_in& _clientAddress) : socket(_socket), clientAddress(_clientAddress) {
        ShutdownController::getInstance().registerResource();
        init();
    }

    ClientSession(const ClientSession&) = delete;
    // ClientSession(ClientSession&& rhs) {}

    ~ClientSession() {
        if(active) {
            shutdown(socket, SD_BOTH);
            closesocket(socket);
            active = false;
        }
        ShutdownController::getInstance().unregisterResource();
    }

    std::string getClientIp() const {
        return clientIp;
    }

    uint16_t getClientPort() const {
        return clientPort;
    }

    std::string getClientIpPort() const {
        return clientIp + ':' + to_string(getClientPort());
    }

    int getClientSocket() const {
        return socket;
    }

    /*
    bool isBufferUpdated() {
        if(oldbuf == nullptr) {
            oldbuf = new char[BufferSize+3];
            memset(oldbuf, 0, BufferSize+3);
            strncpy(oldbuf, buf, BufferSize);
            return true;
        } else {
            if(strcmp(buf, oldbuf) != 0) {
                memset(oldbuf, 0, BufferSize+3);
                strncpy(oldbuf, buf, BufferSize);
                return true;
            } else {
                return false;
            }
        }
    }
    */

    std::string getConnectionDuration() const {
        auto now = std::chrono::system_clock::now();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - connectionTime);
        return std::to_string(seconds.count()) + "s";
    }

    std::string makeWidgetName(const std::string& name, const std::string& postfix = "") const {
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
    ~ClientVector() {
        for(auto ite = clientVec.begin(); ite != clientVec.end(); ite++) {
            delete *ite;
        }
    }
    void removeClosed() {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        auto removePtr = std::remove_if(clientVec.begin(), clientVec.end(), [](ClientSession*& c) {
            return !c->active.load();
        });

        for(auto ite = removePtr; ite != clientVec.end(); ite++) {
            delete *ite;
        }

        clientVec.erase(removePtr, clientVec.end());
    }

    void pushBack(ClientSession* c) {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        clientVec.emplace_back(c);
    }

    int size() {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        return (int) clientVec.size();
    }

    ClientSession*& operator[](int index) {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        return clientVec.at(index);
    }
} clientVector;


void clientListener(ClientSession *ptr, ThreadWrapper* wrapper) {
    ShutdownController::getInstance().registerResource();

    // Set socket to non-blocking, how to do this on linux?
    u_long nonblocking = 1;
    ioctlsocket(ptr->getClientSocket(), FIONBIO, &nonblocking);

    int ret = 0;
    while(ptr->active.load() && !(wrapper && wrapper->shouldStop())) {
        ret = recv(ptr->getClientSocket(), ptr->buf, BufferSize, 0);
        if(ret > 0) {
            continue;
        } else if(ret == 0) {
            fprintf(stderr, "Client %s disconnected gracefully\n", ptr->getClientIpPort().c_str());
            ptr->active = 0;
            break;
        } else if(ret < 0) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                fprintf(stderr, "is server force disconnecting?\n");
                break;
            }
            fprintf(stderr, "Client %s disconnected abruptedly. error\n", ptr->getClientIpPort().c_str());
            ptr->active = 0;
            break;
        }
    }
    ptr->active.store(false);
    ShutdownController::getInstance().unregisterResource();
}

bool isInvalidAddress(sockaddr_in* addr) {
    // Filter 0.0.0.0:0
    if (addr->sin_addr.s_addr == 0 && addr->sin_port == 0) return true;
    
    // Filter broadcast addresses
    if (addr->sin_addr.s_addr == INADDR_BROADCAST) return true;

    return false;
}

void socketLoop() {
    ShutdownController::getInstance().registerResource();

    int socketfile = -1;
    sockaddr_in sa;
    int iResult;

    // Create socket
    socketfile = (int) socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketfile == -1) {
        printf("socket failed: %d\n", WSAGetLastError());
        return;
    }

    //set reusable
    int setsockopt_opt = 1;
    setsockopt(socketfile, SOL_SOCKET, SO_REUSEADDR, (const char*) &setsockopt_opt, sizeof(setsockopt_opt));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(DEFAULT_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind
    if ((iResult = bind(socketfile, (sockaddr*) &sa, sizeof(sa))) == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        closesocket(socketfile);
        return;
    }

    // Listen
    if ((iResult = listen(socketfile, SOMAXCONN)) == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(socketfile);
        return;
    }

    printf("server init\n");
    while(!ShutdownController::getInstance().isShutdownRequested()) {
        sockaddr_in incomingAddress;
        const int incomingAddressSize = sizeof(incomingAddress);
        int incomingSocket = (int) accept(socketfile, (sockaddr*) &incomingAddress, const_cast<int*>(&incomingAddressSize));
        if(isInvalidAddress(&incomingAddress)) closesocket(incomingSocket);
        ClientSession *clientInstance = new ClientSession(incomingSocket, incomingAddress);
        printf("client %s connected!\n", clientInstance->getClientIpPort().c_str());
        clientVector.pushBack(clientInstance);

        ThreadWrapper th (clientListener, clientInstance, &th);
    }

    // Force close all client sessions during shutdown
    for(int i = 0; i < clientVector.size(); i++) {
        clientVector[i]->active = false;
        shutdown(clientVector[i]->getClientSocket(), SD_BOTH);
        closesocket(clientVector[i]->getClientSocket());
    }
    clientVector.removeClosed();

    shutdown(socketfile, SD_BOTH);
    closesocket(socketfile);
    ShutdownController::getInstance().unregisterResource();
}

void RunGui() {
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // ImGui::ShowDemoWindow();

    clientVector.removeClosed();

    ImGui::Begin("server panel", nullptr, ImGuiWindowFlags_None);
    ImGui::SeparatorText("Connected Clients");
    ImGui::Text("Active clients: %d", clientVector.size());
    for(int i=0; i < clientVector.size(); i++) {
        auto& ref = clientVector[i];

        // ImGui::PushID(ref->getClientSocket());
        if(ImGui::CollapsingHeader(ref->getClientIpPort().c_str())) {
            ImGui::Text("Connected for: %s", ref->getConnectionDuration().c_str());
            // if(ref->isBufferUpdated()) {
                static std::string localInput;
                std::string displayData(ref->buf, strnlen(ref->buf, BufferSize));
                ImGui::InputTextMultiline(ref->makeWidgetName("Received Data").c_str(), 
                    const_cast<char*>(displayData.c_str()), displayData.size() + 1, 
                    ImVec2(-1, 100), ImGuiInputTextFlags_ReadOnly);
                ImGui::InputText(ref->makeWidgetName("Send to client").c_str(), &localInput);
                if(ImGui::Button(ref->makeWidgetName("Send").c_str()) && !localInput.empty()) {
                    ThreadWrapper sendThread([](ClientSession *c, const std::string& tstr) {
                        int sent = send(c->getClientSocket(), tstr.c_str(), (int)tstr.size(), 0);
                        if(sent != tstr.size()) {
                            fprintf(stderr, "Failed to send complete message to %s\n", 
                                   c->getClientIpPort().c_str());
                        }
                    }, ref, localInput);
                    localInput.clear();
                }
            // }
        }
        // ImGui::PopID();
    }
    ImGui::SeparatorText("System");
    ImGui::Text("Average FPS: %d", (int) roundf(io.Framerate));
    ImGui::End();
}

void RunSFMLBackend() {
    // sf::RenderWindow window(sf::VideoMode(800, 600), "ServerInterface", sf::Style::Titlebar | sf::Style::Close);
    sf::RenderWindow window(sf::VideoMode(800, 600), "ServerInterface");
    window.setVerticalSyncEnabled(true);
    if(!ImGui::SFML::Init(window)) {
        throw new std::runtime_error("SFML ImGui can't be initialized!");
    }
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;

    sf::Clock deltaClock;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) {
                window.close();
                ShutdownController::getInstance().requestShutdown();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // ImGui::SetNextWindowSize(ImVec2(0, 0));
        // ImGui::SetWindowSize(ImVec2(500, 400));
        RunGui();

        window.clear(sf::Color(0, 0, 128, 255));
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
}

int main(void) {
    WSADATA wsaData;
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    ThreadWrapper socketThread(socketLoop);
    socketThread.run();
    RunSFMLBackend();

    ShutdownController::getInstance().requestShutdown();
    ShutdownController::getInstance().waitForShutdown();

    WSACleanup();
    return 0;
}