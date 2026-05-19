#include "../include/sim/logging/logger.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include "spdlog/sinks/stdout_color_sinks.h"
#include <memory>

namespace sigint_sim {

std::shared_ptr<spdlog::logger> Logger::instance_ = nullptr;

void Logger::init(const std::string& filepath) {
    instance_ = spdlog::basic_logger_mt("sigint_sim", filepath);
    instance_->set_level(spdlog::level::info);
    instance_->flush_on(spdlog::level::info);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!instance_) {
        // Fallback to stderr if not yet initialised
        instance_ = spdlog::stderr_color_mt("sigint_sim_fallback");
    }
    return instance_;
}

} // namespace sigint_sim