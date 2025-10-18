#include "digital_twin.hpp"
#include "config_loader.hpp"
#include "safe_data_model.hpp"
#include "simulation_engine.hpp"
#include "modbus_server.hpp"
#include <iostream>
#include <csignal>
#include <memory>
#include <chrono>
#include <thread>

// Global pointers for signal handling
std::unique_ptr<SimulationEngine> g_sim_engine_ptr;
std::unique_ptr<ModbusServer> g_modbus_server_ptr;

/**
 * @brief Signal handler for graceful shutdown (e.g., on Ctrl+C).
 * @param signum The signal number received.
 */
void signal_handler(int signum) {
    std::cout << "\nCaught signal " << signum << ". Shutting down gracefully..." << std::endl;
    if (g_modbus_server_ptr) {
        g_modbus_server_ptr->stop();
    }
    if (g_sim_engine_ptr) {
        g_sim_engine_ptr->stop();
    }
}

int main(int argc, char* argv[]) {
    // --- 1. Load Configuration ---
    std::string config_file = "sma_inverter_profile.yaml";
    if (argc > 1) {
        config_file = argv[1];
    }
    std::cout << "Loading configuration from: " << config_file << std::endl;

    Config config;
    try {
        config = ConfigLoader::loadConfig(config_file);
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "Configuration loaded successfully." << std::endl;
    std::cout << "Simulating device with Serial Number: " << config.identity.serial_number << std::endl;

    // --- 2. Initialize Shared Data Model ---
    auto shared_data_model = std::make_shared<SafeDataModel>();
    shared_data_model->initialize(config.registers);
    std::cout << "Shared data model initialized." << std::endl;

    // --- 3. Initialize and Start Simulation Engine ---
    g_sim_engine_ptr = std::make_unique<SimulationEngine>(shared_data_model, config);
    g_sim_engine_ptr->start();
    std::cout << "Simulation engine started in a background thread." << std::endl;

    // --- 4. Initialize and Start Modbus Server ---
    const int modbus_port = 1502; // Use a non-privileged port
    g_modbus_server_ptr = std::make_unique<ModbusServer>(shared_data_model, config.identity.unit_id);
    if (!g_modbus_server_ptr->start(modbus_port)) {
        std::cerr << "Failed to start Modbus server." << std::endl;
        g_sim_engine_ptr->stop();
        return 1;
    }
    std::cout << "Modbus TCP server started on port " << modbus_port << "." << std::endl;

    // --- 5. Set up Signal Handler and Wait ---
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "\nDigital Twin is running. Press Ctrl+C to exit." << std::endl;

    // The main thread can simply wait here. The signal handler will trigger the shutdown.
    // The destructor of the unique_ptrs will handle joining the threads.
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }


    return 0; // This part is unreachable due to the infinite loop and signal handler
}
