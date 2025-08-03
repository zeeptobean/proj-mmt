#pragma once

#include <vector>
#include <string>
#include <deque>
#include <chrono>
#include <mutex>
#include <memory>
#include <imgui.h>

class ScrollableTextDisplay {
public:
    // Constructor with optional max history size (0 for unlimited)
    explicit ScrollableTextDisplay(size_t maxHistory = 1000) 
        : maxHistorySize(maxHistory), autoScroll(true), showTimestamps(true) {}

    // Clear all history
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex);
        history.clear();
    }

    // Draw the widget
    void Draw(const char* title = "Text Display", bool border = true, const ImVec2& size = ImVec2(0, 0)) {
                // First copy the current state under lock
        std::deque<TextEntry> historyCopy;
        bool localAutoScroll, localScrollToBottom;
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            historyCopy = history;
            localAutoScroll = autoScroll;
            localScrollToBottom = scrollToBottom;
        }

        if (!ImGui::BeginChild(title, size, border, ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGui::EndChild();
            return;
        }

        // Display options (these modify member variables, so need lock)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ImGui::Checkbox("Auto-scroll", &autoScroll);
            ImGui::SameLine();
            ImGui::Checkbox("Timestamps", &showTimestamps);
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                history.clear();
            }
        }

        ImGui::Separator();

        // Display all text lines (using our local copy)
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        for (const auto& entry : historyCopy) {
            ImGui::PushStyleColor(ImGuiCol_Text, entry.color);
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar();

        // Auto-scroll logic
        if (localScrollToBottom && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() || !localAutoScroll)) {
            ImGui::SetScrollHereY(1.0f);
        }

        // Reset scroll flag under lock
        if (localScrollToBottom) {
            std::lock_guard<std::mutex> lock(mutex);
            scrollToBottom = false;
        }

        ImGui::EndChild();
    }

    // Set maximum history size (0 for unlimited)
    void SetMaxHistory(size_t maxHistory) {
        std::lock_guard<std::mutex> lock(mutex);
        maxHistorySize = maxHistory;
        // Trim existing history if needed
        if (maxHistorySize > 0 && history.size() > maxHistorySize) {
            history.erase(history.begin(), history.begin() + (history.size() - maxHistorySize));
        }
    }

    void AddLine(const std::string& text) {
        AddLineImpl(text, IM_COL32(255, 255, 255, 255));
    }
    void AddLine(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(255, 255, 255, 255), fmt, args);
        va_end(args);
    }
    void AddLineError(const std::string& text) {
        AddLineImpl(text, IM_COL32(255, 50, 50, 255));
    }
    void AddLineError(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(255, 50, 50, 255), fmt, args);
        va_end(args);
    }
    void AddLineSuccess(const std::string& text) {
        AddLineImpl(text, IM_COL32(0, 255, 0, 255));
    }
    void AddLineSuccess(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(0, 255, 0, 255), fmt, args);
        va_end(args);
    }
    void AddLineWarning(const std::string& text) {
        AddLineImpl(text, IM_COL32(255, 255, 0, 255));
    }
    void AddLineWarning(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AddLineFmtImpl(IM_COL32(255, 255, 0, 255), fmt, args);
        va_end(args);
    }
    void AddLineInfo(const std::string& text) {
        AddLineImpl(text, IM_COL32(0, 255, 255, 255));
    }
    void AddLineInfo(const char* fmt, ...) {
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

    void AddLineFmtImpl(ImU32 color, const char* fmt, va_list args) {
        // Determine required buffer size
        va_list args_copy;
        va_copy(args_copy, args);
        const int size = vsnprintf(nullptr, 0, fmt, args_copy) + 1; // +1 for null terminator
        va_end(args_copy);
        
        if (size > 0) {
            // Allocate buffer and format string
            std::unique_ptr<char[]> buf(new char[size]);
            vsnprintf(buf.get(), size, fmt, args);
            AddLineImpl(std::string(buf.get()), color);
        }
    }

    // Common implementation for both AddLine and AddLineFmt
    void AddLineImpl(const std::string& text, ImU32 color) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Get current time if timestamps are enabled
        std::string timestamp;
        if (showTimestamps) {
            timestamp = GetCurrentTimestamp() + ": ";
        }

        // Add to history
        history.emplace_back(TextEntry{timestamp + text, color});
        
        // Trim history if we've exceeded max size
        if (maxHistorySize > 0 && history.size() > maxHistorySize) {
            history.pop_front();
        }
        
        // Auto-scroll will happen on next draw
        scrollToBottom = autoScroll;
    }

    std::string GetCurrentTimestamp() {
        using namespace std::chrono;
        
        // Get current time
        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = system_clock::to_time_t(now);
        
        // Convert to tm struct
        struct tm *tm = localtime(&timer);
        
        // Format as string
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
        
        return std::string(buffer) + "." + std::to_string(ms.count());
    }
};