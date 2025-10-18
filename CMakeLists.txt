cmake_minimum_required(VERSION 3.10)
project(SunnyBoyDigitalTwin CXX C)

# --- Compiler and Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Find Dependencies ---
find_package(yaml-cpp REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)

# --- Include Directories ---
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBMODBUS_INCLUDE_DIRS}
    ${yaml-cpp_INCLUDE_DIRS}
)

# --- Add Executable ---
add_executable(sunny_boy_digital_twin
    src/main.cpp
    src/config_loader.cpp
    src/simulation_engine.cpp
    src/modbus_server.cpp
    src/safe_data_model.cpp
)

# --- Link Libraries ---
target_link_libraries(sunny_boy_digital_twin
    PRIVATE
    Threads::Threads
    ${LIBMODBUS_LIBRARIES}
    yaml-cpp::yaml-cpp
)

# --- Set RPATH for runtime library search path ---
set_target_properties(sunny_boy_digital_twin PROPERTIES
    INSTALL_RPATH "/usr/local/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# --- Copy YAML Configuration File ---
add_custom_command(
    TARGET sunny_boy_digital_twin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/sma_inverter_profile.yaml
            $<TARGET_FILE_DIR:sunny_boy_digital_twin>/sma_inverter_profile.yaml
    COMMENT "Copying sma_inverter_profile.yaml to the output directory"
)

# --- Install Executable and Configuration File ---
install(TARGETS sunny_boy_digital_twin RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sma_inverter_profile.yaml DESTINATION etc)