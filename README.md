# Elecsy — ESP32-C3 AI Desktop Companion

**Elecsy** is an interactive, voice-enabled desktop robot companion powered by an **ESP32-C3 Super Mini**. Featuring an expressive animated OLED face, capacitive touch controls, high-speed speech-to-text via **Groq Whisper**, and conversational intelligence powered by **Google Gemini**, Elecsy acts as a smart desk assistant capable of answering questions, displaying live weather, tracking timers, showing UPI payment QR codes, and more.

---

## ✨ Key Features

* **Expressive Animated OLED Face:** Renders dynamic facial animations on a 128×64 SSD1306 display—including idle eye movements, random blinking, happy expressions, listening indicators, and thinking animations.
* **Voice-to-AI Assistant:** Uses an **INMP441 I2S MEMS microphone** paired with **Groq (`whisper-large-v3-turbo`)** for rapid speech-to-text transcription and **Google Gemini (`gemini-3.6-flash`)** for intelligent responses.
* **Captive Portal Wi-Fi Manager:** Automatically falls back to Access Point mode (`Elecsy-Setup`) with a built-in DNS captive portal and web UI if saved Wi-Fi credentials fail or aren't configured, storing credentials permanently in NVS Flash.
* **Built-in Voice & Touch Utilities:**
  * **Live Weather:** Fetches real-time temperature, humidity, wind speed, and weather icons via the Open-Meteo API.
  * **Interactive Countdown Timer:** Voice-activated timer with a circular progress ring, percentage readout, and touch-to-pause/adjust controls.
  * **UPI Payment QR Code:** Generates and renders a camera-safe, high-contrast UPI payment QR code directly on the OLED screen.
  * **NTP Real-Time Clock:** Displays synced IST (`UTC+5:30`) time and date via double-tap or voice command.
  * **Social Badges:** Displays custom bitmaps for Instagram and YouTube handles.

---

## 🛠️ Hardware Requirements

| Component | Specification / Notes |
| :--- | :--- |
| **Microcontroller** | ESP32-C3 Super Mini |
| **Display** | 0.96" I2C OLED Display (128×64, SSD1306, Address `0x3C`) |
| **Microphone** | INMP441 Omnidirectional I2S MEMS Microphone |
| **Touch Sensor** | TTP223 Capacitive Touch Sensor Module |

---

## 🔌 Pinout & Wiring Guide

Connect the peripherals to the **ESP32-C3 Super Mini** according to the pin mapping defined in the firmware:

### 1. SSD1306 OLED Display (I2C)
| OLED Pin | ESP32-C3 Pin | Description |
| :--- | :--- | :--- |
| `VCC` | `3.3V` | Power Supply |
| `GND` | `GND` | Ground |
| `SDA` | `GPIO 8` | I2C Data |
| `SCL` | `GPIO 9` | I2C Clock |

### 2. INMP441 Microphone (I2S)
| INMP441 Pin | ESP32-C3 Pin | Description |
| :--- | :--- | :--- |
| `VDD` | `3.3V` | Power Supply |
| `GND` | `GND` | Ground |
| `L/R` | `GND` | Left Channel Select |
| `SCK` | `GPIO 7` | Serial Clock (BCLK) |
| `WS` | `GPIO 5` | Word Select (LRCLK) |
| `SD` | `GPIO 6` | Serial Data (DOUT) |

### 3. TTP223 Touch Sensor
| TTP223 Pin | ESP32-C3 Pin | Description |
| :--- | :--- | :--- |
| `VCC` | `3.3V` | Power Supply |
| `GND` | `GND` | Ground |
| `I/O` | `GPIO 3` | Digital Touch Signal |

---

## 📚 Dependencies & Libraries

Install the following libraries via the Arduino Library Manager or your PlatformIO environment:

1. **Adafruit GFX Library** (`Adafruit_GFX.h`)
2. **Adafruit SSD1306** (`Adafruit_SSD1306.h`)
3. **ArduinoJson** (`ArduinoJson.h` — v7+ recommended)
4. **Groq STT Engine** (`groq_stt.h` — custom/integrated I2S Groq Whisper wrapper)
5. **ESP32 Arduino Core** (provides built-in `WiFi.h`, `WebServer.h`, `DNSServer.h`, `Preferences.h`, `HTTPClient.h`, and `qrcode.h`)

---

## ⚙️ Configuration & Setup

Before flashing the firmware to your ESP32-C3, update the API keys and personal details near the top of the sketch:

```cpp
// API Configurations
const char* apiKey       = "YOUR_GEMINI_API_KEY"; 
const char* GROQ_API_KEY = "YOUR_GROQ_API_KEY";

// UPI Payment URI (for QR Code generation)
const char* UPI_URI = "upi://pay?pa=your.upi@bank&pn=Your%20Name&cu=INR";

// Weather Coordinates (Default: New Delhi)
const char* WEATHER_URL = "http://api.open-meteo.com/v1/forecast?latitude=28.6139&longitude=77.2090&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=Asia%2FKolkata";
```

> **Security Note:** Avoid committing real API keys to public repositories. Replace placeholder keys with your own credentials locally before compiling.

### Wi-Fi Provisioning (Captive Portal)
1. On first boot (or if the saved Wi-Fi network is unreachable within 15 seconds), Elecsy creates a Wi-Fi hotspot named **`Elecsy-Setup`**.
2. Connect to **`Elecsy-Setup`** using your smartphone or PC.
3. A captive portal page will open automatically (or navigate to `http://192.168.4.1`).
4. Select your 2.4 GHz Wi-Fi network from the dropdown, enter the password, and click **Save & Connect**. The device will store the credentials in NVS memory and reboot.

---

## 🎮 Controls & Voice Commands

### Touch Gestures
* **Single Tap (Idle Screen):** Activates the microphone for **3 seconds** to listen for voice commands or AI prompts.
* **Double Tap (Idle Screen):** Displays the current IST time and date overlay.
* **Single Tap (Timer Active):** Starts, pauses, or resets a finished countdown timer.
* **Long Press ≥ 800ms (Timer Paused/Ready):** Increments the timer duration by 1 minute (cycles 1–60 minutes).

### Supported Voice Commands
| Command Phrase | Action |
| :--- | :--- |
| *"Your name"* | Elecsy introduces itself on screen. |
| *"What is the time"* / *"What's the time"* | Displays the synced IST clock and date. |
| *"Weather"* | Fetches and displays live temperature, conditions, humidity, and wind speed. |
| *"Set a timer for [1 / 2 / 5 / 10] minutes"* | Launches the interactive countdown timer UI. |
| *"QR Code"* / *"Payment Details"* / *"UPI ID"* | Renders your scannable UPI payment QR code with camera-safe OLED contrast. |
| *"Instagram ID"* / *"Insta ID"* | Displays the custom Instagram handle bitmap. |
| *"YouTube"* | Displays the custom YouTube channel bitmap. |
| *"WiFi setup"* / *"Reset WiFi"* | Re-opens the `Elecsy-Setup` Wi-Fi configuration portal. |
| *Any other question or prompt* | Forwards your spoken prompt to **Google Gemini** and displays the response. |
