#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace sigint_sim {

struct StepMetrics {
    double cumulative_intelligence = 0.0;   // total mission value so far
    int    lpd_violations = 0;            // transmissions that exceeded LPD budget
    int    transmissions_attempted = 0;
    int    transmissions_succeeded = 0;
    double average_snr = 0.0;
    double step_execution_time_us = 0.0;  // microseconds
    double raw_intelligence = 0.0;       // sum of emitter priorities only
    double lpd_penalty_total = 0.0;      // sum of LPD penalties
};

// Optionally, store history inside the Simulator
using MetricsHistory = std::vector<StepMetrics>;

} // namespace sigint_sim