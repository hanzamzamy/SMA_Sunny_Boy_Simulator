#include "simulation_engine.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <ctime>
#include <random>

SimulationEngine::SimulationEngine(std::shared_ptr<SafeDataModel> model, const Config& cfg)
    : data_model(model), config(cfg), running(false), current_state(DeviceState::OFF),
      current_weather_model_index(0), last_weather_change_time(0) {
    
    // Set static values from config
    data_model->setLogicalValue(30003, config.identity.susy_id);
    data_model->setLogicalValue(30005, config.identity.serial_number);
    data_model->setLogicalValue(30051, config.identity.device_class);
    data_model->setLogicalValue(30053, config.identity.susy_id);
    data_model->setLogicalValue(30055, config.identity.manufacturer);
    data_model->setLogicalValue(30057, config.identity.serial_number);
    data_model->setLogicalValue(30059, config.identity.software_package);
    data_model->setLogicalValue(30231, (uint32_t)config.sim_params.max_power_watts);
}

void SimulationEngine::start() {
    if (running) return;
    running = true;
    simulation_thread = std::thread(&SimulationEngine::run, this);
}

void SimulationEngine::stop() {
    if (!running) return;
    running = false;
    if (simulation_thread.joinable()) {
        simulation_thread.join();
    }
}

void SimulationEngine::run() {
    std::cout << "Simulation thread started." << std::endl;
    while (running) {
        auto start_time = std::chrono::steady_clock::now();

        updateSimulationState();

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto sleep_duration = std::chrono::milliseconds(config.sim_params.update_interval_ms) - elapsed;

        if (sleep_duration.count() > 0) {
            std::this_thread::sleep_for(sleep_duration);
        }
    }
    std::cout << "Simulation thread stopped." << std::endl;
}

double SimulationEngine::calculatePowerOutput() {
    time_t now = time(0);
    struct tm *ltm = localtime(&now);

    // Simple diurnal curve: sine wave from 6 AM to 6 PM
    double hour_of_day = ltm->tm_hour + ltm->tm_min / 60.0;
    if (hour_of_day < 6.0 || hour_of_day > 18.0) {
        return 0.0;
    }

    double diurnal_factor = sin((hour_of_day - 6.0) * M_PI / 12.0);
    
    // Check for weather change
    if (now - last_weather_change_time > config.sim_params.weather_change_interval_seconds) {
        current_weather_model_index = rand() % config.sim_params.weather_models.size();
        last_weather_change_time = now;
        std::cout << "Weather changed to: " << config.sim_params.weather_models[current_weather_model_index].name << std::endl;
    }

    double weather_multiplier = config.sim_params.weather_models[current_weather_model_index].power_multiplier;
    
    return config.sim_params.max_power_watts * diurnal_factor * weather_multiplier;
}

void SimulationEngine::updateSimulationState() {
    // 1. Check for client commands
    auto op_state_val = data_model->getLogicalValue(40009); // Operating State
    auto ack_error_val = data_model->getLogicalValue(40011); // Acknowledge Error

    uint32_t op_state = op_state_val ? std::get<uint32_t>(*op_state_val) : 295;
    uint32_t ack_error = ack_error_val ? std::get<uint32_t>(*ack_error_val) : 0;
    
    // 2. Update state machine
    if (ack_error == 26 && current_state == DeviceState::ERROR) {
        current_state = DeviceState::OFF;
        data_model->setLogicalValue(40011, (uint32_t)0); // Reset ack
    } else if (op_state == 381) { // Stop command
        current_state = DeviceState::OFF;
    } else if (current_state != DeviceState::ERROR) {
        // Random fault injection
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 100);
        if (dis(gen) < config.sim_params.fault_probability_percent) {
             current_state = DeviceState::ERROR;
        } else if (op_state == 295) { // MPP command
             current_state = DeviceState::OK;
        }
    }

    // 3. Calculate dynamic values based on state
    double ac_power = 0.0;
    double dc_power = 0.0;
    uint32_t device_status_enum = 303; // Off
    uint32_t grid_contactor_enum = 311; // Open
    double voltage = 230.0;
    double current = 0.0;

    if (current_state == DeviceState::OK) {
        ac_power = calculatePowerOutput();
        if (ac_power > 0) {
            double efficiency = config.sim_params.efficiency_percent / 100.0;
            dc_power = ac_power / efficiency;
            device_status_enum = 307; // OK
            grid_contactor_enum = 51; // Closed
            current = ac_power / voltage;
        } else {
            // Still OK, but night time, so no power
            device_status_enum = 307; // OK
            grid_contactor_enum = 311; // Open
        }
    } else if (current_state == DeviceState::ERROR) {
        device_status_enum = 35; // Error
        grid_contactor_enum = 311; // Open
    }
    // Other states like WARNING can be added here.
    
    // 4. Update accumulators
    double seconds_per_tick = config.sim_params.update_interval_ms / 1000.0;
    auto op_time_val = data_model->getLogicalValue(30521);
    uint64_t op_time = op_time_val ? std::get<uint64_t>(*op_time_val) : 0;
    op_time += static_cast<uint64_t>(seconds_per_tick);
    
    auto feed_time_val = data_model->getLogicalValue(30525);
    uint64_t feed_time = feed_time_val ? std::get<uint64_t>(*feed_time_val) : 0;
    
    auto total_yield_val = data_model->getLogicalValue(30513);
    uint64_t total_yield = total_yield_val ? std::get<uint64_t>(*total_yield_val) : 0;

    auto daily_yield_val = data_model->getLogicalValue(30517);
    uint64_t daily_yield = daily_yield_val ? std::get<uint64_t>(*daily_yield_val) : 0;

    if (ac_power > 0) {
        feed_time += static_cast<uint64_t>(seconds_per_tick);
        double energy_wh = ac_power * (seconds_per_tick / 3600.0);
        total_yield += static_cast<uint64_t>(energy_wh);
        daily_yield += static_cast<uint64_t>(energy_wh);
    }
    // Note: A real implementation would reset daily yield at midnight.

    // 5. Calculate correlated data (e.g., temperature)
    double power_ratio = (ac_power > 0) ? (ac_power / config.sim_params.max_power_watts) : 0.0;
    double internal_temp = 25.0 + (config.sim_params.max_internal_temp_celsius - 25.0) * power_ratio;

    // 6. Write all new values to the data model
    data_model->setLogicalValue(30201, device_status_enum);
    data_model->setLogicalValue(30217, grid_contactor_enum);
    data_model->setLogicalValue(30775, static_cast<int32_t>(ac_power));
    data_model->setLogicalValue(30773, static_cast<int32_t>(dc_power));
    data_model->setLogicalValue(30797, static_cast<uint32_t>(current * 1000)); // FIX3
    data_model->setLogicalValue(30953, static_cast<int32_t>(internal_temp * 10)); // TEMP is FIX1
    
    data_model->setLogicalValue(30521, op_time);
    data_model->setLogicalValue(30525, feed_time);
    data_model->setLogicalValue(30513, total_yield);
    data_model->setLogicalValue(30517, daily_yield);
}
