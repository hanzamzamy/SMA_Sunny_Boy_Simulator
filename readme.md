# SMA Sunny Boy Digital Twin Simulator

The **SMA Sunny Boy Digital Twin Simulator** is a high-fidelity C++17 implementation of a digital twin for the SMA Sunny Boy solar inverter series. It is designed to behave exactly like a physical unit on a network, providing a low-cost, high-reliability environment for testing SCADA, EMS, and industrial IoT platforms.

## Core Architecture & Logic

The simulator operates as a multi-threaded system where the physical world (simulation) and the digital world (Modbus) are decoupled by a thread-safe data model.

### 1. The Simulation Engine (`simulation_engine.cpp`)

This is the heart of the project. It follows physical models to generate data:

- **Diurnal Curve**: Uses a bell-curve (Gaussian) distribution based on the system time. It calculates `noon`, `sunrise`, and `sunset` with seasonal offsets to simulate longer days in summer.

- **Thermal Inertia**: Internal temperature is calculated using the formula:

  $$T_{internal} = T_{ambient} + (T_{max} - T_{ambient}) \times \frac{P_{out}}{P_{max}} \times WeatherFactor$$

  A smoothing factor (thermal inertia) is applied so temperature doesn't jump instantly.

- **Power Derating**: If the simulated internal temperature exceeds **65°C**, the engine automatically throttles AC output (Linear Derating) to protect the virtual hardware.

### 2. Config Loader (`config_loader.cpp`)

The `ConfigLoader` class uses the [`yaml-cpp`](https://github.com/jbeder/yaml-cpp) library to read the device profile and simulation parameters from the `sma_inverter_profile.yaml` file. It populates a `Config` structure that drives the entire simulation, including device identity, simulation parameters, and all Modbus registers.

### 3. Safe Data Model (`safe_data_model.cpp`)

Because the **Simulation Thread** writes data and the **Modbus Thread** reads/writes it, we use a mutex-protected `unordered_map`. This model handles the Splitting of data:

- **Logic to Protocol**: A 32-bit value (like Serial Number) is automatically deconstructed into two 16-bit Modbus registers (High Word and Low Word) following the **Big Endian** standard used by SMA.

### 4. Modbus Layer (`modbus_server.cpp`)

It implements a subset of the SMA Modbus protocol. It listens on a configurable port (default 1502) and responds to Function Codes `0x03` (Read Holding) and `0x04` (Read Input). It utilizes [`libmodbus`](https://github.com/stephane/libmodbus) to manage low-level TCP frame handling, socket management, and protocol compliance.

## Prerequisites

- **C/C++ Compiler**: Support for C99 and C++17.
- **CMake**: Version 3.10+.
- **Dependencies**: `libmodbus` and `yaml-cpp`.


## Building on Linux/macOS

### Option 1: Install Prebuilt Packages (Linux only)

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake libyaml-cpp-dev libmodbus-dev pkg-config
```

Then, jump to step 4.

### Option 2: Build Dependencies from Source

If your distro doesn't support the binary packages, follow these steps:

#### 1. Build `yaml-cpp`

```bash
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp && mkdir build && cd build
cmake -DYAML_BUILD_SHARED_LIBS=ON ..
make -j$(nproc)
sudo make install
```

#### 2. Build `libmodbus`

```bash
git clone [https://github.com/stephane/libmodbus.git](https://github.com/stephane/libmodbus.git)
cd libmodbus
./autogen.sh
./configure
make && sudo make install
```

#### 3. Build the Simulator

```bash
cd SMA_Sunny_Boy_Simulator
mkdir build && cd build
cmake ..
make
```


## Building on Windows (vcpkg)

### 1. Install vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
```

### 2. Install Libs

```powershell
vcpkg install libmodbus:x64-windows yaml-cpp:x64-windows
```

### 3. Build the Simulator

```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```


## Development & Hacking

How to add a new simulated behavior:
1. **Define Register**: Add the address and type to `sma_inverter_profile.yaml`.
2. **Update Logic**: In `simulation_engine.cpp`, within the `updateSimulationState()` function, calculate your new variable.
3. **Commit to Model**: Use `data_model->setLogicalValue(ADDRESS, VALUE);`.

Hacking for Stress Testing:
- **Fault Injection**: Change `fault_probability_percent` in the YAML to a high value (e.g., `50.0`) to force the system into an `ERROR` state (Status `35`) frequently.
- Time Acceleration: You can hack the system time by modifying the `seconds_per_tick` calculation in the code to simulate a full 24-hour cycle in just a few minutes.

## Integration Options

This Simulator is a **Modbus Server**. It needs client to serve.
1. **Sister Project**: You can pair this simulator to its sister project [SMA Inverter Modbus to OPC UA Gateway](https://github.com/hanzamzamy/SMA_Sunny_Boy_OPC_Server/), which combine Modbus Client and OPC UA Server, by pointing the Gateway to this Simulator (`127.0.0.1:1502`).
2. **Modbus Client**: Use other Modbus Client application or write your own.
3. SCADA Ecosystem: Some proprietary softwares provide ecosystem that include communication via Modbus TCP, OPC Server, and SCADA & HMI solution.

## License

MIT License - 2025 Rayhan Zamzamy.
