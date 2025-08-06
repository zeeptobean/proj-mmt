#pragma once

#include <string>

struct FunctionalityStruct {
    std::string rawText = "";
    bool isKeyloggerActive = false;
    bool isScreenCapActive = false;

    void reset() {
        rawText = "";
        isKeyloggerActive = false;
        isScreenCapActive = false;
    }
};