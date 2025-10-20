#ifndef DIGITAL_TWIN_H
#define DIGITAL_TWIN_H

#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

/// @brief Defines the access type for a Modbus register.
enum class RegisterAccess {
    RO, ///< Read Only
    RW, ///< Read Write
    WO  ///< Write Only
};

/// @brief Defines the data format for a Modbus register.
enum class RegisterFormat {
    RAW,    ///< Raw value
    ENUM,   ///< Enumeration
    FIX0,   ///< Fixed point, 0 decimal places
    FIX1,   ///< Fixed point, 1 decimal place
    FIX2,   ///< Fixed point, 2 decimal places
    FIX3,   ///< Fixed point, 3 decimal places
    FIX4,   ///< Fixed point, 4 decimal places
    DT,     ///< Date/Time
    FW,     ///< Firmware version
    TEMP,   ///< Temperature
    Duration ///< Duration in seconds
};

/// @brief Defines the data type for a Modbus register.
enum class RegisterType {
    U16,
    S16,
    U32,
    S32,
    U64,
    S64
};

/**
 * @struct Register
 * @brief Holds all properties of a single Modbus register.
 *
 * This structure is populated from the YAML configuration file and represents
 * a single register in the device's data model.
 */
struct Register {
    uint16_t address;
    RegisterType type;
    RegisterFormat format;
    RegisterAccess access;
    std::variant<uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t> value;
    size_t num_regs; // Number of 16-bit Modbus registers it occupies
};

/**
 * @struct DeviceIdentity
 * @brief Holds static identification data for the simulated device.
 */
struct DeviceIdentity {
    int unit_id;
    uint32_t serial_number;
    uint32_t susy_id;
    uint32_t device_class;
    uint32_t manufacturer;
    uint32_t software_package;
};

/**
 * @struct WeatherModel
 * @brief Defines parameters for a specific weather condition.
 */
struct WeatherModel {
    std::string name;
    double power_multiplier;
    double temp_increase_factor;
};

/**
 * @struct SimulationParams
 * @brief Holds parameters that control the simulation engine's behavior.
 */
struct SimulationParams {
    int update_interval_ms;
    double max_power_watts;
    double efficiency_percent;
    double max_internal_temp_celsius;
    double fault_probability_percent;
    double voltage_variation_percent;
    double grid_voltage_nominal;
    double grid_frequency_nominal;
    double frequency_variation_hz;
    int daily_yield_reset_hour;
    double ambient_temp_celsius;
    int startup_delay_seconds;
    int shutdown_delay_seconds;
    int weather_change_interval_seconds;
    std::vector<WeatherModel> weather_models;
};

/**
 * @struct Config
 * @brief Top-level structure to hold the entire parsed configuration.
 */
struct Config {
    DeviceIdentity identity;
    SimulationParams sim_params;
    std::vector<Register> registers;
};

#endif // DIGITAL_TWIN_H
