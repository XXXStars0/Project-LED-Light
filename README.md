# 🚨 Project LED Light

![Class](https://img.shields.io/badge/Class-INFO_5321-B31B1B)
![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?logo=Raspberry-Pi&logoColor=white)
![Arduino IDE](https://img.shields.io/badge/-Arduino_IDE-00979D?logo=Arduino&logoColor=white)
![C++](https://img.shields.io/badge/-C++-00599C?logo=c%2B%2B&logoColor=white)

A Technical mini-project using Raspberry Pi Pico W and RGB LEDs. This project connects to an API to receive data and provides visual feedback through LED lighting.

## 📁 Project Structure

- `docs/`: Design documents and technical notes.
- `src/`: Arduino/C++ source code for the Pico W.
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

![Circuit Diagram (Unverified)](docs/img_circuit_1.jpeg)

| Component         | Pico W Pin                   | Note           |
| ----------------- | ---------------------------- | -------------- |
| **RGB LED**       | GP13 (R), GP14 (G), GP15 (B) | PWM Support    |
| **Potentiometer** | GP26                         | Analog Input   |
| **Ultrasonic**    | GP28 / Pin 34                |                |
| **Button**        | GP16                         | 100KΩ Resistor |
| **LDR**           | GP27                         |                |

## 📡 API Integration

This project uses the **Trello API** to monitor project changes and provide visual feedback via the RGB LED. (**Pending**)


## 💻 Development Environment

- **Primary IDE**: Arduino IDE (for Pico W Sketch)
- **Secondary IDE**: VS Code (for documentation and API testing)
- **Platform**: Raspberry Pi Pico W 

## Getting Started

1. Install the Raspberry Pi Pico W board support in Arduino IDE.
2. Open the sketch in `src/`.
3. Configure your Wi-Fi credentials.
4. Upload the sketch to the Pico W.