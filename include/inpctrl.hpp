/*
===============================================================================
CrossInput - Cross-Platform Input Handling Library (Header-Only)
===============================================================================

Author: 3443
Date: [19/11/2025]
License: MIT

Description:
-------------
CrossInput is a lightweight, header-only C++ library for handling keyboard
and mouse input in a cross-platform manner. It is designed primarily for 
macro or automation tools. The library currently supports:

    - Windows (via GetAsyncKeyState + low-level keyboard hook + SendInput)
    - Linux (via /dev/input event devices and uinput for synthetic input)

It provides a simple API to check key states, simulate key presses/releases,
and move the mouse programmatically. This library is intended to be simple
to integrate into existing projects with minimal setup.

Features:
----------
- Check if a key is currently pressed.
- Press, hold, and release keyboard keys.
- Type text strings with automatic character mapping.
- Move the mouse relative to its current position.
- Map between human-readable keys and system key codes.
- Thread-safe key state tracking.
- Header-only, cross-platform, no external dependencies (besides standard 
  C++ and system headers).
- Designed for simplicity and quick integration into macros or automation 
  scripts.

Usage Example (C++):
---------------------
#include "inpctrl.hpp"
#include <iostream>

int main() {
    CrossInput input;

    if (!input.init()) {
        std::cerr << "Failed to initialize input system!" << std::endl;
        return 1;
    }

    // Press and release the 'A' key
    input.pressKey(CrossInput::Key::A);

    // Type a string
    input.typeText("Hello World!");

    // Move mouse 100 pixels right and 50 pixels down
    input.moveMouse(100, 50);

    input.cleanup();
    return 0;
}

Notes:
------
- On Linux, running this library requires root privileges or access to
  /dev/uinput and /dev/input/event* devices.
- Windows implementation uses low-level hooks and SendInput API.
- MacOS is not supported.
- Designed for macros, automation, and input simulation, not games or
  high-performance input tracking.

===============================================================================
*/


#ifndef INPCTRL_HPP
#define INPCTRL_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 
    #include <windows.h>
#else
    #include <linux/input-event-codes.h>
    #include <linux/uinput.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/ioctl.h>
    #include <vector>
#endif

class CrossInput {
public:
    // Cross-platform key codes
    enum class Key : unsigned int {
        // Letters
        A = 0x41, B = 0x42, C = 0x43, D = 0x44, E = 0x45, F = 0x46,
        G = 0x47, H = 0x48, I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C,
        M = 0x4D, N = 0x4E, O = 0x4F, P = 0x50, Q = 0x51, R = 0x52,
        S = 0x53, T = 0x54, U = 0x55, V = 0x56, W = 0x57, X = 0x58,
        Y = 0x59, Z = 0x5A,
        
        // Numbers
        Num0 = 0x30, Num1 = 0x31, Num2 = 0x32, Num3 = 0x33, Num4 = 0x34,
        Num5 = 0x35, Num6 = 0x36, Num7 = 0x37, Num8 = 0x38, Num9 = 0x39,
        
        // Function keys
        F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75,
        F7 = 0x76, F8 = 0x77, F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,
        
        // Special keys
        Space = 0x20, Enter = 0x0D, Tab = 0x09, Escape = 0x1B,
        Backspace = 0x08, Delete = 0x2E, Insert = 0x2D,
        
        // Modifiers
        LShift = 0xA0, RShift = 0xA1, LCtrl = 0xA2, RCtrl = 0xA3,
        LAlt = 0xA4, RAlt = 0xA5,
        
        // Arrow keys
        Left = 0x25, Up = 0x26, Right = 0x27, Down = 0x28,
        
        // Mouse buttons
        LMB = 0x01, RMB = 0x02, MMB = 0x04,
        Mouse4 = 0x05, Mouse5 = 0x06,
        
        // Brackets
        LeftBracket = 0xDB, RightBracket = 0xDD,

        //Other
        Slash = 0xBF,
        Semicolon = 0xBA,  // QWERTY ';' key
        Colon = 0xBA,       // Use same physical key, send with shift
        Exclamation = 0x31,    // 1 + Shift
        At = 0x32,             // 2 + Shift
        Hash = 0x33,           // 3 + Shift
        Dollar = 0x34,         // 4 + Shift
        Percent = 0x35,        // 5 + Shift
        Caret = 0x36,          // 6 + Shift
        Ampersand = 0x37,      // 7 + Shift
        Asterisk = 0x38,       // 8 + Shift
        LeftParen = 0x39,      // 9 + Shift
        RightParen = 0x30,     // 0 + Shift
        Minus = 0xBD,          // - key
        Underscore = 0xBD,     // - + Shift
        Equal = 0xBB,          // = key
        Plus = 0xBB,           // = + Shift
        Backslash = 0xDC,      // \ key
        Pipe = 0xDC,           // \ + Shift
        Quote = 0xDE,          // ' key
        DoubleQuote = 0xDE,    // ' + Shift
        Comma = 0xBC,          // , key
        Less = 0xBC,           // , + Shift
        Dot = 0xBE,            // . key
        Greater = 0xBE,        // . + Shift
        Grave = 0xC0,          // ` key
        Tilde = 0xC0,          // ` + Shift

