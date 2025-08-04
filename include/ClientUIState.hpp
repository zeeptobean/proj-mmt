#pragma once

#include <bits/stdc++.h>
using namespace std;

class ClientUIState {
    std::mutex rawMessageLock;
    std::string rawMessage;
    
    ClientUIState() = default;
    ClientUIState(const ClientUIState&) = delete;
    ClientUIState& operator=(const ClientUIState&) = delete;
    
    public:
    std::atomic<bool> isKeylogger{false};

    static ClientUIState& getInstance() {
        static ClientUIState instance;
        return instance;
    }

    void setRawMessage(const std::string& tstr) {
        std::lock_guard<std::mutex> lock(rawMessageLock);
        rawMessage = tstr;
    }

    std::string getRawMessage() {
        std::lock_guard<std::mutex> lock(rawMessageLock);
        return rawMessage;
    }
};