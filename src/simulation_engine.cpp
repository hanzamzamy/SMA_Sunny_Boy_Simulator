#include "simulation_engine.hpp"
#include <iostream>
#include <chrono>
#include <cmath>
#include <ctime>
#include <random>

SimulationEngine::SimulationEngine(std::shared_ptr<SafeDataModel> model, const Config& cfg)
    : data_model(model), config(cfg), running(false), current_state(DeviceState::OK), // Start in OK state
      current_weather_model_index(0), last_weather_change_time(0), last_daily_reset_day(-1) {
    
    // Set static values from config
    data_model->setLogicalValue(30003, config.identity.susy_id);
    data_model->setLogicalValue(30005, config.identity.serial_number);
    data_model->setLogicalValue(30051, config.identity.device_class);
    data_model->setLogicalValue(30053, config.identity.susy_id);
    data_model->setLogicalValue(30055, config.identity.manufacturer);
    data_model->setLogicalValue(30057, config.identity.serial_number);
    data_model->setLogicalValue(30059, config.identity.software_package);
    data_model->setLogicalValue(30231, (uint32_t)config.sim_params.max_power_watts);
    
    // Initialize random number generator
    std::random_device rd;
    rng.seed(rd());
    
    std::cout << "Inverter starting in operational state..." << std::endl;
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

    // Enhanced diurnal curve with seasonal variation
    double hour_of_day = ltm->tm_hour + ltm->tm_min / 60.0 + ltm->tm_sec / 3600.0;
    
    // Seasonal adjustment (day of year)
    int day_of_year = ltm->tm_yday;
    double seasonal_factor = 0.8 + 0.4 * sin(2 * M_PI * (day_of_year - 80) / 365.0); // Peak in summer
    
    // More realistic sunrise/sunset times based on season
    double sunrise = 6.0 + 2.0 * cos(2 * M_PI * day_of_year / 365.0);
    double sunset = 18.0 + 2.0 * cos(2 * M_PI * day_of_year / 365.0);
    
    if (hour_of_day < sunrise || hour_of_day > sunset) {
        return 0.0;
    }

    // Improved solar curve - more realistic bell curve
    double day_length = sunset - sunrise;
    double noon = (sunrise + sunset) / 2.0;
    double time_from_noon = hour_of_day - noon;
    double normalized_time = 2.0 * time_from_noon / day_length; // -1 to 1
    
    // Bell curve with sharper edges
    double solar_factor = exp(-2.0 * normalized_time * normalized_time);
    
    // Check for weather change
    if (now - last_weather_change_time > config.sim_params.weather_change_interval_seconds) {
        std::uniform_int_distribution<> dis(0, config.sim_params.weather_models.size() - 1);
        current_weather_model_index = dis(rng);
        last_weather_change_time = now;
        std::cout << "Weather changed to: " << config.sim_params.weather_models[current_weather_model_index].name << std::endl;
    }

    double weather_multiplier = config.sim_params.weather_models[current_weather_model_index].power_multiplier;
    
    // Add some random variation (clouds, etc.)
    std::uniform_real_distribution<> variation_dis(0.9, 1.1);
    double random_variation = variation_dis(rng);
    
    return config.sim_params.max_power_watts * solar_factor * seasonal_factor * weather_multiplier * random_variation;
}

double SimulationEngine::calculateGridVoltage(int phase) {
    // Simulate realistic grid voltage variations
    std::uniform_real_distribution<> voltage_dis(-config.sim_params.voltage_variation_percent, 
                                                  config.sim_params.voltage_variation_percent);
    double variation = voltage_dis(rng) / 100.0;
    
    // Add phase offset for 3-phase system
    double phase_offset = phase * 120.0 * M_PI / 180.0; // 120° phase shift
    double voltage_ripple = 0.005 * sin(time(0) * 2 * M_PI + phase_offset); // Small ripple
    
    return config.sim_params.grid_voltage_nominal * (1.0 + variation + voltage_ripple);
}

double SimulationEngine::calculateGridFrequency() {
    std::uniform_real_distribution<> freq_dis(-config.sim_params.frequency_variation_hz, 
                                              config.sim_params.frequency_variation_hz);
    return config.sim_params.grid_frequency_nominal + freq_dis(rng);
}

