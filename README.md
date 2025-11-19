# inpctrl

**inpctrl** is a simple, **cross-platform, header-only C++ library** to handle **keyboard and mouse input** for macros and automation tools. It supports **Windows** and **Linux** (macOS is not supported).

---

## Features

- Check if a **key is currently pressed**
- **Press, hold, and release** keys programmatically
- **Move the mouse** relative to its current position
- Map between **human-readable key names** and system key codes
- Thread-safe key state tracking
- Pure **header-only**, no compilation required

---

## How It Works

### Windows
- Uses **GetAsyncKeyState** to detect key states
- Installs a **low-level keyboard hook** for accurate key tracking
- Sends input events via **SendInput** for simulating key presses/releases
- Mouse movement is simulated with **SendInput** relative motion

### Linux
- Reads `/dev/input/event*` devices for real-time key states
- Sends synthetic key/mouse events via **uinput**
- Maps Windows virtual key codes to **evdev key codes** for consistency
- Uses a dedicated thread to listen for input events and update key states

---

## Installation

Simply **include the header** in your project:

```cpp
#include "inpctrl.hpp"
```

No compilation or linking is required.

---

## Usage Example

```cpp
#include "inpctrl.hpp"
#include <iostream>

int main() {
    inpctrl::CrossInput input;

    if (!input.init()) {
        std::cerr << "Failed to initialize input system!" << std::endl;
        return 1;
    }

    // Press and release the 'A' key
    input.pressKey(inpctrl::CrossInput::Key::A);

    // Move mouse 100 pixels right and 50 pixels down
    input.moveMouse(100, 50);

    input.cleanup();
    return 0;
}
```

You can also **hold and release keys** manually:

```cpp
input.holdKey(inpctrl::CrossInput::Key::Space);
// do something while holding Space
input.releaseKey(inpctrl::CrossInput::Key::Space);
```

---

## Functions

### Windows-only helpers
- `bool initWindows()`  
  Initializes Windows input system with hook and async key tracking.

- `void cleanupWindows()`  
  Cleans up hook and input resources.

- `void holdKeyWindows(unsigned int vkCode)`  
  Simulates pressing a key.

- `void releaseKeyWindows(unsigned int vkCode)`  
  Simulates releasing a key.

- `void moveMouseWindows(int dx, int dy)`  
  Moves the mouse relative to current position.

### Linux-only helpers
- `bool initLinux()`  
  Initializes Linux input system via `/dev/input` and `uinput`.

- `void cleanupLinux()`  
  Cleans up file descriptors and virtual device.

- `void holdKeyLinux(unsigned int evdevCode)`  
  Simulates pressing a key.

- `void releaseKeyLinux(unsigned int evdevCode)`  
  Simulates releasing a key.

- `void moveMouseLinux(int dx, int dy)`  
  Moves the mouse relative to current position.

- `unsigned int toEvdevCode(unsigned int vkCode)`  
  Converts Windows virtual key code to Linux evdev code.

- `unsigned int fromEvdevCode(unsigned int evdevCode)`  
  Converts Linux evdev code back to Windows virtual key code.

### Cross-platform functions
- `bool init()`  
  Initializes the input system (Windows or Linux).

- `void cleanup()`  
  Cleans up resources.

- `bool isKeyPressed(Key key)`  
  Returns true if the key is currently pressed.

- `void holdKey(Key key)`  
  Press and hold a key.

- `void releaseKey(Key key)`  
  Release a key.

- `void pressKey(Key key, int delayMs = 50)`  
  Press and release a key with optional delay.

- `void moveMouse(int dx, int dy)`  
  Move the mouse relative to its current position.

- `std::string getKeyName(Key key)`  
  Returns a human-readable name for a key.

---

## License

MIT License. See `LICENSE` file.

---

## Credits
* @quuuut on github.
* https://github.com/Spencer0187/Spencer-Macro-Utilities for the examples.
