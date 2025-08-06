#pragma once

#include <vector>
#include <string>
#include <deque>
#include <chrono>
#include <mutex>
#include <memory>
#include <imgui.h>
#include "InternalUtilities.hpp"

class GuiScrollableTextDisplay {
public:
    // Constructor with optional max history size (0 for unlimited)
    explicit GuiScrollableTextDisplay(size_t maxHistory = 1000);

    // Clear all history
    void Clear();

    // Draw the widget
    void Draw(const char* title = "Text Display", bool border = true, const ImVec2& size = ImVec2(0, 0));

    // Set maximum history size (0 for unlimited)
    void SetMaxHistory(size_t maxHistory);

    inline void AddLine(const std::string& text) {
        AddLineImpl(text, IM_COL32(255, 255, 255, 255));
    }
    inline void AddLine(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(255, 255, 255, 255), fmt, args);
        va_end(args);
    }
    inline void AddLineError(const std::string& text) {
        AddLineImpl(text, IM_COL32(255, 50, 50, 255));
    }
    inline void AddLineError(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(255, 50, 50, 255), fmt, args);
        va_end(args);
    }
    inline void AddLineSuccess(const std::string& text) {
        AddLineImpl(text, IM_COL32(0, 255, 0, 255));
    }
    inline void AddLineSuccess(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(0, 255, 0, 255), fmt, args);
        va_end(args);
    }
    inline void AddLineWarning(const std::string& text) {
        AddLineImpl(text, IM_COL32(255, 255, 0, 255));
    }
    inline void AddLineWarning(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(255, 255, 0, 255), fmt, args);
        va_end(args);
    }
    inline void AddLineInfo(const std::string& text) {
        AddLineImpl(text, IM_COL32(0, 255, 255, 255));
    }
    inline void AddLineInfo(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(0, 255, 255, 255), fmt, args);
        va_end(args);
    }

private:
    struct TextEntry {
        std::string text;
        ImU32 color;
    };
    std::deque<TextEntry> history;
    size_t maxHistorySize;
    bool autoScroll;
    bool showTimestamps;
    bool scrollToBottom = false;
    std::mutex mutex;

    void AddLineFmtImpl(ImU32 color, const char* fmt, va_list args);

    // Common implementation
    void AddLineImpl(const std::string& text, ImU32 color);
};