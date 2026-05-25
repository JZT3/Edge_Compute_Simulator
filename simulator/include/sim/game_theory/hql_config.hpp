#pragma once
#include <cstdint>

namespace sigint_sim::game_theory {

struct HystereticQLearnerConfig {
    // Optuna optimized hyper parameters tools/tune_optuna.py
    double alpha = 0.0413; // previously 0.1
    double beta  = 0.004;  // previously 0.01
    double gamma = 0.8864; // previously 0.95
    double lambda = 0.8116; // previously 0.8
    double epsilon_start = 0.998; // preeviously 1.0
    double epsilon_end   = 0.02; // previously 0.05
    int    epsilon_decay_steps = 19600; //previously 10000
    double mu = 1.8;                // LPD penalty per transmission (previously 0.5)
    double max_weight = 1000.0;     // divergence threshold -> SARSA 
    int    trace_length = 491;      // replay buffer capacity (previously 500)
    int    reliability_window = 10;
    int    gamma_loc_window = 200;
};

} // namespace