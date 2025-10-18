#include "safe_data_model.hpp"
#include <iostream>

void SafeDataModel::initialize(const std::vector<Register>& initial_registers) {
    std::lock_guard<std::mutex> lock(data_mutex);

    for (const auto& reg_template : initial_registers) {
        logical_register_map[reg_template.address] = reg_template;

        // Deconstruct the logical value into 16-bit Modbus registers
        uint16_t start_addr = reg_template.address;
        switch (reg_template.type) {
            case RegisterType::U16:
                modbus_register_map[start_addr] = std::get<uint16_t>(reg_template.value);
                break;
            case RegisterType::S16:
                modbus_register_map[start_addr] = static_cast<uint16_t>(std::get<int16_t>(reg_template.value));
                break;
            case RegisterType::U32: {
                uint32_t val = std::get<uint32_t>(reg_template.value);
                modbus_register_map[start_addr] = (val >> 16) & 0xFFFF; // High word
                modbus_register_map[start_addr + 1] = val & 0xFFFF; // Low word
                break;
            }
            case RegisterType::S32: {
                int32_t val = std::get<int32_t>(reg_template.value);
                uint32_t uval = static_cast<uint32_t>(val);
                modbus_register_map[start_addr] = (uval >> 16) & 0xFFFF; // High word
                modbus_register_map[start_addr + 1] = uval & 0xFFFF; // Low word
                break;
            }
            case RegisterType::U64: {
                uint64_t val = std::get<uint64_t>(reg_template.value);
                modbus_register_map[start_addr] = (val >> 48) & 0xFFFF;
                modbus_register_map[start_addr + 1] = (val >> 32) & 0xFFFF;
                modbus_register_map[start_addr + 2] = (val >> 16) & 0xFFFF;
                modbus_register_map[start_addr + 3] = val & 0xFFFF;
                break;
            }
            case RegisterType::S64: {
                int64_t val = std::get<int64_t>(reg_template.value);
                uint64_t uval = static_cast<uint64_t>(val);
                modbus_register_map[start_addr] = (uval >> 48) & 0xFFFF;
                modbus_register_map[start_addr + 1] = (uval >> 32) & 0xFFFF;
                modbus_register_map[start_addr + 2] = (uval >> 16) & 0xFFFF;
                modbus_register_map[start_addr + 3] = uval & 0xFFFF;
                break;
            }
        }
    }
}

bool SafeDataModel::getRegisterValue(uint16_t address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(data_mutex);
    auto it = modbus_register_map.find(address);
    if (it != modbus_register_map.end()) {
        value = it->second;
        return true;
    }
    return false;
}

bool SafeDataModel::setRegisterValue(uint16_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(data_mutex);

    // Find which logical register this modbus register belongs to
    uint16_t logical_addr = 0;
    Register* logical_reg = nullptr;
    for(auto& pair : logical_register_map) {
        if(address >= pair.first && address < pair.first + pair.second.num_regs) {
            logical_addr = pair.first;
            logical_reg = &pair.second;
            break;
        }
    }

    if (!logical_reg) {
        std::cerr << "Warning: Write to unmapped modbus address " << address << std::endl;
        return false;
    }

    if (logical_reg->access == RegisterAccess::RO) {
        std::cerr << "Warning: Denied write to RO logical register " << logical_addr << std::endl;
        return false;
    }

    // Update the 16-bit register in the map
    modbus_register_map[address] = value;

    // Reconstruct the logical value from the updated 16-bit registers
    switch(logical_reg->type) {
        case RegisterType::U16:
            logical_reg->value = value;
            break;
        case RegisterType::S16:
            logical_reg->value = static_cast<int16_t>(value);
            break;
        case RegisterType::U32: {
            uint32_t high = modbus_register_map[logical_addr];
            uint32_t low = modbus_register_map[logical_addr + 1];
            logical_reg->value = (high << 16) | low;
            break;
        }
        case RegisterType::S32: {
            uint32_t high = modbus_register_map[logical_addr];
            uint32_t low = modbus_register_map[logical_addr + 1];
            uint32_t uval = (high << 16) | low;
            logical_reg->value = static_cast<int32_t>(uval);
            break;
        }
        case RegisterType::U64: {
            uint64_t val = 0;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr]) << 48;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr+1]) << 32;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr+2]) << 16;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr+3]);
            logical_reg->value = val;
            break;
        }
        case RegisterType::S64: {
            uint64_t val = 0;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr]) << 48;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr+1]) << 32;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr+2]) << 16;
            val |= static_cast<uint64_t>(modbus_register_map[logical_addr+3]);
            logical_reg->value = static_cast<int64_t>(val);
            break;
        }
    }
    return true;
}

std::optional<std::variant<uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t>> SafeDataModel::getLogicalValue(uint16_t address) {
    std::lock_guard<std::mutex> lock(data_mutex);
    auto it = logical_register_map.find(address);
    if (it != logical_register_map.end()) {
        return it->second.value;
    }
    return std::nullopt;
}

void SafeDataModel::setLogicalValue(uint16_t address, const std::variant<uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t>& value) {
    std::lock_guard<std::mutex> lock(data_mutex);
    auto it = logical_register_map.find(address);
    if (it != logical_register_map.end()) {
        it->second.value = value;

        // Deconstruct and update the underlying 16-bit modbus registers
        switch (it->second.type) {
            case RegisterType::U16:
                modbus_register_map[address] = std::get<uint16_t>(value);
                break;
            case RegisterType::S16:
                modbus_register_map[address] = static_cast<uint16_t>(std::get<int16_t>(value));
                break;
            case RegisterType::U32: {
                uint32_t val = std::get<uint32_t>(value);
                modbus_register_map[address] = (val >> 16) & 0xFFFF;
                modbus_register_map[address + 1] = val & 0xFFFF;
                break;
            }
            case RegisterType::S32: {
                int32_t val = std::get<int32_t>(value);
                uint32_t uval = static_cast<uint32_t>(val);
                modbus_register_map[address] = (uval >> 16) & 0xFFFF;
                modbus_register_map[address + 1] = uval & 0xFFFF;
                break;
            }
            case RegisterType::U64: {
                uint64_t val = std::get<uint64_t>(value);
                modbus_register_map[address] = (val >> 48) & 0xFFFF;
                modbus_register_map[address + 1] = (val >> 32) & 0xFFFF;
                modbus_register_map[address + 2] = (val >> 16) & 0xFFFF;
                modbus_register_map[address + 3] = val & 0xFFFF;
                break;
            }
            case RegisterType::S64: {
                int64_t val = std::get<int64_t>(value);
                uint64_t uval = static_cast<uint64_t>(val);
                modbus_register_map[address] = (uval >> 48) & 0xFFFF;
                modbus_register_map[address + 1] = (uval >> 32) & 0xFFFF;
                modbus_register_map[address + 2] = (uval >> 16) & 0xFFFF;
                modbus_register_map[address + 3] = uval & 0xFFFF;
                break;
            }
        }
    }
}
