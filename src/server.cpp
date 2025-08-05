#include "server.hpp"

std::map<std::string, uint32_t> clientNamingPool;

ServerConnectionManager connectionManager;

ClientVector clientVector;

GuiScrollableTextDisplay miniConsole;

CryptHandler crypt;

void ServerConnectionManager::acceptClientLoop() {
    sockaddr_in incomingAddress;
    const int incomingAddressSize = sizeof(incomingAddress);
    while(running.load()) {
        int incomingSocket = (int) accept(socketfile, (sockaddr*) &incomingAddress, const_cast<int*>(&incomingAddressSize));
        if(incomingSocket == -1) continue;  //skip?
        if(isInvalidAddress(&incomingAddress)) closesocket(incomingSocket);
        ClientSession *clientInstance = new ClientSession(incomingSocket, incomingAddress);
        miniConsole.AddLineSuccess("client %s connected!\n", clientInstance->getClientIpPort().c_str());
        clientVector.pushBack(clientInstance);
    }
}

void ServerConnectionManager::init() {
    int newSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (newSocket == -1) {
        running.store(false);
        // throw std::runtime_error("socket failed");
    }
    int reuse = 1;
    setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(DEFAULT_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(newSocket, (sockaddr*)&sa, sizeof(sa)) == -1) {
        closesocket(newSocket);
        running.store(false);
        // throw std::runtime_error("bind failed");
    }

    if (listen(newSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(newSocket);
        running.store(false);
        // throw std::runtime_error("listen failed");
    }

    socketfile.store(newSocket);
    running.store(true);
    miniConsole.AddLineSuccess("Server initialized!");

    ThreadWrapper looper(ServerConnectionManager::acceptClientLoop, this);
}

void ServerConnectionManager::disconnect() {
    if (!running.load()) return;
    int currentSocket = socketfile.load();
    if(currentSocket != -1) {
        shutdown(currentSocket, SD_BOTH);
        closesocket(currentSocket);
    }
    
    socketfile.store(-1);
    running.store(false);
}

//could be called within a thread
bool ServerConnectionManager::sendData(ClientSession *c, const Message& msg) {
    char *data;
    int dataSize;
    int ret = prepareMessage(msg, data, dataSize, NULL);
    if(!ret) {
        miniConsole.AddLineError("Failed to prepare message for %s\n", c->getClientIpPort().c_str());
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
        sent = send(c->getClientSocket(), (char*) cipherText.data()+sent, remaining, 0);
        if(sent == -1) {
            miniConsole.AddLineError("Failed to send complete message to %s\n", c->getClientIpPort().c_str());
            return 0;
        }
        remaining -= sent;
    }
    return 1;
}

//Each entity handle key sending on their own
//could be called within a thread
bool ServerConnectionManager::sendPublicKey(ClientSession *c) {
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

    int sent = send(c->getClientSocket(), tbuf, 40, 0);
    if(sent != 40) {
        miniConsole.AddLineError("Failed to send server public key to %s\n", c->getClientIpPort().c_str());
        return 0;
    }
    return 1;
}

///////////////////////////////////

void ClientSession::init() {
    active = 1;
    char *tmp = inet_ntoa(clientAddress.sin_addr);
    clientIp = std::string(tmp);
    clientPort = ntohs(clientAddress.sin_port);
    connectionTime = std::chrono::system_clock::now();
    ThreadWrapper th (ClientSession::clientListener, this);
}

void ClientSession::clientListener() {
    std::vector<uint8_t> fullData;
    int lastDataRemainingSize = 0;
    std::array<uint8_t, 12> nonce;

    if(connectionManager.sendPublicKey(this)) {
        miniConsole.AddLineSuccess("Successfully sent key to client %s", this->getClientIpPort().c_str());
    } else {
        miniConsole.AddLineError("Failed sent key to client %s", this->getClientIpPort().c_str());
    }

    while(this->active.load()) {
        int ret = recv(this->getClientSocket(), this->outputBuffer, BufferSize, 0);
        this->outputBuffer[ret] = 0;
        if(ret > 0) {
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
        } else if(ret == 0) {
            miniConsole.AddLineInfo("Client %s disconnected gracefully", this->getClientIpPort().c_str());
            break;
        } else if(ret < 0) {
            /*
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                miniConsole.AddLineWarning("is server force disconnecting?\n");
            }
            */
            miniConsole.AddLineWarning("Client %s disconnected abruptedly\n", this->getClientIpPort().c_str());
            break;
        }
    }
    this->active.store(false);
}

ClientSession::ClientSession(const int& _socket, const sockaddr_in& _clientAddress) : socket(_socket), clientAddress(_clientAddress) {
    init();
}

ClientSession::~ClientSession() {
    if(active) {
        active = false;
    }
    shutdown(socket, SD_BOTH);
    closesocket(socket);
}

/////////////////////////////////////////////////////// 

void ClientVector::removeClosed() {
    std::lock_guard<std::mutex> lock(clientVecMutex);
    auto removePtr = std::remove_if(clientVec.begin(), clientVec.end(), [](ClientSession*& c) {
        return !c->active.load();
    });

    for(auto ite = removePtr; ite != clientVec.end(); ite++) {
        delete *ite;
    }

    clientVec.erase(removePtr, clientVec.end());
}

void ClientVector::pushBack(ClientSession* c) {
    std::lock_guard<std::mutex> lock(clientVecMutex);
    clientVec.emplace_back(c);
}

int ClientVector::size() {
    std::lock_guard<std::mutex> lock(clientVecMutex);
    return (int) clientVec.size();
}

ClientSession*& ClientVector::operator[](int index) {
    std::lock_guard<std::mutex> lock(clientVecMutex);
    return clientVec.at(index);
}

///////////////////////////////

void RunGui() {
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // ImGui::ShowDemoWindow();

    clientVector.removeClosed();

    ImGui::Begin("server panel", nullptr, ImGuiWindowFlags_None);
    ImGui::SeparatorText("Connected Clients");
    ImGui::Text("Active clients: %d", clientVector.size());
    for(int i=0; i < clientVector.size(); i++) {
        auto& ref = clientVector[i];

        if(ImGui::CollapsingHeader(ref->getClientIpPort().c_str())) {
            ImGui::Text("Connected for: %s", ref->getConnectionDuration().c_str());
            std::string displayData(ref->outputBuffer, strnlen(ref->outputBuffer, BufferSize));
            ImGui::InputText(ref->makeWidgetName("Send to client").c_str(), &clientVector[i]->inputBuffer);
            if(ImGui::Button(ref->makeWidgetName("Send").c_str()) && !clientVector[i]->inputBuffer.empty()) {
                ThreadWrapper sendThread([](ClientSession *c) {
                    if(crypt.checkOtherPublicKeyStatus()) {
                        Message textMessage;
                        textMessage.commandNumber = MessageRawText;
                        textMessage.setBinaryData(c->inputBuffer.c_str(), c->inputBuffer.size());
                        if(connectionManager.sendData(c, textMessage)) {
                            c->inputBuffer.clear();
                            miniConsole.AddLineSuccess("Client %s: Meesage sent", c->getClientIpPort());
                        }
                    } else {
                        miniConsole.AddLineError("Client %s: Can't send meesage. Hasn't received client public key", c->getClientIpPort());
                    }
                }, ref);
            }
            if(ImGui::Button(ref->makeWidgetName("Kick").c_str())) {
                ref->active.store(false);
                clientVector.removeClosed();
            }
        }
    }
    ImGui::SeparatorText("Console");
    miniConsole.Draw();
    ImGui::SeparatorText("System");
    ImGui::Text("Average FPS: %d", (int) roundf(io.Framerate));
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

int main(void) {
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