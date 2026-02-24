# 🚨 Project LED Light

![Class](https://img.shields.io/badge/Class-INFO_5321-B31B1B)
![Trello](https://img.shields.io/badge/Trello-%23026AA7.svg?style=flat&logo=trello&logoColor=white)
![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?logo=Raspberry-Pi&logoColor=white)
![Arduino IDE](https://img.shields.io/badge/-Arduino_IDE-00979D?logo=Arduino&logoColor=white)
![C++](https://img.shields.io/badge/-C++-00599C?logo=c%2B%2B&logoColor=white)

A Technical mini-project using Raspberry Pi Pico W and RGB LEDs. This project connects to an API to receive data and provides visual feedback through LED lighting.

## 📁 Project Structure

- `docs/`: Design documents and technical notes.
- `RGB_LED/`: Arduino/C++ source code for the Pico W.
- `Processing_Connect/`: Alt wired internet connection mode using Processing.
- `tests/`: API verification and testing scripts (Python/JS).

## ⚙️ Hardware Components

- Raspberry Pi Pico W
- RGB LED (GP13, GP14, GP15)
- Potentiometer (GP26)
- Ultrasonic Sensor (GP28)
- Button (GP16 + 100KΩ Resistor)
- Photoresistor (GP27)
- Breadboard and jumper wires

## 💡 Pin Mapping Reference

![Circuit Diagram (Corrected with 220Ω Resistors)](docs/img_circuit_2.jpeg)

| Component         | Pico W Pin                   | Note           |
| ----------------- | ---------------------------- | -------------- |
| **RGB LED**       | GP13 (R), GP14 (G), GP15 (B) | PWM Support    |
| **Potentiometer** | GP26                         | Analog Input   |
| **Ultrasonic**    | GP28                         |                |
| **Button**        | GP16                         | 100KΩ Resistor |
| **LDR**           | GP27                         |                |

## 📡 API Integration

This project uses the **[Trello API](https://developer.atlassian.com/cloud/trello/guides/rest-api/api-introduction/)** to monitor project changes and provide visual feedback via the RGB LED.

## 🕹️ Usage & States

### Controls
Once the device is running, interact with the **Potentiometer (Knob)**. Rotating the knob maps its analog value (0-4095) to dynamically scroll through and select different lists from your Trello board. When you switch to a different list, the system detects the change (Edge Detection) and instantly triggers a new data fetch.

### Visual Status Indicators (LED)
The Pico W operates using a dual-core architecture to ensure smooth, non-blocking LED animations (Core 1) even during Wi-Fi / API requests (Core 0):
- ⚪ **BOOTING / LOADING:** Smooth white breathing effect. Occurs during startup, network connection, or when actively querying new data from the Trello server.
- 🔴 **ERROR:** Rapid red double-blink. Indicates Wi-Fi disconnection or API failure.
- **TRACKING:** Solid colors indicating the accumulated "Pressure Score" of the currently tracked list (calculated based on card due dates):
  - 🔵 **Blue:** Idle / Empty list.
  - 🟢 **Green:** Low pressure (distant due dates).
  - 🟡 **Yellow:** Medium pressure.
  - 🔴 **Red:** High pressure (urgent or overdue tasks).
## 💻 Development Environment

- **Primary IDE**: Arduino IDE (for Pico W Sketch)
- **Secondary IDE**: VS Code (for documentation and API testing)
- **Platform**: Raspberry Pi Pico W 

## ⭐ Getting Started

### Hardware Setup (Arduino)
1. Install the Raspberry Pi Pico W board support in Arduino IDE.
2. Open the sketch in `src/`.
3. Configure your Wi-Fi credentials in `secrets.h`.
4. Upload the sketch to the Pico W.

### API Testing Setup (Python)
If need to test the Trello API using the provided Python scripts in `tests/`:

1. Install the required dependencies:
   ```bash
   pip install python-dotenv requests
   ```
2. Create a `.env` file in the root directory and add Trello API credentials:
   ```env
   TRELLO_API_KEY=your_api_key_here
   TRELLO_TOKEN=your_oauth_token_here
   TRELLO_BOARD_ID=your_board_id_here
   ```
3. Run the test script: 
   ```bash
   python tests/trello_api_test.py
   ```

   **💡 Note:** In line 29 of `trello_api_test.py` (`selected_list = trello_lists[0]`), is possible to configure the specific list to track by changing the index (0 represents the first list).

   **Example Output of Testing Script:**
   ```text
   ---> Simulating potentiometer input: Currently tracking list 'To Do'
   
   There are 1 cards in this list. Starting pressure value calculation...
   
      Card: Test 1
       Status: No due date -> Pressure +1
   
   ========================================
   Total pressure score for the current list: 1
   Pico W Pin PWM Output Instructions:
      -> 🔴 GP13 (Red):   5
      -> 🟢 GP14 (Green): 249
      -> 🔵 GP15 (Blue):  0
   ```

### Wired Connection Setup (Processing)
An alternative wired mode is available using Processing to fetch the Trello API and communicate with the Pico W directly via Serial (USB). This allows the computer to handle API requests instead of the Pico W's Wi-Fi.

1. Open `RGB_LED/wifi_config.h` and comment out the line: `// #define USE_WIFI_MODE`.
2. Upload the `RGB_LED.ino` sketch to the Pico W using Arduino IDE.
3. Keep the Pico W connected to your computer via USB.
4. Open the `Processing_Connect/Processing_Connect.pde` sketch in the [Processing IDE](https://processing.org/).
5. Check your Arduino IDE to see which COM port the Pico W is using, and update the `COM_PORT` variable in the Processing sketch accordingly (e.g., `"COM4"`).
6. Run the script in Processing. It will automatically read your API credentials from the root `.env` file, read potentiometer data sent from the Pico W, and send back realtime RGB color values!

## 🛠️ Troubleshooting & Fixes

- **Pico W not reading Potentiometer data:**
  - *Issue:* The board and wiring appeared correct, but no potentiometer readings were being registered.
  - *Fix:* Discovered that the potentiometer was accidentally connected to the `RUN` (reset) pin instead of the designated `GP26` analog input pin. Reconnecting it to `GP26` resolved the issue.

- **Pico W Board Overheating:**
  - *Issue:* The RGB LED caused a short circuit, resulting in the Pico W board overheating rapidly when powered on.
  - *Fix:* Added **220Ω resistors** to the RGB LED circuits to limit the current, effectively preventing the short circuit and heat issues.