void SimulationEngine::updateSimulationState() {
    time_t current_time = time(0);
    struct tm *ltm = localtime(&current_time);
    
    // Handle daily yield reset
    if (last_daily_reset_day != ltm->tm_mday && ltm->tm_hour == config.sim_params.daily_yield_reset_hour) {
        data_model->setLogicalValue(30517, (uint64_t)0); // Reset daily yield
        last_daily_reset_day = ltm->tm_mday;
        std::cout << "Daily yield reset at midnight" << std::endl;
    }

    // 1. Check for client commands
    auto op_state_val = data_model->getLogicalValue(40009);
    auto ack_error_val = data_model->getLogicalValue(40011);

    uint32_t op_state = op_state_val ? std::get<uint32_t>(*op_state_val) : 295;
    uint32_t ack_error = ack_error_val ? std::get<uint32_t>(*ack_error_val) : 0;
    
    // 2. Enhanced state machine
    if (ack_error == 26 && current_state == DeviceState::ERROR) {
        current_state = DeviceState::OK; // Resume operation after error ack
        data_model->setLogicalValue(40011, (uint32_t)0);
        std::cout << "Error acknowledged, resuming operation" << std::endl;
    } else if (op_state == 381) { // Stop command
        current_state = DeviceState::OFF;
        std::cout << "Stop command received" << std::endl;
    } else if (current_state != DeviceState::ERROR) {
        // Realistic fault injection based on temperature and power
        double current_power = calculatePowerOutput();
        double power_ratio = current_power / config.sim_params.max_power_watts;
        double temp_factor = 1.0 + power_ratio * 2.0; // Higher power = higher fault risk
        
        std::uniform_real_distribution<> fault_dis(0, 100);
        if (fault_dis(rng) < config.sim_params.fault_probability_percent * temp_factor) {
            current_state = DeviceState::ERROR;
            std::cout << "Random fault injected" << std::endl;
        } else if (op_state == 295) {
            current_state = DeviceState::OK;
        }
    }

    // 3. Calculate realistic dynamic values
    double ac_power_total = 0.0;
    double dc_power_total = 0.0;
    uint32_t device_status_enum = 303; // Off
    uint32_t detailed_op_status = 381; // Stop
    uint32_t grid_contactor_enum = 311; // Open
    uint32_t derating_status = 302; // No derating
    uint32_t event_number = 0;

    // Calculate grid parameters with realistic variations
    double voltage_l1 = calculateGridVoltage(0);
    double voltage_l2 = calculateGridVoltage(1);
    double voltage_l3 = calculateGridVoltage(2);
    double grid_frequency = calculateGridFrequency();
    
    double power_factor = 0.99; // Slightly less than perfect

    if (current_state == DeviceState::OK) {
        ac_power_total = calculatePowerOutput();
        if (ac_power_total > 100) { // Minimum power threshold
            double efficiency = config.sim_params.efficiency_percent / 100.0;
            dc_power_total = ac_power_total / efficiency;
            device_status_enum = 307; // OK
            detailed_op_status = 295; // MPP
            grid_contactor_enum = 51; // Closed
            
            // Calculate realistic power factor based on load
            power_factor = 0.98 + 0.02 * (ac_power_total / config.sim_params.max_power_watts);
            
            // Temperature-based derating
            double power_ratio = ac_power_total / config.sim_params.max_power_watts;
            double weather_temp_factor = config.sim_params.weather_models[current_weather_model_index].temp_increase_factor;
            double internal_temp = config.sim_params.ambient_temp_celsius + 
                                 (config.sim_params.max_internal_temp_celsius - config.sim_params.ambient_temp_celsius) * 
                                 power_ratio * weather_temp_factor;
            
            if (internal_temp > 65.0) {
                derating_status = 557; // Temperature derating
                double derating_factor = 1.0 - (internal_temp - 65.0) / 20.0; // Linear derating
                ac_power_total *= std::max(0.5, derating_factor);
                dc_power_total = ac_power_total / efficiency;
                std::cout << "Temperature derating active: " << internal_temp << "°C" << std::endl;
            }
        } else {
            // Inverter is OK but no significant power (night/early morning)
            device_status_enum = 307; // OK
            detailed_op_status = 1393; // Waiting for DC start conditions
            grid_contactor_enum = 311; // Open at night
        }
    } else if (current_state == DeviceState::ERROR) {
        device_status_enum = 35; // Error
        detailed_op_status = 1392; // Error
        grid_contactor_enum = 311; // Open
        event_number = 1001 + (rand() % 10); // Various error codes
    }
    
    // 4. Calculate balanced 3-phase power distribution
    double ac_power_l1 = ac_power_total / 3.0;
    double ac_power_l2 = ac_power_total / 3.0;
    double ac_power_l3 = ac_power_total / 3.0;
    
    // Add small imbalance for realism
    std::uniform_real_distribution<> imbalance_dis(-0.02, 0.02);
    ac_power_l1 *= (1.0 + imbalance_dis(rng));
    ac_power_l2 *= (1.0 + imbalance_dis(rng));
    ac_power_l3 = ac_power_total - ac_power_l1 - ac_power_l2; // Ensure total is conserved
    
    // 5. Enhanced DC string simulation with realistic I-V curves
    double dc_power_1 = dc_power_total * 0.52; // Slight imbalance between strings
    double dc_power_2 = dc_power_total * 0.48;
    
    // Realistic voltage calculation based on load
    double dc_voltage_1 = 0.0, dc_voltage_2 = 0.0;
    double dc_current_1 = 0.0, dc_current_2 = 0.0;
    
    if (dc_power_1 > 0) {
        // Realistic I-V curve simulation
        double normalized_power = dc_power_1 / (config.sim_params.max_power_watts * 0.52);
        dc_voltage_1 = 350.0 + normalized_power * 250.0; // 350V to 600V range
        dc_current_1 = dc_power_1 / dc_voltage_1;
    }
    
    if (dc_power_2 > 0) {
        double normalized_power = dc_power_2 / (config.sim_params.max_power_watts * 0.48);
        dc_voltage_2 = 360.0 + normalized_power * 240.0; // Slightly different characteristics
        dc_current_2 = dc_power_2 / dc_voltage_2;
    }
    
    // 6. Calculate reactive and apparent power
    double reactive_power = ac_power_total * tan(acos(power_factor));
    double apparent_power = ac_power_total / power_factor;
    
    // 7. Update energy accumulators
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

    auto grid_connections_val = data_model->getLogicalValue(30599);
    uint32_t grid_connections = grid_connections_val ? std::get<uint32_t>(*grid_connections_val) : 0;

    if (ac_power_total > 100) { // Only count when actually producing
        feed_time += static_cast<uint64_t>(seconds_per_tick);
        double energy_wh = ac_power_total * (seconds_per_tick / 3600.0);
        total_yield += static_cast<uint64_t>(energy_wh);
        daily_yield += static_cast<uint64_t>(energy_wh);
        
        // Realistic grid connection counting
        static int connection_timer = 0;
        if (grid_contactor_enum == 51) {
            connection_timer++;
            if (connection_timer > 3600) { // Every hour of operation
                grid_connections++;
                connection_timer = 0;
            }
        }
    }
    
    // 8. Calculate temperature with environmental factors
    double power_ratio = (ac_power_total > 0) ? (ac_power_total / config.sim_params.max_power_watts) : 0.0;
    double weather_temp_factor = config.sim_params.weather_models[current_weather_model_index].temp_increase_factor;
    double internal_temp = config.sim_params.ambient_temp_celsius + 
                          (config.sim_params.max_internal_temp_celsius - config.sim_params.ambient_temp_celsius) * 
                          power_ratio * weather_temp_factor;

    // Add thermal inertia
    static double prev_temp = config.sim_params.ambient_temp_celsius;
    internal_temp = prev_temp * 0.9 + internal_temp * 0.1; // Smooth temperature changes
    prev_temp = internal_temp;
    
    // 9. Write all values to the data model
    // Status registers
    data_model->setLogicalValue(30197, event_number);
    data_model->setLogicalValue(30201, device_status_enum);
    data_model->setLogicalValue(30217, grid_contactor_enum);
    data_model->setLogicalValue(30219, derating_status);
    data_model->setLogicalValue(30229, static_cast<uint32_t>(current_time));
    data_model->setLogicalValue(40029, detailed_op_status);
    
    // Power and energy registers
    data_model->setLogicalValue(30775, static_cast<int32_t>(ac_power_total));
    data_model->setLogicalValue(30777, static_cast<int32_t>(ac_power_l1));
    data_model->setLogicalValue(30779, static_cast<int32_t>(ac_power_l2));
    data_model->setLogicalValue(30781, static_cast<int32_t>(ac_power_l3));
    data_model->setLogicalValue(30805, static_cast<int32_t>(reactive_power));
    data_model->setLogicalValue(30813, static_cast<int32_t>(apparent_power));
    
    // DC registers with proper scaling
    data_model->setLogicalValue(30769, static_cast<int32_t>(dc_current_1 * 1000)); // FIX3
    data_model->setLogicalValue(30771, static_cast<int32_t>(dc_voltage_1 * 100)); // FIX2
    data_model->setLogicalValue(30773, static_cast<int32_t>(dc_power_1));
    data_model->setLogicalValue(30957, static_cast<int32_t>(dc_current_2 * 1000)); // FIX3
    data_model->setLogicalValue(30959, static_cast<int32_t>(dc_voltage_2 * 100)); // FIX2
    data_model->setLogicalValue(30961, static_cast<int32_t>(dc_power_2));
    
    // AC grid parameters with realistic variations
    data_model->setLogicalValue(30783, static_cast<uint32_t>(voltage_l1 * 100)); // FIX2
    data_model->setLogicalValue(30785, static_cast<uint32_t>(voltage_l2 * 100)); // FIX2
    data_model->setLogicalValue(30787, static_cast<uint32_t>(voltage_l3 * 100)); // FIX2
    data_model->setLogicalValue(30797, static_cast<uint32_t>((ac_power_l1 / voltage_l1) * 1000)); // FIX3
    data_model->setLogicalValue(30803, static_cast<uint32_t>(grid_frequency * 100)); // FIX2
    data_model->setLogicalValue(30949, static_cast<uint32_t>(power_factor * 1000)); // FIX3
    
    // Temperature
    data_model->setLogicalValue(30953, static_cast<int32_t>(internal_temp * 10)); // TEMP is FIX1
    
    // Counters
    data_model->setLogicalValue(30521, op_time);
    data_model->setLogicalValue(30525, feed_time);
    data_model->setLogicalValue(30513, total_yield);
    data_model->setLogicalValue(30517, daily_yield);
    data_model->setLogicalValue(30599, grid_connections);
}