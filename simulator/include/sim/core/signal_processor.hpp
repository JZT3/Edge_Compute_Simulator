#pragma once
#include "hardware_profile.hpp"
#include "sim_config.hpp"
#include <cstdint>
#include <vector>
#include <random>

namespace sigint_sim {

enum class SignalTaskType { SCAN, DETECT, RAW_IQ, TDOA, FEATURE };

struct SignalTask {
    SignalTaskType type;
    double center_freq_hz = 2.4e9;
    double bandwidth_hz = DEFAULT_BANDWIDTH_HZ;
    double duration_s = 0.001;      // observation time
};

struct TaskResult {
    bool signal_present = false;
    double confidence = 0.0;            // 0..1
    std::vector<uint8_t> data;          // compressed output
    double time_error_std_s = 0.0;      // TDOA precision
    double gflops_consumed = 0.0;
    double energy_joules = 0.0;
};

class SignalProcessor {
public:
    // Execute a task given current SNR (linear) and the node's profile.
    // snr_linear = received signal power / noise power (unitless).
    static TaskResult execute(const SignalTask& task,
                              double snr_linear,
                              const HardwareProfile& profile,
                              std::mt19937& rng);
};

} // namespace sigint_sim