#include "config_loader.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <stdexcept>

// Helper to convert string to enum
RegisterAccess to_access(const std::string& s) {
    if (s == "RO") return RegisterAccess::RO;
    if (s == "RW") return RegisterAccess::RW;
    if (s == "WO") return RegisterAccess::WO;
    throw std::runtime_error("Invalid register access type: " + s);
}

RegisterType to_type(const std::string& s) {
    if (s == "U16") return RegisterType::U16;
    if (s == "S16") return RegisterType::S16;
    if (s == "U32") return RegisterType::U32;
    if (s == "S32") return RegisterType::S32;
    if (s == "U64") return RegisterType::U64;
    if (s == "S64") return RegisterType::S64;
    throw std::runtime_error("Invalid register type: " + s);
}

RegisterFormat to_format(const std::string& s) {
    if (s == "RAW") return RegisterFormat::RAW;
    if (s == "ENUM") return RegisterFormat::ENUM;
    if (s == "FIX0") return RegisterFormat::FIX0;
    if (s == "FIX1") return RegisterFormat::FIX1;
    if (s == "FIX2") return RegisterFormat::FIX2;
    if (s == "FIX3") return RegisterFormat::FIX3;
    if (s == "FIX4") return RegisterFormat::FIX4;
    if (s == "DT") return RegisterFormat::DT;
    if (s == "FW") return RegisterFormat::FW;
    if (s == "TEMP") return RegisterFormat::TEMP;
    if (s == "Duration") return RegisterFormat::Duration;
    throw std::runtime_error("Invalid register format: " + s);
}


Config ConfigLoader::loadConfig(const std::string& filename) {
    Config config;
    YAML::Node root = YAML::LoadFile(filename);

    // Load Device Identity
    const auto& identity_node = root["device_identity"];
    config.identity.unit_id = identity_node["unit_id"].as<int>();
    config.identity.serial_number = identity_node["serial_number"].as<uint32_t>();
    config.identity.susy_id = identity_node["susy_id"].as<uint32_t>();
    config.identity.device_class = identity_node["device_class"].as<uint32_t>();
    config.identity.manufacturer = identity_node["manufacturer"].as<uint32_t>();
    config.identity.software_package = identity_node["software_package"].as<uint32_t>();

    // Load Simulation Parameters
    const auto& sim_node = root["simulation_parameters"];
    config.sim_params.update_interval_ms = sim_node["update_interval_ms"].as<int>();
    config.sim_params.max_power_watts = sim_node["max_power_watts"].as<double>();
    config.sim_params.efficiency_percent = sim_node["efficiency_percent"].as<double>();
    config.sim_params.max_internal_temp_celsius = sim_node["max_internal_temp_celsius"].as<double>();
    config.sim_params.fault_probability_percent = sim_node["fault_probability_percent"].as<double>();
    config.sim_params.weather_change_interval_seconds = sim_node["weather_change_interval_seconds"].as<int>();

    const auto& weather_nodes = root["weather_models"];
    for (const auto& node : weather_nodes) {
        config.sim_params.weather_models.push_back({
            node["name"].as<std::string>(),
            node["power_multiplier"].as<double>()
        });
    }

    // Load Registers
    const auto& reg_nodes = root["registers"];
    for (const auto& node : reg_nodes) {
        Register reg;
        reg.address = node["address"].as<uint16_t>();
        reg.type = to_type(node["type"].as<std::string>());
        reg.format = to_format(node["format"].as<std::string>());
        reg.access = to_access(node["access"].as<std::string>());

        switch (reg.type) {
            case RegisterType::U16:
                reg.value = node["value"] ? node["value"].as<uint16_t>() : (uint16_t)0;
                reg.num_regs = 1;
                break;
            case RegisterType::S16:
                 reg.value = node["value"] ? node["value"].as<int16_t>() : (int16_t)0;
                reg.num_regs = 1;
                break;
            case RegisterType::U32:
                 reg.value = node["value"] ? node["value"].as<uint32_t>() : (uint32_t)0;
                reg.num_regs = 2;
                break;
            case RegisterType::S32:
                 reg.value = node["value"] ? node["value"].as<int32_t>() : (int32_t)0;
                reg.num_regs = 2;
                break;
            case RegisterType::U64:
                 reg.value = node["value"] ? node["value"].as<uint64_t>() : (uint64_t)0;
                reg.num_regs = 4;
                break;
            case RegisterType::S64:
                 reg.value = node["value"] ? node["value"].as<int64_t>() : (int64_t)0;
                reg.num_regs = 4;
                break;
        }
        config.registers.push_back(reg);
    }
    return config;
}
