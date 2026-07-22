# nearPlane ADSB Tracker for M5Stack Core2 and M5StickC Plus 2

![nearPlane](https://i.imgur.com/YsXTTUf.jpeg)


So, I was inspired by [this](https://www.reddit.com/r/ADSB/comments/1nbsb3c/inspired_by_ufil1983s_nearest_aircraft_display_i/) reddit post, and he was inspired by [this one](https://www.reddit.com/r/ADSB/comments/1nb56ld/nearest_aircraft_display/). But hey, in the end, I created this nearPlane.

It transforms an M5Stack Core2 or M5StickC Plus 2 into a portable, real-time aircraft tracker. No need to check Flightradar or something else. IDK if you're that much of an avgeek, but it's perfect for aviation enthusiasts, curious minds, and anyone who's ever looked up at the sky and wondered, "What plane is that?"

The tracker fetches data from **adsb.lol**, a free, open-source community-driven project that collects and provides real-time `ADS-B` (Automatic Dependent Surveillance-Broadcast) data from aircraft around the world. This allows you to see detailed flight information without needing your own expensive radio hardware.

## Features

*   **Real-Time Tracking**: Displays the closest aircraft's flight data, updated every few seconds.
*   **Core2 Flight Overview**: Displays a large airline logo, passenger-facing flight number, aircraft type, and origin/destination airport codes and cities.
*   **Reliable Flight Route Information**: Displays route and airport location data for most commercial flights by querying the adsb.lol API directly.
*   **Offline Airline Branding**: Includes logos for 55 major passenger and cargo airlines, with a code-based fallback for unknown operators.
*   **Multi-Page Interface**: The Core2 overview is followed by 6 pages of detailed telemetry, including altitude, speed, heading, squawk code, and more.
*   **Emergency Alerts**: The display highlights aircraft with emergency squawk codes (7500, 7600, 7700).
*   **Audible Alerts**: Plays a soft two-note chime when a new aircraft is detected.
*   **Easy Web-Based Configuration**: An initial setup mode allows you to easily connect the device to your Wi-Fi and set your location.
*   **Factory Reset**: An easy hardware-button-based reset to clear settings.

## Install

The original M5StickC Plus 2 firmware is listed on M5Burner. For M5Stack Core2, build and upload the Core2 environment with PlatformIO as described below.

## Do you want one?

### Prerequisites

*   An M5Stack Core2 or M5StickC Plus 2
*   Install [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode), or install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
*   A USB-C cable for programming and charging

### Installation Guide

Follow these steps to compile and upload the tracker firmware. M5Stack Core2 is the default PlatformIO target.

#### 1. Install USB Driver

Core2 units use either a CP2104 or CH9102F USB communication chip, while the M5StickC Plus 2 uses CH9102. Install the appropriate driver if your operating system does not detect the device automatically.

*   **M5Stack Core2 drivers**: [Core2 documentation](https://docs.m5stack.com/en/core/Core2)
*   **M5StickC Plus 2 drivers**: [M5StickC Plus 2 documentation](https://docs.m5stack.com/en/core/M5StickC%20PLUS2)

#### 2. Build and Upload with PlatformIO

PlatformIO installs the ESP32 toolchain and the pinned `M5Unified` and `ArduinoJson` dependencies automatically.

1.  Clone this repository and open its root directory in Visual Studio Code.
2.  Connect the target device with a USB-C cable.
3.  Use the PlatformIO **Upload** task, or run these commands from the repository root:
    ```sh
    pio run
    pio run --target upload
    ```

The default commands build and upload the M5Stack Core2 environment. The targets can also be selected explicitly:

```sh
# M5Stack Core2
pio run -e m5stack-core2
pio run -e m5stack-core2 --target upload

# M5StickC Plus 2
pio run -e m5stick-c-plus2
pio run -e m5stick-c-plus2 --target upload
```

PlatformIO normally detects the serial port automatically. If more than one compatible device is attached, pass it explicitly:

```sh
pio run --target upload --upload-port /dev/cu.wchusbserial...
```

To view firmware logs, use the PlatformIO **Monitor** task or run:

```sh
pio device monitor
```

The Core2 environment uses PlatformIO's `m5stack-core2` board definition and 16 MB flash layout. The Plus2 environment uses the `m5stick-c` board definition with the Plus2 PSRAM flags and overrides the legacy board definition with the Plus2's 8 MB flash layout.

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

The device will save your settings and restart. It will then automatically connect to your specified Wi-Fi network and begin tracking aircraft.

### Usage

*   **M5Stack Core2**: The airline and route overview is the default page. Tap the left virtual button (BtnA) to cycle through the 6 telemetry pages. Press and hold the middle virtual button (BtnB) for 5 seconds to reset settings.
*   **M5StickC Plus 2**: Short press the large front button (BtnA) to change pages. Press and hold the right-side button (BtnB) for 5 seconds to reset settings.

The Core2 overview, telemetry pages, setup flow, and status messages all use responsive 320 x 240 layouts. The Plus2 keeps its compact 240 x 135 presentation.

Airline logo licensing and attribution are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Changelog

**Unreleased**

*   Added an M5Stack Core2 build target using the existing M5Unified hardware abstraction.
*   Added a full-screen Core2 flight overview with airline branding, flight number, aircraft type, and airport cities.
*   Expanded route parsing to use airport metadata and multi-leg route endpoints from the existing adsb.lol response.
*   Added retry handling for transient route lookup failures and graceful fallbacks for missing data.
*   Retained the original six telemetry pages as secondary Core2 pages and preserved the Plus2 presentation.
*   Added responsive Core2 layouts for setup, Wi-Fi connection, status, error, and telemetry screens.
*   Added three automatic retries to the initial Wi-Fi connection process.

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
