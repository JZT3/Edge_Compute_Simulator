#pragma once
#include "sim_config.hpp"
#include <string>

namespace sigint_sim {

enum class DeviceType { RTL_SDR, HACKRF, LIME_SDR, USRP_B2XX, USRP_X3XX, VIRTUAL };

struct HardwareProfile {
    DeviceType type = DeviceType::VIRTUAL;
    double noise_figure_dB        = DEFAULT_NOISE_FIGURE_DB;
    double tx_power_dBm           = DEFAULT_TX_POWER_DBM;
    double frequency_accuracy_ppm = DEFAULT_FREQ_ACCURACY_PPM;

    // Compute capabilities
    double fft_gflops_per_sec = DEFAULT_FFT_GFLOPS_PER_SEC;   // GFLOPS available for detection
    double memory_mib         = DEFAULT_MEMORY_MIB;

    // Convert the enum to a human-readable string.
    [[nodiscard]] std::string deviceTypeToString() const {
        switch (type) {
            case DeviceType::RTL_SDR:   return   "RTL_SDR";
            case DeviceType::HACKRF:    return    "HackRF";
            case DeviceType::LIME_SDR:  return  "LIME_SDR";
            case DeviceType::USRP_B2XX: return "USRP_B2XX";
            case DeviceType::USRP_X3XX: return "USRP_X3XX"; 
            default: return "VIRTUAL";
        }
    }
};

} // namespace sigint_sim