#pragma once
enum class DeviceType { RTL_SDR, HACKRF, LIME_SDR, USRP_B2XX, USRP_X3XX, VIRTUAL };

struct HardwareProfile {
    DeviceType type = DeviceType::VIRTUAL;
    double noise_figure_dB = 10.0;
    double tx_power_dBm = 10.0;
    double frequency_accuracy_ppm = 1.0;
    // Compute capabilities
    double fft_gflops_per_sec = 1.0;   // GFLOPS available for detection
    double memory_mib = 1024.0;
};