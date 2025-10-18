#ifndef SAFE_DATA_MODEL_H
#define SAFE_DATA_MODEL_H

#include "digital_twin.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

/**
 * @class SafeDataModel
 * @brief Manages the shared state of all Modbus registers with thread-safe access.
 *
 * This class acts as the central repository for the inverter's register data.
 * It uses a mutex to protect the data from concurrent access by the Modbus
 * server thread and the simulation engine thread.
 */
class SafeDataModel {
public:
    /**
     * @brief Initializes the data model from the loaded configuration.
     * @param initial_registers A vector of Register structs from the config file.
     */
    void initialize(const std::vector<Register>& initial_registers);

    /**
     * @brief Gets the value of a single 16-bit Modbus register.
     * @param address The Modbus address of the register.
     * @param value A reference to be filled with the register's value.
     * @return True if the address is valid and the value was retrieved, false otherwise.
     */
    bool getRegisterValue(uint16_t address, uint16_t& value);

    /**
     * @brief Sets the value of a single 16-bit Modbus register.
     * @param address The Modbus address of the register.
     * @param value The new value to set.
     * @return True if the address is valid and writable, false otherwise.
     */
    bool setRegisterValue(uint16_t address, uint16_t value);

    /**
     * @brief Gets the value of a register by its logical address, handling multi-register values.
     * @param address The logical start address of the register.
     * @return An std::optional containing the register's value if found.
     */
    std::optional<std::variant<uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t>> getLogicalValue(uint16_t address);

    /**
     * @brief Sets the value of a register by its logical address, handling multi-register values.
     * @param address The logical start address of the register.
     * @param value The new value to set.
     */
    void setLogicalValue(uint16_t address, const std::variant<uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t>& value);


private:
    std::mutex data_mutex;
    std::unordered_map<uint16_t, Register> logical_register_map;
    std::unordered_map<uint16_t, uint16_t> modbus_register_map;
};

#endif // SAFE_DATA_MODEL_H
