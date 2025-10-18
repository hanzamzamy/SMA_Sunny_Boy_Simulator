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

    // Create a mapping large enough to handle addresses up to 65535
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

// Convert Modbus protocol address (0-based from client) to our internal config address
uint16_t ModbusServer::protocolToInternal(uint16_t protocol_addr) {
    // Modbus client sends 0-based addresses for holding registers
    // Our config uses notation like 30775, 40009, etc.
    // 
    // For holding registers (function 3/6/16):
    // - Protocol address 0-9998 maps to config addresses 30001-39999
    // - Protocol address 775 should map to config address 30775
    // 
    // For input registers (function 4):
    // - Protocol address 0-9998 maps to config addresses 30001-39999
    //
    // Since most SMA addresses are in 30xxx range, we add 30000
    if (protocol_addr < 10000) {
        return protocol_addr + 30000;
    } else if (protocol_addr >= 10000 && protocol_addr < 20000) {
        // Handle 4xxxx addresses (like 40009, 40011)
        return protocol_addr + 30000;  // 10009 -> 40009
    }
    
    // For addresses that are already in full format, return as-is
    return protocol_addr;
}

// Convert our internal config address to Modbus protocol address  
uint16_t ModbusServer::internalToProtocol(uint16_t internal_addr) {
    if (internal_addr >= 30000 && internal_addr < 40000) {
        return internal_addr - 30000;
    } else if (internal_addr >= 40000 && internal_addr < 50000) {
        return internal_addr - 30000;  // 40009 -> 10009
    }
    return internal_addr;
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

        std::cout << "Client connected" << std::endl;

        while(running) {
            rc = modbus_receive(ctx, query);
            if (rc > 0) {
                // Parse the request
                int function_code = query[7];
                int addr = (query[8] << 8) | query[9];
                int nb = (query[10] << 8) | query[11];
                
                std::cout << "Received function " << function_code << " for protocol addr " << addr << " nb " << nb << std::endl;
                
                if (function_code == 0x03 || function_code == 0x04) { // Read Holding/Input Registers
                    // Convert protocol address to internal config address
                    uint16_t internal_addr = protocolToInternal(addr);
                    std::cout << "Translated to internal addr " << internal_addr << std::endl;
                    
                    // Update mapping from data model before responding
                    bool all_valid = true;
                    for (int i = 0; i < nb; ++i) {
                        uint16_t value;
                        if (data_model->getRegisterValue(internal_addr + i, value)) {
                            // Store in the protocol address space for libmodbus
                            mb_mapping->tab_registers[addr + i] = value;
                            std::cout << "Read internal reg " << (internal_addr + i) << " = " << value << " -> protocol addr " << (addr + i) << std::endl;
                        } else {
                            std::cout << "Internal register " << (internal_addr + i) << " not found" << std::endl;
                            all_valid = false;
                        }
                    }
                    
                    if (!all_valid) {
                        // Send exception response for illegal data address
                        modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
                        continue;
                    }
                }
                
                int reply_rc = modbus_reply(ctx, query, rc, mb_mapping);
                
                // Handle writes after reply
                if (reply_rc > 0 && (function_code == 0x06 || function_code == 0x10)) {
                    uint16_t internal_addr = protocolToInternal(addr);
                    std::cout << "Write to protocol addr " << addr << " -> internal addr " << internal_addr << std::endl;
                    
                    if (function_code == 0x06) { // Write Single Register
                        uint16_t value = (query[10] << 8) | query[11];
                        if (!data_model->setRegisterValue(internal_addr, value)) {
                            std::cout << "Failed to write internal register " << internal_addr << std::endl;
                        } else {
                            std::cout << "Wrote internal reg " << internal_addr << " = " << value << std::endl;
                        }
                    } else if (function_code == 0x10) { // Write Multiple Registers
                        bool all_written = true;
                        for (int i = 0; i < nb; ++i) {
                            uint16_t value = (query[13 + i*2] << 8) | query[14 + i*2];
                            if (!data_model->setRegisterValue(internal_addr + i, value)) {
                                all_written = false;
                                std::cout << "Failed to write internal register " << (internal_addr + i) << std::endl;
                            } else {
                                std::cout << "Wrote internal reg " << (internal_addr + i) << " = " << value << std::endl;
                            }
                        }
                    }
                }
            } else if (rc == -1) {
                std::cout << "Client disconnected" << std::endl;
                break;
            }
        }
        modbus_close(ctx);
    }
    std::cout << "Modbus server thread stopped." << std::endl;
}