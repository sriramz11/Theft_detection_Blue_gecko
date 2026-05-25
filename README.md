# Bluetooth Mesh Backpack / Locker Anti-Tamper System

This project is a Bluetooth Mesh based anti-tamper prototype built using Silicon Labs Blue Gecko boards. The system detects possible opening or tamper activity near a backpack, locker, flap, zipper, or protected access point and reports the status wirelessly through a Bluetooth Mesh network.

The project uses one board as a Low-Power Node and one board as a Friend Node. The Low-Power Node performs sensing and publishes state updates. The Friend Node receives those updates, displays the current state, and provides a visible alert through the onboard LED.

## Project Overview

The system is designed around a simple embedded security flow:

1. The Low-Power Node reads external sensor data.
2. The firmware compares sensor readings against thresholds.
3. The node classifies the system state as `SAFE`, `OPEN_DETECTED`, or `TAMPER_DETECTED`.
4. The Low-Power Node publishes the state using a Bluetooth Mesh custom vendor model.
5. The Friend Node receives the message.
6. The Friend Node updates its LCD and controls the alert LED.

This is not a normal BLE GATT client/server project. It uses Bluetooth Mesh concepts such as provisioning, Friend/Low-Power Node behavior, vendor model messages, publication, and receive-side state handling.

## System Architecture

### Low-Power Node

The Low-Power Node is attached to the protected object such as a backpack or locker. It handles:

- TSL2591 ambient light sensor reading
- VCNL4010 proximity sensor reading
- I2C sensor communication
- Threshold based event detection
- Local LCD status display
- Bluetooth Mesh vendor model publishing
- Low-power idle behavior between sensing cycles

### Friend Node

The Friend Node acts as the monitoring side of the system. It handles:

- Bluetooth Mesh Friend functionality
- Vendor model message reception
- Message decoding
- LCD status display
- Onboard LED alert output
- Monitoring state display

## Hardware Used

- Silicon Labs Blue Gecko development boards
- TSL2591 ambient light sensor
- VCNL4010 proximity sensor
- Onboard LCD display
- Onboard LED
- I2C interface for sensor communication

The original proposal used the TSL2561 light sensor, but the final implementation uses the TSL2591 because it was the available hardware. The system role remains the same: detecting opening-related light exposure.

## Repository Structure

```text
Theft_detection_Blue_gecko/
│
├── btmesh_vendor_client/
│   └── Low-Power Node / Client-side firmware
│
├── btmesh_vendor_server/
│   └── Friend Node / Server-side firmware
│
└── README.md
```

## Application States

| State | Meaning |
|---|---|
| `SAFE` | No opening or tamper condition detected |
| `OPEN_DETECTED` | Light level crossed the opening threshold |
| `TAMPER_DETECTED` | Proximity level crossed the tamper threshold |
| `ACK_ALARM` | Optional acknowledgement state |

## Data Flow

```text
TSL2591 Light Sensor
        │
        ├── I2C Read
        │
VCNL4010 Proximity Sensor
        │
        ▼
Threshold / Event Detection Logic
        │
        ▼
System State: SAFE / OPEN / TAMPER
        │
        ▼
Bluetooth Mesh Vendor Model Publish
        │
        ▼
Friend Node Receive + Decode
        │
        ├── LCD Status Update
        └── LED Alert Control
```

## Software Features

- Bluetooth Mesh Friend and Low-Power Node setup
- Custom vendor model message handling
- I2C driver support for external sensors
- TSL2591 light sensor integration
- VCNL4010 proximity sensor integration
- Threshold based state machine
- LCD status updates on both nodes
- LED alert output on the Friend Node
- Periodic sensor polling
- Low-power behavior evaluation using Energy Profiler

## Sensor Logic

The Low-Power Node periodically samples the light and proximity sensors.

A simplified version of the decision logic is:

```c
if (proximity_count > PROXIMITY_THRESHOLD)
{
    state = TAMPER_DETECTED;
}
else if (light_count > LIGHT_THRESHOLD)
{
    state = OPEN_DETECTED;
}
else
{
    state = SAFE;
}
```

The proximity event has higher priority because a close hand or object near the protected opening is a stronger tamper indication than light change alone.

## Build and Run Instructions

1. Clone the repository:

```bash
git clone https://github.com/sriramz11/Theft_detection_Blue_gecko.git
```

2. Open Silicon Labs Simplicity Studio.

3. Import both project folders:

```text
btmesh_vendor_client
btmesh_vendor_server
```

4. Build the client project.

5. Flash the client project to the board used as the Low-Power Node.

6. Build the server project.

7. Flash the server project to the board used as the Friend Node.

8. Provision or self-provision the Bluetooth Mesh nodes based on the course project setup.

9. Verify the LCD output:

```text
Low-Power Node:
Client/LPN
LUX value
SAFE / OPEN / TAMPER

Friend Node:
Server/Friend
LUX value
SAFE / OPEN / TAMPER
Connected
```

## Expected Demo Behavior

### Safe Condition

- Backpack or locker remains closed
- Light reading stays below threshold
- Proximity reading stays below threshold
- Low-Power Node displays `SAFE`
- Friend Node displays `SAFE`
- Alert LED remains off

### Open Detected

- Light level increases near the protected opening
- Low-Power Node publishes `OPEN_DETECTED`
- Friend Node displays `OPEN_DETECTED`
- Alert LED turns on

### Tamper Detected

- A hand or object is detected near the opening
- Low-Power Node publishes `TAMPER_DETECTED`
- Friend Node displays `TAMPER_DETECTED`
- Alert LED turns on

### Recovery

- Sensor values return to normal
- Low-Power Node publishes `SAFE`
- Friend Node clears the alert state

## Testing

The project was tested through subsystem and integration tests.

Test areas included:

- Project build and compilation
- Node boot and initialization
- I2C sensor communication
- TSL2591 light sensor reading
- VCNL4010 proximity sensor reading
- Threshold based event detection
- LCD updates on both nodes
- Bluetooth Mesh message exchange
- Friend Node receive behavior
- LED alert output
- Low-power behavior using Energy Profiler

## Low-Power Behavior

The Low-Power Node is intended to spend most of its time in a lower-energy state and wake only when sensing, LCD update, or mesh communication is required.

The project uses the Friend/Low-Power Node structure so that the sensing node does not need to remain fully active all the time. The Friend Node stays more available and supports the mesh monitoring side.

Energy Profiler measurements showed periodic active regions followed by lower-current regions, which confirms that the Low-Power Node was not running as one continuous active workload.

## Technologies Used

- Embedded C
- Silicon Labs Bluetooth Mesh SDK
- Simplicity Studio
- Blue Gecko development boards
- I2C sensor communication
- Bluetooth Mesh vendor models
- LCD display interface
- GPIO LED control
- Energy Profiler

## Authors

- Sriramkumar Jayaraman

Course Project: ECEN 5823 Internet of Things Embedded Firmware  
University of Colorado Boulder

## Notes

This project was developed as an academic embedded systems prototype. It demonstrates Bluetooth Mesh communication, external sensor integration, low-power design concepts, and node-to-node event reporting. It is not intended to be used as a commercial security product without further hardware validation, enclosure design, security review, and long-term reliability testing.
