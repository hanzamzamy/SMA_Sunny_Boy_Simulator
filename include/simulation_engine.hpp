#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include "safe_data_model.hpp"
#include "digital_twin.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <random>

class SimulationEngine {
public:
    SimulationEngine(std::shared_ptr<SafeDataModel> data_model, const Config& config);
    void start();
    void stop();

private:
    void run();
    void updateSimulationState();
    double calculatePowerOutput();
    double calculateGridVoltage(int phase);
    double calculateGridFrequency();

    std::shared_ptr<SafeDataModel> data_model;
    const Config& config;
    std::thread simulation_thread;
    std::atomic<bool> running;

    // Simulation state variables
    enum class DeviceState { OFF, OK, WARNING, ERROR };
    DeviceState current_state;
    int current_weather_model_index;
    time_t last_weather_change_time;
    int last_daily_reset_day;
    
    // Random number generation
    std::mt19937 rng;
};

#endif // SIMULATION_ENGINE_H