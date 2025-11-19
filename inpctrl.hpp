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
        LeftBracket = 0xDB, RightBracket = 0xDD
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
            {0x5A, "Z"}, {0x20, "Space"}, {0x0D, "Enter"}, {0x09, "Tab"},
            {0x1B, "Escape"}, {0x70, "F1"}, {0x71, "F2"}, {0x72, "F3"},
            {0x73, "F4"}, {0x74, "F5"}, {0x75, "F6"}, {0x76, "F7"},
            {0x77, "F8"}, {0x78, "F9"}, {0x79, "F10"}, {0x7A, "F11"},
            {0x7B, "F12"}, {0xDB, "["}, {0xDD, "]"}, {0x01, "LMB"},
            {0x02, "RMB"}, {0x04, "MMB"}, {0xA0, "LShift"}, {0xA2, "LCtrl"}
        };
        
        if (names.count(code)) {
            return names[code];
        }
        return "Unknown";
    }

private:
    std::unordered_map<unsigned int, bool> m_keyStates;
    std::mutex m_keyMutex;
    std::thread m_listenerThread;
    std::atomic<bool> m_running;
    bool m_initialized;

#ifdef _WIN32
    // ==================== WINDOWS IMPLEMENTATION ====================
    HHOOK m_hookHandle;
    
    static CrossInput* s_instance;
    
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
        input.ki.wVk = vkCode;
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(INPUT));
    }
    
    void releaseKeyWindows(unsigned int vkCode) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vkCode;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
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

#else
    // ==================== LINUX IMPLEMENTATION ====================
    int m_uinputFd;
    std::vector<int> m_inputFds;
    
    bool initLinux() {
        // Initialize uinput for output
        m_uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (m_uinputFd < 0) {
            std::cerr << "Failed to open /dev/uinput. Run with sudo!" << std::endl;
            return false;
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
                if (n == sizeof(ev) && ev.type == EV_KEY) {
                    unsigned int winCode = fromEvdevCode(ev.code);
                    std::lock_guard<std::mutex> lock(m_keyMutex);
                    m_keyStates[winCode] = (ev.value != 0);
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
            {0xDB, KEY_LEFTBRACE}, {0xDD, KEY_RIGHTBRACE}
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
            {KEY_LEFTBRACE, 0xDB}, {KEY_RIGHTBRACE, 0xDD}
        };
        
        if (evdevToVk.count(evdevCode)) {
            return evdevToVk[evdevCode];
        }
        return evdevCode;
    }
#endif
};

#ifdef _WIN32
CrossInput* CrossInput::s_instance = nullptr;
#endif

#endif // CROSS_INPUT_HPP