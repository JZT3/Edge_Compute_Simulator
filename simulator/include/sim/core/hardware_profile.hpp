#pragma once
#include <string>

namespace sigint_sim {

struct HardwareProfile {
    std::string device_string = "virtual";
    double noise_figure_dB = 10.0;
    double tx_power_dBm = 10.0;
    double frequency_accuracy_ppm = 1.0;
};

} // namespace sigint_sim