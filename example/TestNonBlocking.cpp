

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

int main() {
    WSADATA wsaData;
    SOCKET ListenSocket = INVALID_SOCKET;
    fd_set masterSet, readSet;
    SOCKET clientSockets[FD_SETSIZE];
    int maxSocket, i, result;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed: %d\n", result);
        return 1;
    }

    struct addrinfo *addrResult = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &addrResult);
    if (result != 0) {
        printf("getaddrinfo failed: %d\n", result);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections
    ListenSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed: %d\n", WSAGetLastError());
        freeaddrinfo(addrResult);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    result = bind(ListenSocket, addrResult->ai_addr, (int)addrResult->ai_addrlen);
    if (result == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        freeaddrinfo(addrResult);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(addrResult);

    result = listen(ListenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Initialize the client socket array
    for (i = 0; i < FD_SETSIZE; i++) {
        clientSockets[i] = INVALID_SOCKET;
    }

    // Add the listener socket to the master set
    FD_ZERO(&masterSet);
    FD_SET(ListenSocket, &masterSet);
    maxSocket = ListenSocket;

    printf("Server is running on port %s\n", DEFAULT_PORT);

    // Main server loop
    while (1) {
        // Make a copy of the master set for select()
        readSet = masterSet;

        // Wait for network events
        result = select(maxSocket + 1, &readSet, NULL, NULL, NULL);
        if (result == SOCKET_ERROR) {
            printf("select failed: %d\n", WSAGetLastError());
            break;
        }

        // Check for new connections
        if (FD_ISSET(ListenSocket, &readSet)) {
            SOCKET newClient = accept(ListenSocket, NULL, NULL);
            if (newClient == INVALID_SOCKET) {
                printf("accept failed: %d\n", WSAGetLastError());
                continue;
            }

            // Set the new socket to non-blocking mode
            u_long mode = 1;
            ioctlsocket(newClient, FIONBIO, &mode);

            // Add the new socket to the master set
            for (i = 0; i < FD_SETSIZE; i++) {
                if (clientSockets[i] == INVALID_SOCKET) {
                    clientSockets[i] = newClient;
                    FD_SET(newClient, &masterSet);
                    if (newClient > maxSocket) {
                        maxSocket = newClient;
                    }
                    printf("New client connected: %d\n", newClient);
                    break;
                }
            }

            if (i == FD_SETSIZE) {
                printf("Too many clients\n");
                closesocket(newClient);
            }
        }

        // Check existing clients for data
        for (i = 0; i < FD_SETSIZE; i++) {
            SOCKET clientSocket = clientSockets[i];
            if (clientSocket == INVALID_SOCKET) continue;

            if (FD_ISSET(clientSocket, &readSet)) {
                result = recv(clientSocket, recvbuf, recvbuflen, 0);
                if (result > 0) {
                    // Echo the data back to the client
                    printf("Received %d bytes from client %d: %.*s\n", 
                           result, clientSocket, result, recvbuf);
                    send(clientSocket, recvbuf, result, 0);
                } else if (result == 0) {
                    // Connection closed
                    printf("Client %d disconnected\n", clientSocket);
                    closesocket(clientSocket);
                    FD_CLR(clientSocket, &masterSet);
                    clientSockets[i] = INVALID_SOCKET;
                } else {
                    // Error occurred
                    int error = WSAGetLastError();
                    if (error != WSAEWOULDBLOCK) {
                        printf("recv failed: %d\n", error);
                        closesocket(clientSocket);
                        FD_CLR(clientSocket, &masterSet);
                        clientSockets[i] = INVALID_SOCKET;
                    }
                }
            }
        }
    }

    // Cleanup
    for (i = 0; i < FD_SETSIZE; i++) {
        if (clientSockets[i] != INVALID_SOCKET) {
            closesocket(clientSockets[i]);
        }
    }
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}