#include "modbus_server.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

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

    // Create a large enough mapping to handle all possible registers
    mb_mapping = modbus_mapping_new(0, 0, 65536, 0); 
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
            if (running) {
                std::cerr << "Modbus accept failed: " << modbus_strerror(errno) << std::endl;
            }
            continue;
        }

        while(running) {
            rc = modbus_receive(ctx, query);
            if (rc > 0) {
                // Handle the request manually by parsing the query
                int function_code = query[7];
                
                if (function_code == 0x03 || function_code == 0x04) { // Read Holding/Input Registers
                    int addr = (query[8] << 8) | query[9];
                    int nb = (query[10] << 8) | query[11];
                    
                    // Update mapping from data model
                    for (int i = 0; i < nb; ++i) {
                        uint16_t value;
                        if (data_model->getRegisterValue(addr + i, value)) {
                            mb_mapping->tab_registers[addr + i] = value;
                        }
                    }
                } else if (function_code == 0x06 || function_code == 0x10) { // Write Single/Multiple Registers
                    // We'll handle writes after modbus_reply
                }
                
                int reply_rc = modbus_reply(ctx, query, rc, mb_mapping);
                
                // Handle writes after reply
                if (reply_rc > 0 && (function_code == 0x06 || function_code == 0x10)) {
                    if (function_code == 0x06) { // Write Single Register
                        int addr = (query[8] << 8) | query[9];
                        uint16_t value = (query[10] << 8) | query[11];
                        data_model->setRegisterValue(addr, value);
                    } else if (function_code == 0x10) { // Write Multiple Registers
                        int addr = (query[8] << 8) | query[9];
                        int nb = (query[10] << 8) | query[11];
                        for (int i = 0; i < nb; ++i) {
                            uint16_t value = (query[13 + i*2] << 8) | query[14 + i*2];
                            data_model->setRegisterValue(addr + i, value);
                        }
                    }
                }
            } else if (rc == -1) {
                break;
            }
        }
        modbus_close(ctx);
    }
    std::cout << "Modbus server thread stopped." << std::endl;
}
