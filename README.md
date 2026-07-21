# nearPlane ADSB Tracker for M5StickC Plus 2

![nearPlane](https://i.imgur.com/YsXTTUf.jpeg)


So, I was inspired by [this](https://www.reddit.com/r/ADSB/comments/1nbsb3c/inspired_by_ufil1983s_nearest_aircraft_display_i/) reddit post, and he was inspired by [this one](https://www.reddit.com/r/ADSB/comments/1nb56ld/nearest_aircraft_display/). But hey, in the end, I created this nearPlane.

It transforms your `M5StickC Plus 2 thing` which dying in your stuff drawer (I know you have one, everybody does, no worries) into a portable, real-time aircraft tracker. No need to check Flightradar or something else. IDK if you're that much of an avgeek, but it's perfect for aviation enthusiasts, curious minds, and anyone who's ever looked up at the sky and wondered, "What plane is that?"

The tracker fetches data from **adsb.lol**, a free, open-source community-driven project that collects and provides real-time `ADS-B` (Automatic Dependent Surveillance-Broadcast) data from aircraft around the world. This allows you to see detailed flight information without needing your own expensive radio hardware.

## Features

*   **Real-Time Tracking**: Displays the closest aircraft's flight data, updated every few seconds.
*   **Reliable Flight Route Information**: Displays the departure and arrival airports (IATA codes) for most commercial flights by querying the adsb.lol API directly.
*   **Multi-Page Interface**: Cycle through 6 different pages of detailed telemetry, including altitude, speed, heading, squawk code, and more.
*   **Emergency Alerts**: The display highlights aircraft with emergency squawk codes (7500, 7600, 7700).
*   **Audible Alerts**: Plays a distinct tone when a new aircraft is detected.
*   **Easy Web-Based Configuration**: An initial setup mode allows you to easily connect the device to your Wi-Fi and set your location.
*   **Factory Reset**: An easy hardware-button-based reset to clear settings.

## Install

It's listing on m5burner now! 🎉 Search as "nearPlane" and burn directly your device or 👇🏻

## Do you want one?

### Prerequisites

*   Buy a M5StickC Plus 2 (ask your wife before order one)
*   Install [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode), or install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
*   A USB-C cable for programming and charging

### Installation Guide

Follow these steps to compile and upload the tracker firmware to your M5StickC Plus 2.

#### 1. Install USB Driver

The M5StickC Plus 2 uses a CH9102 chip for USB communication. You will need to install the driver for your operating system to ensure your computer can communicate with the device.

*   **Windows**: [Download and install the CH9102 driver](https://docs.m5stack.com/en/core/M5StickC%20PLUS2).
*   **MacOS**: MacOS should detect the device automatically. If not, the driver can be found on the same M5Stack documentation page.

#### 2. Build and Upload with PlatformIO

PlatformIO installs the ESP32 toolchain and the pinned `M5Unified` and `ArduinoJson` dependencies automatically.

1.  Clone this repository and open its root directory in Visual Studio Code.
2.  Connect the M5StickC Plus 2 with a USB-C cable.
3.  Use the PlatformIO **Upload** task, or run these commands from the repository root:
    ```sh
    pio run
    pio run --target upload
    ```

PlatformIO normally detects the serial port automatically. If more than one compatible device is attached, pass it explicitly:

```sh
pio run --target upload --upload-port /dev/cu.wchusbserial...
```

To view firmware logs, use the PlatformIO **Monitor** task or run:

```sh
pio device monitor
```

The PlatformIO environment uses the `m5stick-c` board definition with the M5StickC Plus2 PSRAM flags recommended by M5Stack, because PlatformIO does not currently provide a separate Plus2 board ID. It overrides the legacy board definition with the Plus2's 8 MB flash layout.

### Device Configuration

After successfully burning the firmware, the device will boot into "SETUP MODE" for the first time.

1.  On your smartphone or computer, search for Wi-Fi networks and connect to the one named **`nearPlane-ADSB-Tracker-Setup`**.
2.  Once connected, open a web browser and navigate to `http://192.168.4.1`.
3.  You will see a configuration page. Enter the following details:
    *   **WiFi Network (SSID)**: The name of your local Wi-Fi network.
    *   **WiFi Password**: The password for your Wi-Fi.
    *   **Your Latitude**: Your current latitude (e.g., `41.015137`).
    *   **Your Longitude**: Your current longitude (e.g., `28.979530`).
    *   **Scan Radius (km)**: The radius around your location to scan for aircraft (e.g., `50`).
4.  Click **"Save & Reboot"**.

The M5StickC Plus 2 will save your settings and restart. It will then automatically connect to your specified Wi-Fi network and begin tracking aircraft.

### Usage

*   **Change Page**: Short press the large button on the front (BtnA) to cycle through the different information pages.
*   **Reset Settings**: To clear all saved settings and re-enter "SETUP MODE", press and hold the right-side button (BtnB) for 5 seconds. A confirmation screen will appear during the hold.

## Changelog

**v1.1 - Sep 10,2025**

*   **Flight Route Fetching**:
    *   Implemented the correct and most reliable method by sending a `POST` request to the `https://api.adsb.lol/api/0/routeset` endpoint with the current aircraft's callsign.
    *   This provides significantly more accurate departure and arrival airport data directly from the adsb.lol ecosystem, eliminating the need for external API keys and solving previous issues with empty or incorrect route information.


**v1.0 - Sep 9,2025**

*   **Initial Release**:
    *   Bare/minimum working copy

## Contributing

Contributions are welcome! If you'd like to help improve the project, please follow these steps:

1.  **Fork** the repository on GitHub.
2.  Create a new **branch** for your feature or bug fix.
3.  Make your changes and commit them with clear, descriptive messages.
4.  Push your branch to your forked repository.
5.  Create a **Pull Request (PR)** back to the main repository, explaining the changes you have made.

## License

This project is open-source. Please feel free to use, modify, and distribute the code.