        // AZERTY-specific (physical keys)
        AZ_Slash = 0xBF,       // / key on AZERTY
        AZ_Colon = 0xBA,       // : key on AZERTY (next to !)
        AZ_Exclamation = 0x31, // ! key
        AZ_At = 0x33,          // @ key
        AZ_Hash = 0x34,        // # key
        // Navigation keys
        Home = 0x24, End = 0x23, PageUp = 0x21, PageDown = 0x22,

        // Numpad keys
        Numpad0 = 0x60, Numpad1 = 0x61, Numpad2 = 0x62, Numpad3 = 0x63,
        Numpad4 = 0x64, Numpad5 = 0x65, Numpad6 = 0x66, Numpad7 = 0x67,
        Numpad8 = 0x68, Numpad9 = 0x69,
        NumpadMultiply = 0x6A, NumpadAdd = 0x6B, NumpadSubtract = 0x6D,
        NumpadDecimal = 0x6E, NumpadDivide = 0x6F,

        // Lock keys
        CapsLock = 0x14, NumLock = 0x90, ScrollLock = 0x91,

        // System keys
        PrintScreen = 0x2C, Pause = 0x13,

        // Windows/Super key
        LWin = 0x5B, RWin = 0x5C,

    };

    CrossInput() : m_running(false), m_initialized(false) {
#ifdef _WIN32
        m_hookHandle = NULL;
#else
        m_uinputFd = -1;
#endif
    }

    ~CrossInput() {
        cleanup();
    }

    // Initialize the input system
    bool init() {
        if (m_initialized) return true;
        
#ifdef _WIN32
        return initWindows();
#else
        return initLinux();
#endif
    }

    // Cleanup resources
    void cleanup() {
        if (!m_initialized) return;
        
        m_running = false;
        
        if (m_listenerThread.joinable()) {
            m_listenerThread.join();
        }
        
#ifdef _WIN32
        cleanupWindows();
#else
        cleanupLinux();
#endif
        
        m_initialized = false;
    }

    // Check if a key is currently pressed
    bool isKeyPressed(Key key) {
        unsigned int code = static_cast<unsigned int>(key);
#ifdef _WIN32
        // On Windows, use GetAsyncKeyState for more reliable detection
        return (GetAsyncKeyState(code) & 0x8000) != 0;
#else
        std::lock_guard<std::mutex> lock(m_keyMutex);
        return m_keyStates[code];
#endif
    }

    // Press and hold a key
    void holdKey(Key key) {
        unsigned int code = static_cast<unsigned int>(key);
#ifdef _WIN32
        holdKeyWindows(code);
#else
        holdKeyLinux(toEvdevCode(code));
#endif
    }

    // Release a key
    void releaseKey(Key key) {
        unsigned int code = static_cast<unsigned int>(key);
#ifdef _WIN32
        releaseKeyWindows(code);
#else
        releaseKeyLinux(toEvdevCode(code));
#endif
    }

    // Press and release a key (single tap)
    void pressKey(Key key, int delayMs = 50) {
        holdKey(key);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        releaseKey(key);
    }

    // Type a string of text
    void typeText(const std::string& text, int delayBetweenKeys = 30) {
        for (char c : text) {
            typeChar(c, delayBetweenKeys);
        }
    }

    // Move mouse relative to current position
    void moveMouse(int dx, int dy) {
#ifdef _WIN32
        moveMouseWindows(dx, dy);
#else
        moveMouseLinux(dx, dy);
#endif
    }

