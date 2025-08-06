////////////////////////// ImGui InputTextXXX with std::string ////////////////////

#pragma once

#include "imgui.h"
#include <string>

namespace ImGui
{
    IMGUI_API bool  InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
    IMGUI_API bool  InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
    IMGUI_API bool  InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
}

struct InputTextCallback_UserData
{
    std::string*            Str;
    ImGuiInputTextCallback  ChainCallback;
    void*                   ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data);

bool ImGui::InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);

bool ImGui::InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);

bool ImGui::InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);