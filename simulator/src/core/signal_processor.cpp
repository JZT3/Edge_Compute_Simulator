#include "../include/sim/core/signal_processor.hpp"
#include "../include/sim/core/sim_config.hpp"
#include <random>
#include <cmath>
#include <cassert>
#include <complex>

namespace sigint_sim {

TaskResult SignalProcessor::execute(const SignalTask& task,
                                    double snr_linear,
                                    const HardwareProfile& profile,
                                    std::mt19937& rng) {
    TaskResult res;
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    // ---- GFLOPS consumed ----
    double complexity_factor = 1.0;
    switch (task.type) {
        case SignalTaskType::SCAN:    complexity_factor = SP_COMPLEXITY_SCAN; break;
        case SignalTaskType::DETECT:  complexity_factor = SP_COMPLEXITY_DETECT; break;
        case SignalTaskType::RAW_IQ:  complexity_factor = SP_COMPLEXITY_RAW_IQ; break;
        case SignalTaskType::TDOA:    complexity_factor = SP_COMPLEXITY_TDOA; break;
        case SignalTaskType::FEATURE: complexity_factor = SP_COMPLEXITY_FEATURE; break;
    }
    res.gflops_consumed = complexity_factor *
                          (task.bandwidth_hz / 1e6) *
                          task.duration_s *
                          (profile.fft_gflops_per_sec / 10.0);
    res.energy_joules = res.gflops_consumed * SP_JOULES_PER_GFLOP;

    // ---- Detection probability ----
    double P_d = SP_DETECTION_THRESHOLD_FACTOR - std::exp(-snr_linear);
    bool detected = (unit(rng) < P_d);
    res.signal_present = detected;
    res.confidence = detected ? P_d : (1.0 - P_d);

    // ---- Data output size ----
    switch (task.type) {
        case SignalTaskType::SCAN:
            res.data = { SP_OUTPUT_SIZE_SCAN };
            break;
        case SignalTaskType::DETECT:
            res.data.assign(1, detected ? 1 : 0);
            break;
        case SignalTaskType::RAW_IQ: {
            size_t num_samples = static_cast<size_t>(task.duration_s * task.bandwidth_hz * 2);
            res.data.resize(num_samples * sizeof(std::complex<float>));   // now complete
            break;
        }
        case SignalTaskType::TDOA:
            res.data.assign(1, detected ? 1 : 0);
            res.time_error_std_s = 1.0 / (2.0 * M_PI * task.bandwidth_hz *
                                          std::sqrt(2.0 * snr_linear));
            break;
        case SignalTaskType::FEATURE:
            res.data.resize(SP_OUTPUT_SIZE_FEATURE);
            break;
    }
    return res;
}

} // namespace sigint_sim