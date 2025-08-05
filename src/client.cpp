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
#include "ImGuiScrollableText.hpp"
#include "Message.hpp"
#include "CryptHandler.hpp"
#include "ClientUIState.hpp"
#include "InternalUtilities.hpp"
using namespace std;

#define DEFAULT_BUFLEN 2048
#define DEFAULT_PORT 62300
const int BufferSize = 2048;
char buf[BufferSize+1];

sf::SoundBuffer connectedSoundBuffer, disconnectedSoundBuffer;
sf::Sound connectedSound, disconnectedSound;

std::mutex socketMutex;

std::string inputBuffer;

CryptHandler crypt;

GuiScrollableTextDisplay miniConsole;

int MessageExecute(const Message& inputMessage, Message& outputMessage) {
    int command = inputMessage.commandNumber;
    switch(command) {
        case MessageRawText: {
            std::string rawMessage(inputMessage.getBinaryData(), inputMessage.getBinaryDataSize());
            ClientUIState::getInstance().setRawMessage(rawMessage);
            return 1;
            //do not send back;
        }
        default: break;
    }
    return 0;
}

class ClientConnectionManager {
private:
    std::atomic<int> socketfile{-1};
    std::atomic<bool> connecting{false};
    std::atomic<bool> connected{false};
    std::queue<std::string> receivedMessages;
    
    void listenToServer() {
        std::vector<uint8_t> fullData;
        int lastDataRemainingSize = 0;
        std::array<uint8_t, 12> nonce;

        //send public key
        if(sendPublicKey()) {
            miniConsole.AddLineError("Successfully  send public key to server");
        } else {
            miniConsole.AddLineError("Failed to send public key to server");
        }

        while (connected.load()) {
            memset(buf, 0, sizeof(buf));
            int bytesReceived = recv(socketfile.load(), buf, BufferSize, 0);
            buf[bytesReceived] = '\0';
            if (bytesReceived > 0) {
                int status = proceedEncryptedMessage(buf, bytesReceived, fullData, lastDataRemainingSize, nonce, NULL);
                if(!crypt.checkOtherPublicKeyStatus()) {
                    if(status == 2) {
                        std::array<uint8_t, 32> outkey;
                        for(int i=0; i < 32; i++) outkey[i] = fullData[i];
                        crypt.setOtherPublicKey(outkey);
                    } else {
                        miniConsole.AddLineWarning("Server haven't send their key, can't proceed data");
                    }
                } else {
                    if(status == 2) {
                        miniConsole.AddLineInfo("Server want to reauth, but not implemented");
                    } else {
                        miniConsole.AddLine("Server sent data, remaining need to be sent: %d", lastDataRemainingSize);
                        if(lastDataRemainingSize == 0) {
                            miniConsole.AddLine("Server complete sent data");
                            std::vector<uint8_t> plainText;
                            Message msg;
                            crypt.decrypt(fullData, nonce, plainText);
                            bytesReceived = assembleMessage((char*) plainText.data(), plainText.size(), msg);
                            if(bytesReceived) {
                                ThreadWrapper([&msg]{
                                    Message outmsg;
                                    if(MessageExecute(msg, outmsg)) {
                                        miniConsole.AddLineSuccess("Successfully executed message!");
                                    } else {
                                        miniConsole.AddLineWarning("Couldn't execute message!");
                                    }
                                });
                            } else {
                                miniConsole.AddLineError("Server can't setup message");
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

    int sendData(const Message& msg) {
        char *data;
        int dataSize;
        int ret = prepareMessage(msg, data, dataSize, NULL);
        if(!ret) {
            miniConsole.AddLineError("Failed to prepare message");
            return 0;
        }
        std::vector<uint8_t> dataVec(dataSize, 0);
        std::vector<uint8_t> cipherText;
        std::array<uint8_t, 12> nonce;
        
        uint8_t *dataVecPtr = dataVec.data();
        memcpy(dataVecPtr, data, dataSize);
        delete[] data;
        crypt.encrypt(dataVec, cipherText, nonce);
        
        uint8_t *noncePtr = nonce.data();
        uint8_t *cipherTextPtr = cipherText.data();

        std::vector<uint8_t> fullEncryptedSegment (cipherText.size() + 24, 0);
        int fullEncryptedSegmentSize = (int) fullEncryptedSegment.size();
        uint8_t *fullEncryptedSegmentPtr = fullEncryptedSegment.data();
        memcpy(fullEncryptedSegmentPtr, "ZZTE", 4);
        memcpy(fullEncryptedSegmentPtr+4, &fullEncryptedSegmentSize, 4);
        memcpy(fullEncryptedSegmentPtr+8, noncePtr, 12);
        memcpy(fullEncryptedSegmentPtr+24, cipherTextPtr, cipherText.size());
        int remaining = fullEncryptedSegment.size();

        int sent = 0;
        while(remaining > 0) {
            sent = send(this->socketfile.load(), (char*) cipherText.data()+sent, remaining, 0);
            if(sent == -1) {
                miniConsole.AddLineError("Failed to send complete message");
                return 0;
            }
            remaining -= sent;
        }
        return 1;
    }

    int sendPublicKey() {
        char tbuf[40];
        tbuf[0] = 'Z';
        tbuf[1] = 'Z';
        tbuf[2] = 'T';
        tbuf[3] = 'E';
        tbuf[4] = (char) 0xff;
        tbuf[5] = (char) 0xff;
        tbuf[6] = (char) 0xff;
        tbuf[7] = (char) 0xff;
        std::array<uint8_t, 32> tpubkey = crypt.getPublicKey();
        memcpy(tbuf+8, tpubkey.data(), 32);

        int sent = send(this->socketfile.load(), tbuf, 40, 0);
        if(sent != 40) {
            return 0;
        }
        return 1;
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
            ImGui::TextWrapped("%s", ClientUIState::getInstance().getRawMessage().c_str());
        }
        ImGui::EndChild();
    }
    ImGui::SeparatorText("Console");
    miniConsole.Draw();

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
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);
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