#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include "safe_data_model.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <modbus/modbus.hpp>

/**
 * @class ModbusServer
 * @brief Handles Modbus TCP communication in a dedicated thread.
 *
 * This class uses libmodbus to create a Modbus TCP server. It listens for
 * client requests, interacts with the SafeDataModel to read or write register
 * data, and sends responses back to the client.
 */
class ModbusServer {
public:
    /**
     * @brief Constructor for the ModbusServer.
     * @param data_model A shared pointer to the thread-safe data model.
     * @param unit_id The Modbus unit ID for the server.
     */
    ModbusServer(std::shared_ptr<SafeDataModel> data_model, int unit_id);

    /**
     * @brief Destructor, ensures the server is stopped.
     */
    ~ModbusServer();

    /**
     * @brief Starts the Modbus server listening loop in a new thread.
     * @param port The TCP port to listen on.
     * @return True on success, false on failure.
     */
    bool start(int port);

    /**
     * @brief Stops the Modbus server.
     */
    void stop();

private:
    /**
     * @brief The main loop of the Modbus server thread.
     */
    void run();

    std::shared_ptr<SafeDataModel> data_model;
    int unit_id;
    int port;
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;
    std::thread server_thread;
    std::atomic<bool> running;
    int server_socket;

    /**
     * @brief Custom callback function to handle read requests.
     */
    static int read_registers_callback(modbus_t *ctx, int addr, int nb, uint16_t *dest, void *data);

    /**
     * @brief Custom callback function to handle write requests.
     */
    static int write_registers_callback(modbus_t *ctx, int addr, int nb, const uint16_t *src, void *data);
};

#endif // MODBUS_SERVER_H