    // Get human-readable key name
    std::string getKeyName(Key key) {
        unsigned int code = static_cast<unsigned int>(key);
        
        static std::unordered_map<unsigned int, std::string> names = {
            {0x41, "A"}, {0x42, "B"}, {0x43, "C"}, {0x44, "D"}, {0x45, "E"},
            {0x46, "F"}, {0x47, "G"}, {0x48, "H"}, {0x49, "I"}, {0x4A, "J"},
            {0x4B, "K"}, {0x4C, "L"}, {0x4D, "M"}, {0x4E, "N"}, {0x4F, "O"},
            {0x50, "P"}, {0x51, "Q"}, {0x52, "R"}, {0x53, "S"}, {0x54, "T"},
            {0x55, "U"}, {0x56, "V"}, {0x57, "W"}, {0x58, "X"}, {0x59, "Y"},
            {0x5A, "Z"}, 
            {0x30, "0"}, {0x31, "1"}, {0x32, "2"}, {0x33, "3"}, {0x34, "4"},
            {0x35, "5"}, {0x36, "6"}, {0x37, "7"}, {0x38, "8"}, {0x39, "9"},
            {0x20, "Space"}, {0x0D, "Enter"}, {0x09, "Tab"},
            {0x1B, "Escape"}, {0x08, "Backspace"}, {0x2E, "Delete"}, {0x2D, "Insert"},
            {0x70, "F1"}, {0x71, "F2"}, {0x72, "F3"},
            {0x73, "F4"}, {0x74, "F5"}, {0x75, "F6"}, {0x76, "F7"},
            {0x77, "F8"}, {0x78, "F9"}, {0x79, "F10"}, {0x7A, "F11"},
            {0x7B, "F12"}, 
            {0xDB, "["}, {0xDD, "]"}, 
            {0xBF, "/"}, {0xBA, ";"}, 
            {0xBD, "-"}, {0xBB, "="}, {0xDC, "\\"}, 
            {0xDE, "'"}, {0xBC, ","}, {0xBE, "."}, {0xC0, "`"},
            {0x25, "Left"}, {0x26, "Up"}, {0x27, "Right"}, {0x28, "Down"},
            {0xA0, "LShift"}, {0xA1, "RShift"}, 
            {0xA2, "LCtrl"}, {0xA3, "RCtrl"},
            {0xA4, "LAlt"}, {0xA5, "RAlt"},
            // Mouse buttons
            {0x01, "LMB"}, {0x02, "RMB"}, {0x04, "MMB"},
            {0x05, "Mouse4"}, {0x06, "Mouse5"}, // Navigation keys
            {0x24, "Home"}, {0x23, "End"}, {0x21, "PageUp"}, {0x22, "PageDown"},
            // Numpad
            {0x60, "Numpad0"}, {0x61, "Numpad1"}, {0x62, "Numpad2"}, {0x63, "Numpad3"},
            {0x64, "Numpad4"}, {0x65, "Numpad5"}, {0x66, "Numpad6"}, {0x67, "Numpad7"},
            {0x68, "Numpad8"}, {0x69, "Numpad9"},
            {0x6A, "Numpad*"}, {0x6B, "Numpad+"}, {0x6D, "Numpad-"},
            {0x6E, "Numpad."}, {0x6F, "Numpad/"},
            // Lock keys
            {0x14, "CapsLock"}, {0x90, "NumLock"}, {0x91, "ScrollLock"},
            // System keys
            {0x2C, "PrintScreen"}, {0x13, "Pause"},
            // Windows key
            {0x5B, "LWin"}, {0x5C, "RWin"},
        };
        
        if (names.count(code)) {
            return names[code];
        }
        return "Unknown";
    }

    //A function that gets the current pressed key.
    Key getCurrentPressedKey(int timeout_ms = 0) {
#ifdef _WIN32
        return getCurrentPressedKeyWindows(timeout_ms);
#else
        return getCurrentPressedKeyLinux(timeout_ms);
#endif
    }

private:
    std::unordered_map<unsigned int, bool> m_keyStates;
    std::mutex m_keyMutex;
    std::thread m_listenerThread;
    std::atomic<bool> m_running;
    bool m_initialized;

    // Type a single character
    void typeChar(char c, int delayMs = 30) {
#ifdef _WIN32
        typeCharWindows(c, delayMs);
#else
        typeCharLinux(c, delayMs);
#endif
    }

#ifdef _WIN32
    // ==================== WINDOWS IMPLEMENTATION ====================
    HHOOK m_hookHandle;
    
