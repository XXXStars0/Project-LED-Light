# 🚨 RGB LED Mini Project - Ambient Trello Aura

![INFO](https://img.shields.io/badge/INFO-5321-B31B1B)
![Trello](https://img.shields.io/badge/Trello-%23026AA7.svg?style=flat&logo=trello&logoColor=white)
![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?logo=Raspberry-Pi&logoColor=white)
![Arduino IDE](https://img.shields.io/badge/-Arduino_IDE-00979D?logo=Arduino&logoColor=white)
![C++](https://img.shields.io/badge/-C++-00599C?logo=c%2B%2B&logoColor=white)

<div align="center">
  <img src="img/raspberry-pi-logo.png-128.png" alt="Raspberry Pi Logo" />
</div>

**Ambient Trello Aura**: A desktop companion that translates Trello task deadlines into dynamic RGB lighting, helping users visualize workload pressure and manage digital anxiety. (Built with Raspberry Pi Pico W and RGB LEDs)

## 💡 Idea
The inspiration for this project stemmed from extensive experience utilizing project management tools like Trello, as well as prior development of Discord bots for automated notifications includes Trello and Github. The primary goal was to bridge the gap between digital workflows and the physical environment—creating a desk accessory that provides real-time project notifications without relying on a browser window or chat interface.

## 📁 Project Structure

- `img/`: Circuit diagrams and design images.
- `RGB_LED/`: Arduino/C++ source code for the Pico W.
- `Processing_Connect/`: Alt wired internet connection mode using Processing.
- `tests/`: API verification and testing scripts (Python).

## ⚙️ Hardware Components

- Raspberry Pi Pico W
- RGB LED (Common Cathode)
- Potentiometer 
- Push Button (with 10KΩ Resistor)
- 3× 220Ω Resistors (for RGB LED current limiting)
- Breadboard and jumper wires

### Initial Design vs. Final Implementation
Due to project time constraints and to ensure code stability, two initially planned components were excluded from the final build to focus effort on software reliability:
1. **Photoresistor (LDR)**: Intended for adaptive LED brightness. Tests showed marginal visual improvements that did not justify the added code complexity.
2. **Ultrasonic Sensor**: Intended to detect user presence (automatic sleep when no one is at the desk). The sleep functionality is now reliably handled manually via the Push Button.

## 💡 Pin Mapping Reference

![Circuit Schematic](img/schematic.jpg)

![Wokwi Layout](img/img_circuit_4.png)

| Component         | Pico W Pin                   | Note           |
| ----------------- | ---------------------------- | -------------- |
| **RGB LED**       | GP13 (R), GP14 (G), GP15 (B) | PWM Support    |
| **Potentiometer** | GP27                         | Analog Input   |
| **Button**        | GP16                         | Pull-down Res. |

### Previous Design Iterations
<details>
<summary>Click to view previous circuit versions</summary>

![Circuit Diagram V3 (Simplified)](img/img_circuit_3.jpeg)
*Version 3 — Simplified breadboard layout, removed unused sensors*

![Circuit Diagram V2 (Corrected with 220Ω Resistors)](img/img_circuit_2.jpeg)
*Version 2 — Fixed original circuit diagram, added 220Ω resistors for RGB LED*
</details>

## 📡 API Integration

This project uses the **[Trello API](https://developer.atlassian.com/cloud/trello/guides/rest-api/api-introduction/)** to monitor project changes and provide visual feedback via the RGB LED.

## 💻 Development Environment

- **Primary IDE**: Arduino IDE (for Pico W Sketch)
- **Secondary IDE**: VS Code (for documentation and API testing)
- **Platform**: Raspberry Pi Pico W 

## ⭐ Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/XXXStars0/Project-LED-Light.git
cd Project-LED-Light
```

### 2. Hardware Assembly
Assemble the components on a breadboard according to the **[Pin Mapping & Circuit Diagram](#-pin-mapping-reference)** above. Ensure the 220Ω current-limiting resistors are correctly placed for the RGB LED.

### 3. Flash the Firmware
1. Install the **Raspberry Pi Pico/RP2040** board support in the [Arduino IDE](https://www.arduino.cc/en/software/).
2. Open `RGB_LED/RGB_LED.ino`.
3. Select the correct board (**Raspberry Pi Pico W**) and COM port.
4. Upload the sketch to the Pico W.

> The firmware supports both Wi-Fi and Wired modes. The mode is determined by a single config flag — see the sections below.

### 4. Create API Credentials (`.env`)
Create a `.env` file in the **project root directory** with Trello API credentials. This file is required for both Wi-Fi and Wired modes:
```env
TRELLO_API_KEY=your_api_key_here
TRELLO_TOKEN=your_oauth_token_here
TRELLO_BOARD_ID=your_board_id_here
```

### 5a. Wi-Fi Mode Configuration
In this mode the Pico W connects to the internet directly and handles all API requests on-device.

1. Open `RGB_LED/wifi_config.h` and **uncomment** the line:
   ```cpp
   #define USE_WIFI_MODE
   ```
2. Open `RGB_LED/keys.h` (you can copy and rename the provided `RGB_LED/keys_template.h` reference file) and fill in Wi-Fi credentials and Trello API keys.
3. Re-upload the sketch.

### 5b. Wired Mode Configuration (Processing)
An alternative mode where a computer handles API requests and communicates with the Pico W via USB Serial.

1. Ensure `RGB_LED/wifi_config.h` has the Wi-Fi flag **commented out** (default):
   ```cpp
   // #define USE_WIFI_MODE
   ```
2. Keep the Pico W connected to the computer via USB.
3. Open `Processing_Connect/Processing_Connect.pde` in the [Processing IDE](https://processing.org/).
4. Update the `COM_PORT` variable to match the Pico W's serial port (e.g., `"COM4"`).
5. Run the Processing sketch. It will automatically read credentials from the root `.env` file, receive potentiometer data from the Pico W, and send back real-time RGB color values.

### API Testing (Optional)
<details>
<summary>Click to expand Python API testing instructions</summary>

To verify Trello API connectivity independently using the provided Python script in `tests/`:

1. Install dependencies:
   ```bash
   pip install python-dotenv requests
   ```
2. Run the test script:
   ```bash
   python tests/trello_api_test.py
   ```

**💡 Note:** In line 29 of `trello_api_test.py` (`selected_list = trello_lists[0]`), the tracked list can be changed by modifying the index.

**Example Output:**
```text
---> Simulating potentiometer input: Currently tracking list 'To Do'

There are 1 cards in this list. Starting pressure value calculation...

   Card: Test 1
    Status: No due date -> Pressure +1

========================================
Total pressure score for the current list: 1
Pico W Pin PWM Output Instructions:
   -> GP13 (Red):   5
   -> GP14 (Green): 249
   -> GP15 (Blue):  0
```
</details>

## 🧰 Usage Instructions

### Controls
- **Button (GP16):**
  - **Short press (< 300ms):** Force refresh the current list data.
  - **Long press (≥ 300ms):** Toggle sleep mode (LEDs turned off).
- **Potentiometer (Knob):** Rotate to scroll through Trello lists. The system detects index changes (Edge Detection) and instantly triggers a data fetch. 
- **Wake:** When in Sleep mode, either pressing the button or rotating the potentiometer will immediately awaken the device and resume tracking.

### Visual Status Indicators (LED)
The Pico W uses dual-core architecture for non-blocking LED animations (Core 1) during API requests (Core 0):
- ⚪ **BOOTING / LOADING:** White breathing effect. Startup or fetching new data.
- 🔴 **ERROR:** Rapid red double-blink. Wi-Fi disconnection or API failure.
- ⚫ **SLEEP:** LEDs off. Power-saving mode.
- **TRACKING:** Solid color based on accumulated "Pressure Score" (card due dates):
  - 🔵 **Blue:** Idle / Empty list.
  - 🟢 **Green:** Low pressure.
  - 🟡 **Yellow:** Medium pressure.
  - 🔴 **Red:** High pressure (urgent/overdue).

## 🛠️ Troubleshooting & Fixes

- **Wired Mode Sleep Failure When Disconnected:**
  - *Issue:* In early iterations, the button logic simply sent sleep commands to the Processing sketch. If the computer was disconnected, the Pico W failed to enter sleep mode.
  - *Fix:* Shifted the sleep state management directly into the Pico W's local loop logic, ensuring independent operation regardless of the Serial connection status.

- **Pico W Not Reading Potentiometer Data:**
  - *Issue:* The board and wiring appeared correct, but no potentiometer readings were registered.
  - *Fix:* Discovered the potentiometer was mistakenly connected to the `RUN` (reset) pin instead of the designated `GP26` analog input pin. Reconnecting resolved the issue.

- **Pico W Board Overheating:**
  - *Issue:* The RGB LED caused a short circuit, resulting in rapid overheating of the Pico W board.
  - *Fix:* Added **220Ω resistors** to the RGB LED circuits to limit current draw, successfully preventing short circuits and excessive heat.

- **USB Disconnect at Low Potentiometer Values (GP26):**
  - *Issue:* When the potentiometer was connected to `GP26` and rotated to near-minimum values, the USB Serial connection between the Pico W and the computer would drop unexpectedly.
  - *Fix:* Relocated the potentiometer signal wire from `GP26` to `GP27`. The exact root cause on `GP26` remains unclear, but the issue has not reoccurred on `GP27`.
