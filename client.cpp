#define WIN32_LEAN_AND_MEAN
#define UNICODE
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
#include <SFML/Audio.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include <bits/stdc++.h>
#include "ThreadWrapper.hpp"
#include "ImGuiStdString.hpp"
#include "GuiScrollableText.hpp"
#include "Message.hpp"
using namespace std;

#define DEFAULT_BUFLEN 2048
#define DEFAULT_PORT 62300
const int BufferSize = 2048;
char buf[BufferSize+1];

sf::SoundBuffer connectedSoundBuffer, disconnectedSoundBuffer;
sf::Sound connectedSound, disconnectedSound;

std::mutex socketMutex;

std::string inputBuffer;

class ClientConnectionManager {
private:
    std::atomic<int> socketfile{-1};
    std::atomic<bool> connecting{false};
    std::atomic<bool> connected{false};
    std::queue<std::string> receivedMessages;
    
    void listenToServer() {
        while (connected.load()) {
            memset(buf, 0, sizeof(buf));
            int bytesReceived = recv(socketfile.load(), buf, BufferSize, 0);
            buf[bytesReceived] = '\0';
            if (bytesReceived > 0) {
                int status = proceedEncryptedMessage(this->outputBuffer, ret, fullData, lastDataRemainingSize, nonce, NULL);
                if(!crypt.checkOtherPublicKeyStatus()) {
                    if(status == 2) {
                        std::array<uint8_t, 32> outkey;
                        for(int i=0; i < 32; i++) outkey[i] = fullData[i];
                        crypt.setOtherPublicKey(outkey);
                    } else {
                        miniConsole.AddLineWarning("Client %s haven't send their key, can't proceed data", this->getClientIpPort().c_str());
                    }
                } else {
                    if(status == 2) {
                        miniConsole.AddLineInfo("Client %s want to reauth, but not implemented", this->getClientIpPort().c_str());
                    } else {
                        miniConsole.AddLine("Client %s sent data, remaining need to be sent: %d", this->getClientIpPort().c_str(), lastDataRemainingSize);
                        if(lastDataRemainingSize == 0) {
                            miniConsole.AddLine("Client %s complete sent data", this->getClientIpPort().c_str());
                            std::vector<uint8_t> plainText;
                            Message msg;
                            crypt.decrypt(fullData, nonce, plainText);
                            ret = assembleMessage((char*) plainText.data(), plainText.size(), msg);
                            if(!ret) {
                                miniConsole.AddLineError("Client %s can't setup message", this->getClientIpPort().c_str());
                            }
                        }
                    }
                }
            } else if (bytesReceived == 0) {
                // Connection closed
                connected.store(false);
                break;
            }
            else {
                // Error occurred
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    connected.store(false);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

public:
    bool isConnected() const { return connected.load(); }
    bool isConnecting() const { return connecting.load(); }

    bool connectToServer(const std::string& address = "127.0.0.1", uint16_t port = DEFAULT_PORT) {
        if (connecting.load() || connected.load()) return false;

        connecting.store(true);
        
        std::lock_guard<std::mutex> lock(socketMutex);
        int newSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (newSocket == INVALID_SOCKET) {
            connecting.store(false);
            return false;
        }

        // Set socket options
        int reuse = 1;
        setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &sa.sin_addr);

        if (::connect(newSocket, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) {
            closesocket(newSocket);
            connecting.store(false);
            return false;
        }

        socketfile.store(newSocket);
        connecting.store(false);
        connected.store(true);

        ThreadWrapper messageThread (&ClientConnectionManager::listenToServer, this);
        messageThread.run();

        return true;
    }

    void disconnect() {
        if (!connected.load()) return;
        disconnectedSound.play();

        std::lock_guard<std::mutex> lock(socketMutex);
        SOCKET currentSocket = socketfile.load();
        if (currentSocket != INVALID_SOCKET) {
            shutdown(currentSocket, SD_BOTH);
            closesocket(currentSocket);
        }
        socketfile.store(-1);
        connected.store(false);
    }

    int sendData(const char* data, size_t length) {
        if (!connected.load()) return -1;

        std::lock_guard<std::mutex> lock(socketMutex);
        int currentSocket = socketfile.load();
        if (currentSocket == -1) return -1;

        return send(currentSocket, data, (int) length, 0);
    }

    ~ClientConnectionManager() {
        disconnect();
    }
};

ClientConnectionManager connectionManager;

void RunGui() {
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::Begin("Client", nullptr, ImGuiWindowFlags_None);
    
    // Connection status section
    ImGui::SeparatorText("Connection Status");
    if (connectionManager.isConnected()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Connected");
        if (ImGui::Button("Disconnect")) {
            std::thread([] {
                connectionManager.disconnect();
            }).detach();
        }
    } else if (connectionManager.isConnecting()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Connecting...");
        if (ImGui::Button("Cancel")) {
            connectionManager.disconnect();
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Status: Disconnected");
        if (ImGui::Button("Connect")) {
            std::thread([] {
                if (connectionManager.connectToServer()) {
                    connectedSound.play();
                }
            }).detach();
        }
    }
    if (connectionManager.isConnected()) {
        ImGui::SeparatorText("Raw messages received");
        if (ImGui::BeginChild("Messages", ImVec2(-1, 150), true)) {
            ImGui::TextWrapped("%s", buf);
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

void RunSFMLBackend() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "Network Client", 
                           sf::Style::Titlebar | sf::Style::Close);
    window.setVerticalSyncEnabled(true);
    
    if (!ImGui::SFML::Init(window)) {
        throw std::runtime_error("Failed to initialize ImGui-SFML");
    }
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;

    // Load sound resources with error checking
    try {
        if (!connectedSoundBuffer.loadFromFile("asset/CONNECTED.ogg")) {
            throw std::runtime_error("Failed to load asset/CONNECTED.ogg");
        }
        connectedSound.setBuffer(connectedSoundBuffer);
        
        if (!disconnectedSoundBuffer.loadFromFile("asset/DISCONNECTED.ogg")) {
            throw std::runtime_error("Failed to load asset/DISCONNECTED.ogg");
        }
        disconnectedSound.setBuffer(disconnectedSoundBuffer);
    } catch (const std::exception& e) {
        std::cerr << "Sound loading error: " << e.what() << std::endl;
    }

    sf::Clock deltaClock;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());
        RunGui();
        
        window.clear(sf::Color(30, 30, 40));
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *lpCmdLine, int nCmdShow) {
    //Attach console for debugging
    /*
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    */
    
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    try {
        RunSFMLBackend();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }

    connectionManager.disconnect();

    WSACleanup();
    return 0;
}