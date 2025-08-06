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
#include "PeerConnection.hpp"
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

Peer

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