    static CrossInput* s_instance;  // Declaration only
    
    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && s_instance) {
            const KBDLLHOOKSTRUCT* pkbhs = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
            
            // Only track non-injected keys
            if ((pkbhs->flags & LLKHF_INJECTED) == 0) {
                bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
                
                std::lock_guard<std::mutex> lock(s_instance->m_keyMutex);
                s_instance->m_keyStates[pkbhs->vkCode] = isDown;
            }
        }
        return CallNextHookEx(s_instance->m_hookHandle, nCode, wParam, lParam);
    }
    
    bool initWindows() {
        s_instance = this;
        
        // Install keyboard hook
        m_hookHandle = SetWindowsHookEx(
            WH_KEYBOARD_LL, 
            keyboardHookProc, 
            GetModuleHandle(NULL), 
            0
        );
        
        if (!m_hookHandle) {
            std::cerr << "Failed to install keyboard hook. Error: " << GetLastError() << std::endl;
            return false;
        }
        
        m_running = true;
        
        // Start message pump thread for the hook
        m_listenerThread = std::thread([this]() { 
            windowsEventLoop(); 
        });
        
        m_initialized = true;
        std::cout << "Windows input initialized (using GetAsyncKeyState + hook)" << std::endl;
        return true;
    }
    
    void cleanupWindows() {
        if (m_hookHandle) {
            UnhookWindowsHookEx(m_hookHandle);
            m_hookHandle = NULL;
        }
        s_instance = nullptr;
    }
    
    void windowsEventLoop() {
        MSG msg;
        // Process messages to keep hook alive
        while (m_running) {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void holdKeyWindows(unsigned int vkCode) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        
        // For the slash key (and other OEM keys), use scan code
        if (vkCode == 0xBF) {  // VK_OEM_2 (slash key)
            input.ki.wScan = 0x35;  // Hardware scan code for /
            input.ki.dwFlags = KEYEVENTF_SCANCODE;
        } else {
            input.ki.wVk = vkCode;
            input.ki.wScan = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
            input.ki.dwFlags = 0;
        }
        
        SendInput(1, &input, sizeof(INPUT));
    }

    void releaseKeyWindows(unsigned int vkCode) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        
        // For the slash key (and other OEM keys), use scan code
        if (vkCode == 0xBF) {  // VK_OEM_2 (slash key)
            input.ki.wScan = 0x35;  // Hardware scan code for /
            input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        } else {
            input.ki.wVk = vkCode;
            input.ki.wScan = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
            input.ki.dwFlags = KEYEVENTF_KEYUP;
        }
        
        SendInput(1, &input, sizeof(INPUT));
    }
    
    void moveMouseWindows(int dx, int dy) {
        INPUT input = {0};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = dx;
        input.mi.dy = dy;
        SendInput(1, &input, sizeof(INPUT));
    }

    void typeCharWindows(char c, int delayMs) {
        // Convert char to virtual key and shift state
        SHORT vk = VkKeyScanA(c);
        if (vk == -1) {
            // Character not available in current keyboard layout
            std::cerr << "Character '" << c << "' not available in keyboard layout" << std::endl;
            return;
        }
        
        BYTE keyCode = LOBYTE(vk);
        BYTE shiftState = HIBYTE(vk);
        
        // Press shift if needed
        bool needShift = (shiftState & 1);
        bool needCtrl = (shiftState & 2);
        bool needAlt = (shiftState & 4);
        
        if (needShift) holdKeyWindows(VK_SHIFT);
        if (needCtrl) holdKeyWindows(VK_CONTROL);
        if (needAlt) holdKeyWindows(VK_MENU);
        
        // Press the key
        holdKeyWindows(keyCode);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        releaseKeyWindows(keyCode);
        
        // Release modifiers
        if (needAlt) releaseKeyWindows(VK_MENU);
        if (needCtrl) releaseKeyWindows(VK_CONTROL);
        if (needShift) releaseKeyWindows(VK_SHIFT);
    }

    Key getCurrentPressedKeyWindows(int timeout_ms) {
        auto startTime = std::chrono::steady_clock::now();
        
        do {
            // Check all possible virtual key codes
            for (unsigned int vk = 0x01; vk <= 0xFE; vk++) {
                // Check if key is pressed
                if (GetAsyncKeyState(vk) & 0x8000) {
                    // Wait for key to be released before returning
                    while (GetAsyncKeyState(vk) & 0x8000) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    return static_cast<Key>(vk);
                }
            }
            
            if (timeout_ms == 0) {
                break; // No wait, check once
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            if (timeout_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime
                ).count();
                if (elapsed >= timeout_ms) {
                    break;
                }
            }
            
        } while (timeout_ms != 0);
        
        return static_cast<Key>(0); // No key pressed
    }
