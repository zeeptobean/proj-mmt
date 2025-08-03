#ifndef TMP_HELPER
#define TMP_HELPER

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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <future>

using namespace std;

#define PORT 24127
#define BUFFER_SIZE 100 // Define buffer size as a constant


/*
//WSA init must be init by the main function
class TCPConnection {
    private:
    sockaddr_in sa;
    bool isClosed = false, isSocketFail = false;
    int socketfile = -1, res = 0;

    void initSocket() {
        socketfile = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        isSocketFail = socketfile == -1;
    }

    public:
    TCPConnection(uint16_t port) {
        initSocket();
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all available interfaces
    }

    TCPConnection(uint8_t ipv4_3, uint8_t ipv4_2, uint8_t ipv4_1, uint8_t ipv4_0, uint16_t port = 0) {
        initSocket();
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = ((uint32_t) ipv4_0 << 24) | ((uint32_t) ipv4_1 << 16) | ((uint32_t) ipv4_2 << 8) | ipv4_3;
    }
    
    TCPConnection(const char *ipaddr, uint16_t port = 0) {
        initSocket();
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = inet_addr(ipaddr);
    }

    ~TCPConnection() {
        if(!isClosed) close();
    }

    bool connect() {
        if(isSocketFail) return false;
        res = ::connect(socketfile, (sockaddr*) &sa, sizeof(sa));
        return !(res == -1);
    }

    bool send(char *buf, size_t len) {
        if(isSocketFail) return false;
        res = ::send(socketfile, buf, len, 0);
        return !(res == -1);
    }

    void close() {
        if(socketfile != -1) {
        #ifdef WIN32
            closesocket(socketfile);
        #else
            close(sockfd);
        #endif
        }
        isClosed = true;
    }
};

class TCPListener {
    private:
    sockaddr_in sa;
    bool isClosed = false, isSocketFail = false;
    int socketfile = -1, res = 0;

    void initSocket() {
        socketfile = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        isSocketFail = socketfile == -1;
    }

    bool listenImpl() {
        close();
        initSocket();
        if(isSocketFail) return false;
        if(::listen(socketfile, SOMAXCONN) == 0) {
            return isSocketFail = true;
        }
        return true;
    }

    void close() {
        if(socketfile != -1) {
        #ifdef WIN32
            closesocket(socketfile);
        #else
            close(sockfd);
        #endif
        }
        isClosed = true;
    }

    public:
    TCPListener() = default;

    ~TCPListener() {
        if(!isClosed) close();
    }

    bool listen(uint8_t ipv4_3, uint8_t ipv4_2, uint8_t ipv4_1, uint8_t ipv4_0, uint16_t port) {
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = ((uint32_t) ipv4_0 << 24) | ((uint32_t) ipv4_1 << 16) | ((uint32_t) ipv4_2 << 8) | ipv4_3;
        return listenImpl();
    }

    bool listen(const char *ipaddr, uint16_t port) {
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = inet_addr(ipaddr);
    }

    bool listen(uint16_t port) {
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all available interfaces
    }

    uint16_t getAssignedPort() const {
        // return 
    }

    
};

*/

int oldmain() {
    int res;

    #ifdef WIN32
    WSADATA wsaData;
    res = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (res != 0) {
        cerr << "WSAStartup failed: " << res << endl;
        return 1;
    }
    #endif

    // Create a UDP socket (SOCK_DGRAM)
    int sockfd = (int) socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        perror("socket creation failed");
        #ifdef WIN32
        WSACleanup();
        #endif
        return 1;
    }

    // Set reusable address option
    int setsockopt_opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*) &setsockopt_opt, sizeof(setsockopt_opt)) == -1) {
        perror("setsockopt SO_REUSEADDR failed");
        #ifdef WIN32
        closesocket(sockfd);
        WSACleanup();
        #else
        close(sockfd);
        #endif
        return 1;
    }

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all available interfaces

    // Bind the socket to the specified port and address
    if (bind(sockfd, (sockaddr*) &sa, sizeof(sa)) == -1) {
        perror("bind failed");
        #ifdef WIN32
        closesocket(sockfd);
        WSACleanup();
        #else
        close(sockfd);
        #endif
        return 1;
    }

    cout << "UDP Server listening on port " << PORT << endl;

    char buffer[BUFFER_SIZE+1];
    unsigned int counter = 0;
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    do {
        memset(buffer, 0, sizeof buffer); // Clear buffer for each new message
        int readcnt = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (sockaddr*)&client_addr, &client_addr_len);

        if (readcnt > 0) {
            buffer[readcnt] = '\0'; // Null-terminate the received data
            cout << "Call " << counter++ << ". Received " << readcnt << " bytes from "
                 << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port)
                 << ", data: " << buffer << endl;
        } else if (readcnt == 0) {
            cout << "Client disconnected (this generally doesn't happen with UDP in the same way as TCP)" << endl;
        } else {
            #ifdef WIN32
            cerr << "recvfrom failed with error: " << WSAGetLastError() << endl;
            #else
            perror("recvfrom failed");
            #endif
        }

        // Sleep after processing (or attempting to process) a message
        // this_thread::sleep_for(chrono::milliseconds(750));
    } while(true);

    #ifdef WIN32
    closesocket(sockfd);
    WSACleanup();
    #else
    close(sockfd);
    #endif

    return 0;
}

#endif