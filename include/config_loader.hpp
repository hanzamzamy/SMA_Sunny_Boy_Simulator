#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include "digital_twin.h"
#include <string>

/**
 * @class ConfigLoader
 * @brief Parses the YAML configuration file to populate the Config structure.
 *
 * This class uses the yaml-cpp library to read the device profile and
 * simulation parameters from the specified file.
 */
class ConfigLoader {
public:
    /**
     * @brief Loads and parses the YAML configuration file.
     * @param filename The path to the YAML configuration file.
     * @return A Config object populated with data from the file.
     * @throw std::runtime_error if the file cannot be opened or parsed.
     */
    static Config loadConfig(const std::string& filename);
};

#endif // CONFIG_LOADER_H
