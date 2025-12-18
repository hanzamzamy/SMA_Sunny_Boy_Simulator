# SMA Sunny Boy Digital Twin Simulator

This project implements a digital twin simulator for the SMA Sunny Boy 2.0 solar inverter. It mimics the behavior of a real inverter by simulating power output based on diurnal cycles, weather conditions, and grid parameters. The simulator communicates via Modbus TCP, allowing external systems (e.g., SCADA, monitoring software) to read and write registers as if interacting with an actual device.

The simulator is built in C++17, uses CMake for build management, YAML for configuration, and libmodbus for Modbus TCP server functionality. It runs in a multi-threaded environment with a simulation engine updating register values in real-time.

## Features

- **Realistic Simulation**: Models solar power output using diurnal curves, seasonal variations, and weather models (e.g., sunny, partly cloudy, overcast, rainy).
- **Modbus TCP Server**: Supports Modbus function codes 3 (Read Holding Registers) and 4 (Read Input Registers), with basic write support for control registers.
- **Dynamic State Management**: Simulates device states (OK, Error, Off), fault injection, temperature derating, and grid parameter variations.
- **Configurable Parameters**: All simulation parameters, device identity, and register definitions are loaded from a YAML configuration file.
- **Thread-Safe Data Model**: Uses mutexes to ensure safe concurrent access to register data between the simulation thread and Modbus server thread.
- **Energy Accumulation**: Tracks total yield, daily yield, operating time, and feed-in time.
- **Grid Simulation**: Includes realistic grid voltage, frequency, and current calculations with variations.
- **Temperature Modeling**: Simulates internal temperature based on power output and ambient conditions.

## Requirements

- **Operating System**: Linux (tested on Ubuntu; may work on other Unix-like systems).
- **Compiler**: GCC 7+ or Clang 5+ supporting C++17.
- **Dependencies**:
  - `yaml-cpp` (for parsing YAML configuration).
  - `libmodbus` (for Modbus TCP communication).
  - `CMake` 3.10+ (for build system).
  - `pkg-config` (for finding libmodbus).
  - Standard C++ libraries (threads, chrono, etc.).
- **Hardware**: No special requirements; runs on standard desktop/server hardware.

Install dependencies on Ubuntu/Debian:
```bash
sudo apt update
sudo apt install build-essential cmake libyaml-cpp-dev libmodbus-dev pkg-config
```

## Building Dependencies from Source

If you prefer to build dependencies from source (e.g., for custom versions or if packages are not available), follow these steps. Note that `yaml-cpp` is licensed under MIT, and `libmodbus` is licensed under LGPL-2.1.

### Building yaml-cpp

1. Clone the repository:
   ```bash
   git clone https://github.com/jbeder/yaml-cpp.git
   cd yaml-cpp
   ```

2. Create a build directory and build:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Install system-wide (optional):
   ```bash
   sudo make install
   ```

### Building libmodbus

1. Clone the repository:
   ```bash
   git clone https://github.com/stephane/libmodbus.git
   cd libmodbus
   ```

2. Build using autotools:
   ```bash
   ./autogen.sh
   ./configure
   make
   ```

3. Install system-wide (optional):
   ```bash
   sudo make install
   ```

After building and installing from source, proceed with the project build as described in the Installation section.

## Installation

1. **Clone or Download the Repository**:
   - Ensure the project files are in a directory, e.g., `/path/to/SMA_SUNNY_BOY_Simulator`.

2. **Build the Project**:
   - Navigate to the project root directory.
   - Create a build directory and run CMake:
     ```bash
     mkdir build
     cd build
     cmake ..
     make
     ```
   - This generates the executable `sunny_boy_digital_twin` in the build directory.
   - The YAML configuration file sma_inverter_profile.yaml is automatically copied to the build directory.

3. **Optional: Install System-Wide**:
   - From the build directory:
     ```bash
     sudo make install
     ```
   - This installs the executable to bin and the config file to etc.

## Usage

1. **Run the Simulator**:
   - From the build directory:
     ```bash
     ./sunny_boy_digital_twin
     ```
   - Or, if installed:
     ```bash
     sunny_boy_digital_twin
     ```
   - The simulator starts a Modbus TCP server on port 1502 (configurable via code).
   - It loads configuration from sma_inverter_profile.yaml (or specify a custom file as the first argument).
   - Output shows initialization messages, simulation start, and weather changes.

2. **Connect via Modbus**:
   - Use a Modbus client (e.g., `modbus-cli`, Python's `pymodbus`, or SCADA software) to connect to `127.0.0.1:1502` (unit ID 3).
   - Read registers (e.g., function 4 for input registers like power output at address 30775).
   - Write to control registers (e.g., function 16 for holding registers like operating state at 40009).
   - Example with `modbus-cli` (install via `pip install modbus-cli`):
     ```bash
     modbus read 127.0.0.1 1502 3 4 30775 1  # Read AC power total
     ```

3. **Shutdown**:
   - Press `Ctrl+C` to gracefully stop the simulator. It joins threads and cleans up resources.

## Configuration

The simulator is driven by sma_inverter_profile.yaml. This file defines device identity, simulation parameters, and all Modbus registers.

### Key Sections

- **device_identity**: Static info like serial number, SUSy-ID, manufacturer.
- **simulation_parameters**: Controls like max power, efficiency, weather models, update interval.
- **registers**: List of Modbus registers with address, type (e.g., U32), format (e.g., FIX0), access (RO/RW), and initial value.

### Example Customization

- Change max power: Edit `max_power_watts` in `simulation_parameters`.
- Add a weather model: Append to `weather_models` list.
- Modify a register: Update the `registers` list (ensure address uniqueness).

Reload by restarting the simulator. Invalid YAML causes a runtime error.

## Architecture

- **main.cpp**: Entry point. Loads config, initializes data model, starts simulation engine and Modbus server, handles signals.
- **simulation_engine.cpp**: Core simulation loop. Updates registers based on time, weather, and state. Runs in a separate thread.
- **modbus_server.cpp**: Modbus TCP server. Handles client connections, reads/writes registers. Runs in a separate thread.
- **safe_data_model.cpp**: Thread-safe storage for registers. Maps logical registers to 16-bit Modbus registers.
- **config_loader.cpp**: Parses YAML into `Config` struct.
- **digital_twin.hpp**: Defines structs for config, registers, etc.
- **CMakeLists.txt**: Build configuration. Finds dependencies, compiles sources, links libraries.

Threads: Main thread waits for shutdown; simulation thread updates data; Modbus thread serves clients.

## Troubleshooting

- **Build Errors**: Ensure all dependencies are installed. Check CMake output for missing packages.
- **Runtime Errors**: Verify YAML syntax (use an online validator). Check console for "Failed to start Modbus server" or config load errors.
- **Connection Issues**: Confirm port 1502 is free. Use `netstat -tlnp | grep 1502` to check.
- **Simulation Not Updating**: Ensure update interval is set (default 1000ms). Check logs for weather changes.
- **Performance**: On low-end hardware, increase `update_interval_ms` to reduce CPU usage.

## Contributing

This is a simulation project. Contributions welcome via pull requests. Focus on improving realism (e.g., more accurate solar models) or adding features (e.g., additional Modbus functions).

## License

MIT License.