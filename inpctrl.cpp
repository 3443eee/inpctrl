#include "inpctrl.hpp"
#include <iostream>
#include <chrono>
#include <thread>

void printMenu() {
    std::cout << "\n========================================\n";
    std::cout << "     inpctrl.hpp Test Program\n";
    std::cout << "========================================\n";
    std::cout << "Press keys to test input detection:\n";
    std::cout << "  F5  - Test single key press\n";
    std::cout << "  F6  - Test key hold and release\n";
    std::cout << "  F7  - Test mouse movement\n";
    std::cout << "  F8  - Test rapid key presses\n";
    std::cout << "  F9  - Test multiple keys combo\n";
    std::cout << "  ESC - Exit program\n";
    std::cout << "========================================\n\n";
}

void testSingleKeyPress(CrossInput& input) {
    std::cout << "[F5 TEST] Pressing Space key in 2 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Pressing Space key... ";
    input.pressKey(CrossInput::Key::Space, 50);
    std::cout << "Done!\n";
}

void testHoldRelease(CrossInput& input) {
    std::cout << "[F6 TEST] Holding W key for 3 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "Holding W... ";
    input.holdKey(CrossInput::Key::W);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    input.releaseKey(CrossInput::Key::W);
    std::cout << "Released!\n";
}

void testMouseMovement(CrossInput& input) {
    std::cout << "[F7 TEST] Moving mouse in a square pattern...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    int distance = 100;
    int steps = 20;
    int stepSize = distance / steps;
    
    std::cout << "Moving right... ";
    for (int i = 0; i < steps; i++) {
        input.moveMouse(stepSize, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    std::cout << "down... ";
    for (int i = 0; i < steps; i++) {
        input.moveMouse(0, stepSize);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    std::cout << "left... ";
    for (int i = 0; i < steps; i++) {
        input.moveMouse(-stepSize, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    std::cout << "up... Done!\n";
    for (int i = 0; i < steps; i++) {
        input.moveMouse(0, -stepSize);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void testRapidKeyPresses(CrossInput& input) {
    std::cout << "[F8 TEST] Rapid fire key presses (X key, 10 times)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    for (int i = 0; i < 10; i++) {
        std::cout << "Press " << (i + 1) << "/10... ";
        input.pressKey(CrossInput::Key::X, 30);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Done!\n";
}

void testMultipleKeys(CrossInput& input) {
    std::cout << "[F9 TEST] Testing key combination (Shift + W)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "Holding LShift... ";
    input.holdKey(CrossInput::Key::LShift);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "Pressing W 5 times... ";
    for (int i = 0; i < 5; i++) {
        input.pressKey(CrossInput::Key::W, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    std::cout << "Releasing LShift... Done!\n";
    input.releaseKey(CrossInput::Key::LShift);
}

void monitorKeys(CrossInput& input) {
    std::cout << "\nMonitoring key states (press keys to see detection)...\n";
    
    // Keys to monitor
    CrossInput::Key monitoredKeys[] = {
        CrossInput::Key::W, CrossInput::Key::A, CrossInput::Key::S, CrossInput::Key::D,
        CrossInput::Key::Space, CrossInput::Key::LShift, CrossInput::Key::LCtrl
    };
    
    std::unordered_map<CrossInput::Key, bool> previousStates;
    
    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = startTime;
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        
        // Check for ESC key
        if (input.isKeyPressed(CrossInput::Key::Escape)) {
            std::cout << "\nESC pressed - exiting monitor mode...\n";
            break;
        }
        
        // Check for test trigger keys
        if (input.isKeyPressed(CrossInput::Key::F5) && !previousStates[CrossInput::Key::F5]) {
            testSingleKeyPress(input);
        }
        if (input.isKeyPressed(CrossInput::Key::F6) && !previousStates[CrossInput::Key::F6]) {
            testHoldRelease(input);
        }
        if (input.isKeyPressed(CrossInput::Key::F7) && !previousStates[CrossInput::Key::F7]) {
            testMouseMovement(input);
        }
        if (input.isKeyPressed(CrossInput::Key::F8) && !previousStates[CrossInput::Key::F8]) {
            testRapidKeyPresses(input);
        }
        if (input.isKeyPressed(CrossInput::Key::F9) && !previousStates[CrossInput::Key::F9]) {
            testMultipleKeys(input);
        }
        
        // Update previous states for trigger keys
        previousStates[CrossInput::Key::F5] = input.isKeyPressed(CrossInput::Key::F5);
        previousStates[CrossInput::Key::F6] = input.isKeyPressed(CrossInput::Key::F6);
        previousStates[CrossInput::Key::F7] = input.isKeyPressed(CrossInput::Key::F7);
        previousStates[CrossInput::Key::F8] = input.isKeyPressed(CrossInput::Key::F8);
        previousStates[CrossInput::Key::F9] = input.isKeyPressed(CrossInput::Key::F9);
        
        // Print key states every 500ms
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() >= 500) {
            bool anyPressed = false;
            std::string pressedKeys = "Currently pressed: ";
            
            for (auto key : monitoredKeys) {
                if (input.isKeyPressed(key)) {
                    pressedKeys += input.getKeyName(key) + " ";
                    anyPressed = true;
                }
            }
            
            if (anyPressed) {
                std::cout << pressedKeys << "\n";
            }
            
            lastUpdate = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    std::cout << "Initializing inpctrl.hpp...\n";
    
    CrossInput input;
    
    if (!input.init()) {
        std::cerr << "Failed to initialize input system!\n";
#ifndef _WIN32
        std::cerr << "On Linux, make sure to run with sudo!\n";
#endif
        return 1;
    }
    
    std::cout << "Input system initialized successfully!\n";
    
    printMenu();
    
    // Give user time to read menu
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Start monitoring
    monitorKeys(input);
    
    std::cout << "\nCleaning up...\n";
    input.cleanup();
    
    std::cout << "Test program finished. Goodbye!\n";
    return 0;
}