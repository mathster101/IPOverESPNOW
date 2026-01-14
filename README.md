# IPOverESPNOW

Create an IP layer link between two computers using packet transmission over ESP-NOWv2 with ESP32 boards.

## Overview

This project establishes a network tunnel between two computers by routing IP packets through ESP32 boards using the ESP-NOW protocol. The system consists of two components: Python code running on each computer and Arduino firmware running on ESP32 boards connected via serial ports.

## Hardware Requirements

- 2x ESP32 boards (preferably ESP32-S3)
- 2x USB cables for serial connection
- 2x Linux computers

## Software Requirements

- Python 3.x
- Arduino IDE
- ESP32 board support for Arduino IDE
## Installation & Setup

### 1. Flash ESP32 Boards

1. Open the Arduino IDE
2. Navigate to the `ESP32 Firmware` folder
3. Open the `.ino` file in Arduino IDE
4. Connect your ESP32 board via USB
5. Select the correct board and port from Tools menu
6. Upload the firmware to the ESP32
7. Repeat for the second ESP32 board

### 2. Configure Python Environment

Install required Python dependencies

### 3. Run the Tunnel

On each computer, run the Python program with sudo (required for TUN device creation):

```bash
sudo python runTunnel.py --tun_ip <IP_ADDRESS> --serial_port <SERIAL_PORT>
```

#### Arguments

- `--tun_ip`: IP address to assign to the TUN interface (e.g., `10.0.0.1` on one side, `10.0.0.2` on the other)
- `--serial_port`: Serial port where the ESP32 is connected (e.g., `/dev/ttyUSB0`, `/dev/ttyACM0`)

#### Example

**Computer 1:**
```bash
sudo python runTunnel.py --tun_ip 10.0.0.1 --serial_port /dev/ttyUSB0
```

**Computer 2:**
```bash
sudo python runTunnel.py --tun_ip 10.0.0.2 --serial_port /dev/ttyUSB0
```

## Configuration

Additional configuration options can be modified in `config.py`.

## How It Works

1. The Python program creates a TUN (network tunnel) device on the host computer
2. IP packets sent to the TUN interface are captured and forwarded to the ESP32 via serial
3. The ESP32 firmware transmits the packets wirelessly using ESP-NOWv2 protocol
4. The receiving ESP32 forwards packets back through serial to its connected computer
5. The receiving Python program injects packets into its TUN interface

## Troubleshooting

- Ensure both ESP32 boards are properly flashed with the firmware
- Verify the correct serial port is specified
- Check that the TUN IP addresses are on the same subnet but different
- Remember to run the Python program with sudo for TUN device permissions