#else
    // ==================== LINUX IMPLEMENTATION ====================
    int m_uinputFd;
    std::vector<int> m_inputFds;
    
    bool initLinux() {
        // Initialize uinput for output
        m_uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (m_uinputFd < 0) {
            std::cout << "Failed to open /dev/uinput. Run with sudo!" << std::endl;
            return true;
        }
        
        struct uinput_setup setup;
        memset(&setup, 0, sizeof(setup));
        strcpy(setup.name, "CrossInput Virtual Device");
        setup.id.bustype = BUS_USB;
        setup.id.vendor = 0x1234;
        setup.id.product = 0x5678;
        setup.id.version = 1;
        
        // Enable key events
        ioctl(m_uinputFd, UI_SET_EVBIT, EV_KEY);
        for (int i = 0; i < 256; ++i) {
            ioctl(m_uinputFd, UI_SET_KEYBIT, i);
        }
        
        // Enable mouse button events
        ioctl(m_uinputFd, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(m_uinputFd, UI_SET_KEYBIT, BTN_RIGHT);
        ioctl(m_uinputFd, UI_SET_KEYBIT, BTN_MIDDLE);
        ioctl(m_uinputFd, UI_SET_KEYBIT, BTN_SIDE);
        ioctl(m_uinputFd, UI_SET_KEYBIT, BTN_EXTRA);

        // Enable mouse movement
        ioctl(m_uinputFd, UI_SET_EVBIT, EV_REL);
        ioctl(m_uinputFd, UI_SET_RELBIT, REL_X);
        ioctl(m_uinputFd, UI_SET_RELBIT, REL_Y);
        
        // Create device
        ioctl(m_uinputFd, UI_DEV_SETUP, &setup);
        ioctl(m_uinputFd, UI_DEV_CREATE);
        
        // Start input listener thread
        m_running = true;
        m_listenerThread = std::thread([this]() { linuxEventLoop(); });
        
        m_initialized = true;
        std::cout << "Linux input initialized" << std::endl;
        return true;
    }
    
    void cleanupLinux() {
        if (m_uinputFd >= 0) {
            ioctl(m_uinputFd, UI_DEV_DESTROY);
            close(m_uinputFd);
            m_uinputFd = -1;
        }
        
        for (int fd : m_inputFds) {
            close(fd);
        }
        m_inputFds.clear();
    }
    
    void linuxEventLoop() {
        // Open all input devices
        DIR* dir = opendir("/dev/input");
        if (!dir) return;
        
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strncmp(ent->d_name, "event", 5) == 0) {
                std::string path = "/dev/input/" + std::string(ent->d_name);
                int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    m_inputFds.push_back(fd);
                }
            }
        }
        closedir(dir);
        
        struct input_event ev;
        while (m_running) {
            for (int fd : m_inputFds) {
                ssize_t n = read(fd, &ev, sizeof(ev));
                if (n == sizeof(ev)) {
                    // Handle keyboard events
                    if (ev.type == EV_KEY && ev.code < 256) {
                        unsigned int winCode = fromEvdevCode(ev.code);
                        std::lock_guard<std::mutex> lock(m_keyMutex);
                        m_keyStates[winCode] = (ev.value != 0);
                    }
                    // Handle mouse button events
                    else if (ev.type == EV_KEY) {
                        unsigned int winCode = 0;
                        if (ev.code == BTN_LEFT) winCode = 0x01;       // LMB
                        else if (ev.code == BTN_RIGHT) winCode = 0x02; // RMB
                        else if (ev.code == BTN_MIDDLE) winCode = 0x04; // MMB
                        else if (ev.code == BTN_SIDE) winCode = 0x05;   // Mouse4
                        else if (ev.code == BTN_EXTRA) winCode = 0x06;  // Mouse5
                        
                        if (winCode != 0) {
                            std::lock_guard<std::mutex> lock(m_keyMutex);
                            m_keyStates[winCode] = (ev.value != 0);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void emitEvent(int type, int code, int val) {
        if (m_uinputFd < 0) return;
        
        struct input_event ie;
        memset(&ie, 0, sizeof(ie));
        ie.type = type;
        ie.code = code;
        ie.value = val;
        write(m_uinputFd, &ie, sizeof(ie));
        
        // Sync event
        memset(&ie, 0, sizeof(ie));
        ie.type = EV_SYN;
        ie.code = SYN_REPORT;
        ie.value = 0;
        write(m_uinputFd, &ie, sizeof(ie));
    }
    
    void holdKeyLinux(unsigned int evdevCode) {
        emitEvent(EV_KEY, evdevCode, 1);
    }
    
    void releaseKeyLinux(unsigned int evdevCode) {
        emitEvent(EV_KEY, evdevCode, 0);
    }
    
    void moveMouseLinux(int dx, int dy) {
        emitEvent(EV_REL, REL_X, dx);
        emitEvent(EV_REL, REL_Y, dy);
    }

    void typeCharLinux(char c, int delayMs) {
        // Map of common ASCII characters to their Linux key codes and shift requirements
        struct KeyMapping {
            unsigned int keyCode;
            bool needShift;
        };
        
        static std::unordered_map<char, KeyMapping> charMap = {
            // Lowercase letters
            {'a', {KEY_A, false}}, {'b', {KEY_B, false}}, {'c', {KEY_C, false}},
            {'d', {KEY_D, false}}, {'e', {KEY_E, false}}, {'f', {KEY_F, false}},
            {'g', {KEY_G, false}}, {'h', {KEY_H, false}}, {'i', {KEY_I, false}},
            {'j', {KEY_J, false}}, {'k', {KEY_K, false}}, {'l', {KEY_L, false}},
            {'m', {KEY_M, false}}, {'n', {KEY_N, false}}, {'o', {KEY_O, false}},
            {'p', {KEY_P, false}}, {'q', {KEY_Q, false}}, {'r', {KEY_R, false}},
            {'s', {KEY_S, false}}, {'t', {KEY_T, false}}, {'u', {KEY_U, false}},
            {'v', {KEY_V, false}}, {'w', {KEY_W, false}}, {'x', {KEY_X, false}},
            {'y', {KEY_Y, false}}, {'z', {KEY_Z, false}},
            
            // Uppercase letters
            {'A', {KEY_A, true}}, {'B', {KEY_B, true}}, {'C', {KEY_C, true}},
            {'D', {KEY_D, true}}, {'E', {KEY_E, true}}, {'F', {KEY_F, true}},
            {'G', {KEY_G, true}}, {'H', {KEY_H, true}}, {'I', {KEY_I, true}},
            {'J', {KEY_J, true}}, {'K', {KEY_K, true}}, {'L', {KEY_L, true}},
            {'M', {KEY_M, true}}, {'N', {KEY_N, true}}, {'O', {KEY_O, true}},
            {'P', {KEY_P, true}}, {'Q', {KEY_Q, true}}, {'R', {KEY_R, true}},
            {'S', {KEY_S, true}}, {'T', {KEY_T, true}}, {'U', {KEY_U, true}},
            {'V', {KEY_V, true}}, {'W', {KEY_W, true}}, {'X', {KEY_X, true}},
            {'Y', {KEY_Y, true}}, {'Z', {KEY_Z, true}},
            
            // Numbers
            {'0', {KEY_0, false}}, {'1', {KEY_1, false}}, {'2', {KEY_2, false}},
            {'3', {KEY_3, false}}, {'4', {KEY_4, false}}, {'5', {KEY_5, false}},
            {'6', {KEY_6, false}}, {'7', {KEY_7, false}}, {'8', {KEY_8, false}},
            {'9', {KEY_9, false}},
            
            // Shifted numbers (symbols)
            {'!', {KEY_1, true}}, {'@', {KEY_2, true}}, {'#', {KEY_3, true}},
            {'$', {KEY_4, true}}, {'%', {KEY_5, true}}, {'^', {KEY_6, true}},
            {'&', {KEY_7, true}}, {'*', {KEY_8, true}}, {'(', {KEY_9, true}},
            {')', {KEY_0, true}},
            
            // Special characters
            {' ', {KEY_SPACE, false}}, {'\n', {KEY_ENTER, false}}, {'\t', {KEY_TAB, false}},
            {'-', {KEY_MINUS, false}}, {'_', {KEY_MINUS, true}},
            {'=', {KEY_EQUAL, false}}, {'+', {KEY_EQUAL, true}},
            {'[', {KEY_LEFTBRACE, false}}, {'{', {KEY_LEFTBRACE, true}},
            {']', {KEY_RIGHTBRACE, false}}, {'}', {KEY_RIGHTBRACE, true}},
            {'\\', {KEY_BACKSLASH, false}}, {'|', {KEY_BACKSLASH, true}},
            {';', {KEY_SEMICOLON, false}}, {':', {KEY_SEMICOLON, true}},
            {'\'', {KEY_APOSTROPHE, false}}, {'"', {KEY_APOSTROPHE, true}},
            {',', {KEY_COMMA, false}}, {'<', {KEY_COMMA, true}},
            {'.', {KEY_DOT, false}}, {'>', {KEY_DOT, true}},
            {'/', {KEY_SLASH, false}}, {'?', {KEY_SLASH, true}},
            {'`', {KEY_GRAVE, false}}, {'~', {KEY_GRAVE, true}},
            {'/', {KEY_SLASH, false}}, {':', {KEY_SEMICOLON, true}},
            {'!', {KEY_1, true}}, {'@', {KEY_2, true}}, {'#', {KEY_3, true}},
            {'$', {KEY_4, true}}, {'%', {KEY_5, true}}, {'^', {KEY_6, true}},
            {'&', {KEY_7, true}}, {'*', {KEY_8, true}}, {'(', {KEY_9, true}},
            {')', {KEY_0, true}}, {'-', {KEY_MINUS, false}}, {'_', {KEY_MINUS, true}},
            {'=', {KEY_EQUAL, false}}, {'+', {KEY_EQUAL, true}}, {'\\', {KEY_BACKSLASH, false}},
            {'|', {KEY_BACKSLASH, true}}, {';', {KEY_SEMICOLON, false}}, {':', {KEY_SEMICOLON, true}},
            {'\'', {KEY_APOSTROPHE, false}}, {'"', {KEY_APOSTROPHE, true}}, {',', {KEY_COMMA, false}},
            {'<', {KEY_COMMA, true}}, {'.', {KEY_DOT, false}}, {'>', {KEY_DOT, true}},
            {'`', {KEY_GRAVE, false}}, {'~', {KEY_GRAVE, true}}

        };
        
        auto it = charMap.find(c);
        if (it == charMap.end()) {
            std::cerr << "Character '" << c << "' not mapped for Linux" << std::endl;
            return;
        }
        
        KeyMapping mapping = it->second;
        
        // Press shift if needed
        if (mapping.needShift) {
            holdKeyLinux(KEY_LEFTSHIFT);
        }
        
        // Press the key
        holdKeyLinux(mapping.keyCode);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        releaseKeyLinux(mapping.keyCode);
        
        // Release shift
        if (mapping.needShift) {
            releaseKeyLinux(KEY_LEFTSHIFT);
        }
    }
    
    // Convert Windows VK codes to evdev codes
    unsigned int toEvdevCode(unsigned int vkCode) {
        static std::unordered_map<unsigned int, unsigned int> vkToEvdev = {
            {0x41, KEY_A}, {0x42, KEY_B}, {0x43, KEY_C}, {0x44, KEY_D},
            {0x45, KEY_E}, {0x46, KEY_F}, {0x47, KEY_G}, {0x48, KEY_H},
            {0x49, KEY_I}, {0x4A, KEY_J}, {0x4B, KEY_K}, {0x4C, KEY_L},
            {0x4D, KEY_M}, {0x4E, KEY_N}, {0x4F, KEY_O}, {0x50, KEY_P},
            {0x51, KEY_Q}, {0x52, KEY_R}, {0x53, KEY_S}, {0x54, KEY_T},
            {0x55, KEY_U}, {0x56, KEY_V}, {0x57, KEY_W}, {0x58, KEY_X},
            {0x59, KEY_Y}, {0x5A, KEY_Z},
            {0x30, KEY_0}, {0x31, KEY_1}, {0x32, KEY_2}, {0x33, KEY_3},
            {0x34, KEY_4}, {0x35, KEY_5}, {0x36, KEY_6}, {0x37, KEY_7},
            {0x38, KEY_8}, {0x39, KEY_9},
            {0x70, KEY_F1}, {0x71, KEY_F2}, {0x72, KEY_F3}, {0x73, KEY_F4},
            {0x74, KEY_F5}, {0x75, KEY_F6}, {0x76, KEY_F7}, {0x77, KEY_F8},
            {0x78, KEY_F9}, {0x79, KEY_F10}, {0x7A, KEY_F11}, {0x7B, KEY_F12},
            {0x20, KEY_SPACE}, {0x0D, KEY_ENTER}, {0x09, KEY_TAB},
            {0x1B, KEY_ESC}, {0xA0, KEY_LEFTSHIFT}, {0xA1, KEY_RIGHTSHIFT},
            {0xA2, KEY_LEFTCTRL}, {0xA3, KEY_RIGHTCTRL},
            {0xA4, KEY_LEFTALT}, {0xA5, KEY_RIGHTALT},
            {0xDB, KEY_LEFTBRACE}, {0xDD, KEY_RIGHTBRACE}, {0xBF, KEY_SLASH}, {0xBA, KEY_SEMICOLON}, // QWERTY and AZERTY support
            {0xBF, KEY_SLASH},      // /
            {0xBA, KEY_SEMICOLON},  // ; or :
            {0x31, KEY_1},          // 1
            {0x32, KEY_2},          // 2
            {0x33, KEY_3},          // 3
            {0x34, KEY_4},          // 4
            {0x35, KEY_5},          // 5
            {0x36, KEY_6},          // 6
            {0x37, KEY_7},          // 7
            {0x38, KEY_8},          // 8
            {0x39, KEY_9},          // 9
            {0x30, KEY_0},          // 0
            {0xBD, KEY_MINUS},      // - or _
            {0xBB, KEY_EQUAL},      // = or +
            {0xDC, KEY_BACKSLASH},  // \ or |
            {0xDE, KEY_APOSTROPHE}, // ' or "
            {0xBC, KEY_COMMA},      // , or <
            {0xBE, KEY_DOT},        // . or >
            {0xC0, KEY_GRAVE},      // ` or ~
            // Navigation keys
            {0x24, KEY_HOME}, {0x23, KEY_END}, {0x21, KEY_PAGEUP}, {0x22, KEY_PAGEDOWN},
            // Numpad
            {0x60, KEY_KP0}, {0x61, KEY_KP1}, {0x62, KEY_KP2}, {0x63, KEY_KP3},
            {0x64, KEY_KP4}, {0x65, KEY_KP5}, {0x66, KEY_KP6}, {0x67, KEY_KP7},
            {0x68, KEY_KP8}, {0x69, KEY_KP9},
            {0x6A, KEY_KPASTERISK}, {0x6B, KEY_KPPLUS}, {0x6D, KEY_KPMINUS},
            {0x6E, KEY_KPDOT}, {0x6F, KEY_KPSLASH},
            // Lock keys
            {0x14, KEY_CAPSLOCK}, {0x90, KEY_NUMLOCK}, {0x91, KEY_SCROLLLOCK},
            // System keys
            {0x2C, KEY_SYSRQ}, {0x13, KEY_PAUSE},
            // Windows/Super key
            {0x5B, KEY_LEFTMETA}, {0x5C, KEY_RIGHTMETA},
            // Arrow keys (if not already there)
            {0x25, KEY_LEFT}, {0x26, KEY_UP}, {0x27, KEY_RIGHT}, {0x28, KEY_DOWN},
            // Backspace, Delete, Insert (if not already there)
            {0x08, KEY_BACKSPACE}, {0x2E, KEY_DELETE}, {0x2D, KEY_INSERT},

        };
        
        if (vkToEvdev.count(vkCode)) {
            return vkToEvdev[vkCode];
        }
        return vkCode;
    }
    
    // Convert evdev codes back to Windows VK codes
    unsigned int fromEvdevCode(unsigned int evdevCode) {
        static std::unordered_map<unsigned int, unsigned int> evdevToVk = {
            {KEY_A, 0x41}, {KEY_B, 0x42}, {KEY_C, 0x43}, {KEY_D, 0x44},
            {KEY_E, 0x45}, {KEY_F, 0x46}, {KEY_G, 0x47}, {KEY_H, 0x48},
            {KEY_I, 0x49}, {KEY_J, 0x4A}, {KEY_K, 0x4B}, {KEY_L, 0x4C},
            {KEY_M, 0x4D}, {KEY_N, 0x4E}, {KEY_O, 0x4F}, {KEY_P, 0x50},
            {KEY_Q, 0x51}, {KEY_R, 0x52}, {KEY_S, 0x53}, {KEY_T, 0x54},
            {KEY_U, 0x55}, {KEY_V, 0x56}, {KEY_W, 0x57}, {KEY_X, 0x58},
            {KEY_Y, 0x59}, {KEY_Z, 0x5A},
            {KEY_0, 0x30}, {KEY_1, 0x31}, {KEY_2, 0x32}, {KEY_3, 0x33},
            {KEY_4, 0x34}, {KEY_5, 0x35}, {KEY_6, 0x36}, {KEY_7, 0x37},
            {KEY_8, 0x38}, {KEY_9, 0x39},
            {KEY_F1, 0x70}, {KEY_F2, 0x71}, {KEY_F3, 0x72}, {KEY_F4, 0x73},
            {KEY_F5, 0x74}, {KEY_F6, 0x75}, {KEY_F7, 0x76}, {KEY_F8, 0x77},
            {KEY_F9, 0x78}, {KEY_F10, 0x79}, {KEY_F11, 0x7A}, {KEY_F12, 0x7B},
            {KEY_SPACE, 0x20}, {KEY_ENTER, 0x0D}, {KEY_TAB, 0x09},
            {KEY_ESC, 0x1B}, {KEY_LEFTSHIFT, 0xA0}, {KEY_RIGHTSHIFT, 0xA1},
            {KEY_LEFTCTRL, 0xA2}, {KEY_RIGHTCTRL, 0xA3},
            {KEY_LEFTALT, 0xA4}, {KEY_RIGHTALT, 0xA5},
            {KEY_LEFTBRACE, 0xDB}, {KEY_RIGHTBRACE, 0xDD}, {KEY_SLASH, 0xBF}, {KEY_SEMICOLON, 0xBA},
            {0xBF, KEY_SLASH},      // /
            {0xBA, KEY_SEMICOLON},  // ; or :
            {0x31, KEY_1},          // 1
            {0x32, KEY_2},          // 2
            {0x33, KEY_3},          // 3
            {0x34, KEY_4},          // 4
            {0x35, KEY_5},          // 5
            {0x36, KEY_6},          // 6
            {0x37, KEY_7},          // 7
            {0x38, KEY_8},          // 8
            {0x39, KEY_9},          // 9
            {0x30, KEY_0},          // 0
            {0xBD, KEY_MINUS},      // - or _
            {0xBB, KEY_EQUAL},      // = or +
            {0xDC, KEY_BACKSLASH},  // \ or |
            {0xDE, KEY_APOSTROPHE}, // ' or "
            {0xBC, KEY_COMMA},      // , or <
            {0xBE, KEY_DOT},        // . or >
            {0xC0, KEY_GRAVE},      // ` or ~
            // Navigation keys
            {KEY_HOME, 0x24}, {KEY_END, 0x23}, {KEY_PAGEUP, 0x21}, {KEY_PAGEDOWN, 0x22},
            // Numpad
            {KEY_KP0, 0x60}, {KEY_KP1, 0x61}, {KEY_KP2, 0x62}, {KEY_KP3, 0x63},
            {KEY_KP4, 0x64}, {KEY_KP5, 0x65}, {KEY_KP6, 0x66}, {KEY_KP7, 0x67},
            {KEY_KP8, 0x68}, {KEY_KP9, 0x69},
            {KEY_KPASTERISK, 0x6A}, {KEY_KPPLUS, 0x6B}, {KEY_KPMINUS, 0x6D},
            {KEY_KPDOT, 0x6E}, {KEY_KPSLASH, 0x6F},
            // Lock keys
            {KEY_CAPSLOCK, 0x14}, {KEY_NUMLOCK, 0x90}, {KEY_SCROLLLOCK, 0x91},
            // System keys
            {KEY_SYSRQ, 0x2C}, {KEY_PAUSE, 0x13},
            // Windows/Super key
            {KEY_LEFTMETA, 0x5B}, {KEY_RIGHTMETA, 0x5C},
            // Arrow keys (if not already there)
            {KEY_LEFT, 0x25}, {KEY_UP, 0x26}, {KEY_RIGHT, 0x27}, {KEY_DOWN, 0x28},
            // Backspace, Delete, Insert (if not already there)
            {KEY_BACKSPACE, 0x08}, {KEY_DELETE, 0x2E}, {KEY_INSERT, 0x2D},

        };
        

        if (evdevToVk.count(evdevCode)) {
            return evdevToVk[evdevCode];
        }
        return evdevCode;
    }

    
    Key getCurrentPressedKeyLinux(int timeout_ms) {
        auto startTime = std::chrono::steady_clock::now();
        
        do {
            unsigned int pressedKeyCode = 0;
            bool keyFound = false;
            
            // Check for pressed keys in a scoped lock
            {
                std::lock_guard<std::mutex> lock(m_keyMutex);
                
                // Find the first pressed key
                for (const auto& pair : m_keyStates) {
                    if (pair.second) {
                        pressedKeyCode = pair.first;
                        keyFound = true;
                        break;
                    }
                }
            } // Lock is released here
            
            // If we found a pressed key, wait for it to be released
            if (keyFound) {
                bool released = false;
                while (!released) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    
                    // Check if key is still pressed
                    std::lock_guard<std::mutex> lock(m_keyMutex);
                    auto it = m_keyStates.find(pressedKeyCode);
                    released = (it == m_keyStates.end() || !it->second);
                }
                
                return static_cast<Key>(pressedKeyCode);
            }
            
            if (timeout_ms == 0) {
                break; // No wait, check once
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            if (timeout_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime
                ).count();
                if (elapsed >= timeout_ms) {
                    break;
                }
            }
            
        } while (timeout_ms != 0);
        
        return static_cast<Key>(0); // No key pressed
    }

#endif
};

#ifdef _WIN32
inline CrossInput* CrossInput::s_instance = nullptr;
#endif

#endif // INPCTRL_HPP