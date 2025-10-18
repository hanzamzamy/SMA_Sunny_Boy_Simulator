#include "modbus_server.hpp"
#include <iostream>
#include <unistd.hpp>
#include <sys/socket.hpp>

// Custom callback implementations
int ModbusServer::read_registers_callback(modbus_t *ctx, int addr, int nb, uint16_t *dest, void *data) {
    SafeDataModel* model = static_cast<SafeDataModel*>(data);
    // std::cout << "CALLBACK: Read request for addr=" << addr << ", nb=" << nb << std::endl;
    for (int i = 0; i < nb; ++i) {
        if (!model->getRegisterValue(addr + i, dest[i])) {
            // Handle error: return an exception or a default value
            // For simplicity, we return 0 for non-existent registers
            dest[i] = 0;
        }
    }
    return nb;
}

int ModbusServer::write_registers_callback(modbus_t *ctx, int addr, int nb, const uint16_t *src, void *data) {
    SafeDataModel* model = static_cast<SafeDataModel*>(data);
    // std::cout << "CALLBACK: Write request for addr=" << addr << ", nb=" << nb << std::endl;
    for (int i = 0; i < nb; ++i) {
        if (!model->setRegisterValue(addr + i, src[i])) {
            // Handle write error, e.g., to a read-only register
            // The model itself prints a warning. We could return an exception here.
            modbus_reply_exception(ctx, nullptr, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            return -1;
        }
    }
    return nb;
}


ModbusServer::ModbusServer(std::shared_ptr<SafeDataModel> model, int id)
    : data_model(model), unit_id(id), port(0), ctx(nullptr), mb_mapping(nullptr), running(false), server_socket(-1) {}

ModbusServer::~ModbusServer() {
    stop();
}

bool ModbusServer::start(int p) {
    if (running) return true;
    port = p;

    ctx = modbus_new_tcp("127.0.0.1", port);
    if (ctx == nullptr) {
        std::cerr << "Failed to create modbus context: " << modbus_strerror(errno) << std::endl;
        return false;
    }

    // Set custom callbacks
    if (modbus_set_hooks(ctx, MODBUS_HOOK_READ_REGISTERS, (modbus_hook_read_registers_t)read_registers_callback, data_model.get()) != 0 ||
        modbus_set_hooks(ctx, MODBUS_HOOK_WRITE_REGISTERS, (modbus_hook_write_registers_t)write_registers_callback, data_model.get()) != 0) {
        std::cerr << "Failed to set custom hooks: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        return false;
    }

    // This mapping is now just a placeholder because we use callbacks.
    // However, libmodbus requires a valid mapping to be allocated.
    mb_mapping = modbus_mapping_new(0, 0, 0, 0); 
    if (mb_mapping == nullptr) {
        std::cerr << "Failed to allocate modbus mapping: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        return false;
    }
    
    modbus_set_slave(ctx, unit_id);

    server_socket = modbus_tcp_listen(ctx, 1);
    if (server_socket == -1) {
        std::cerr << "Unable to listen on TCP port " << port << ": " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        modbus_mapping_free(mb_mapping);
        return false;
    }

    running = true;
    server_thread = std::thread(&ModbusServer::run, this);
    return true;
}

void ModbusServer::stop() {
    if (!running) return;
    running = false;
    
    // The modbus_receive can block. We need to unblock it by closing the socket.
    if(server_socket != -1){
        shutdown(server_socket, SHUT_RDWR);
        close(server_socket);
        server_socket = -1;
    }

    if (server_thread.joinable()) {
        server_thread.join();
    }

    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = nullptr;
    }
    if (mb_mapping) {
        modbus_mapping_free(mb_mapping);
        mb_mapping = nullptr;
    }
}

void ModbusServer::run() {
    std::cout << "Modbus server thread started." << std::endl;
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    
    while (running) {
        int rc = modbus_tcp_accept(ctx, &server_socket);
        if (rc == -1) {
            if (running) { // Avoid error message on graceful shutdown
                std::cerr << "Modbus accept failed: " << modbus_strerror(errno) << std::endl;
            }
            continue;
        }

        while(running) {
             rc = modbus_receive(ctx, query);
             if (rc > 0) {
                 // The hooks are called automatically inside modbus_reply
                 modbus_reply(ctx, query, rc, mb_mapping);
             } else if (rc == -1) {
                 // Connection closed or error
                 break;
             }
        }
        modbus_close(ctx);
    }
    std::cout << "Modbus server thread stopped." << std::endl;
}
