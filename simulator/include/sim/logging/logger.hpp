#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>

namespace sigint_sim {

class Logger {
public:
    // Initialise the global logger to write to a file.
    static void init(const std::string& filepath);

    // Get the global logger instance.
    static std::shared_ptr<spdlog::logger> get();

private:
    static std::shared_ptr<spdlog::logger> instance_;
};

} // namespace sigint_sim