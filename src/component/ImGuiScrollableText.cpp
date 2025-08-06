#include "ImGuiScrollableText.hpp"

GuiScrollableTextDisplay::GuiScrollableTextDisplay(size_t maxHistory) 
    : maxHistorySize(maxHistory), autoScroll(true), showTimestamps(true) {}

void GuiScrollableTextDisplay::Clear() {
    std::lock_guard<std::mutex> lock(mutex);
    history.clear();
}

void GuiScrollableTextDisplay::Draw(const char* title, bool border, const ImVec2& size) {
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

void GuiScrollableTextDisplay::SetMaxHistory(size_t maxHistory) {
    std::lock_guard<std::mutex> lock(mutex);
    maxHistorySize = maxHistory;
    // Trim existing history if needed
    if (maxHistorySize > 0 && history.size() > maxHistorySize) {
        history.erase(history.begin(), history.begin() + (history.size() - maxHistorySize));
    }
}

void GuiScrollableTextDisplay::AddLineFmtImpl(ImU32 color, const char* fmt, va_list args) {
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

void GuiScrollableTextDisplay::AddLineImpl(const std::string& text, ImU32 color) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Get current time if timestamps are enabled
    std::string timestamp;
    if (showTimestamps) {
        timestamp = getCurrentIsoTime() + ": ";
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