#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include "safe_data_model.h"
#include "digital_twin.h"
#include <thread>
#include <atomic>

/**
 * @class SimulationEngine
 * @brief Runs the dynamic simulation logic in a background thread.
 *
 * This class is responsible for updating the device's state over time,
 * simulating a diurnal solar cycle, weather effects, temperature changes,
 * and responding to control commands written by a Modbus client.
 */
class SimulationEngine {
public:
    /**
     * @brief Constructor for the SimulationEngine.
     * @param data_model A shared pointer to the thread-safe data model.
     * @param config The global configuration object.
     */
    SimulationEngine(std::shared_ptr<SafeDataModel> data_model, const Config& config);

    /**
     * @brief Starts the simulation engine in a new thread.
     */
    void start();

    /**
     * @brief Signals the simulation engine to stop and waits for the thread to join.
     */
    void stop();

private:
    /**
     * @brief The main loop of the simulation thread.
     */
    void run();

    /**
     * @brief Updates all dynamic register values for a single simulation tick.
     */
    void updateSimulationState();

    /**
     * @brief Calculates power output based on time of day and weather.
     * @return The calculated power in watts.
     */
    double calculatePowerOutput();

    std::shared_ptr<SafeDataModel> data_model;
    const Config& config;
    std::thread simulation_thread;
    std::atomic<bool> running;

    // Simulation state variables
    enum class DeviceState { OFF, OK, WARNING, ERROR };
    DeviceState current_state;
    int current_weather_model_index;
    time_t last_weather_change_time;
};

#endif // SIMULATION_ENGINE_H
