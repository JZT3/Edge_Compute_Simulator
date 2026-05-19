#include "../include/sim/core/simulator.hpp"
#include "../include/sim/agents/random_agent.hpp"
#include "../include/sim/logging/logger.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // --- Setup logging (writes to "sim_run.log") ---
    sigint_sim::Logger::init("sim_run.log");
    auto logger = sigint_sim::Logger::get();
    logger->info("Headless run started.");

    // --- Parse minimal command-line args (for demo, use hardcoded config) ---
    uint64_t seed = (argc > 1) ? std::stoull(argv[1]) : 42;
    int num_steps = (argc > 2) ? std::stoi(argv[2]) : 50;

    // --- Build simulator configuration ---
    sigint_sim::Simulator::Config config;
    config.seed = seed;
    config.timestep = 0.1;
    config.duration = static_cast<double>(num_steps) * config.timestep;

    // Two nodes: node 0 and node 1, with one directed link from 0→1.
    config.topology_edges = {
        {sigint_sim::NodeId{0}, sigint_sim::NodeId{1}},
        {sigint_sim::NodeId{1}, sigint_sim::NodeId{0}}   // bidirectional
    };

    // Use a block-fading channel with high availability
    sigint_sim::BlockFadingChannel::Params ch_params;
    ch_params.availability = 0.9;
    ch_params.avg_snr_db = 25.0;
    ch_params.snr_std_db = 3.0;
    ch_params.outage_snr_db = -5.0;
    ch_params.bandwidth = 10e6;
    config.channel = std::make_shared<sigint_sim::BlockFadingChannel>(ch_params);

    // --- Create simulator ---
    sigint_sim::Simulator sim(config);

    // --- Attach random agents to each node ---
    for (int i = 0; i < 2; ++i) {
        auto agent = std::make_unique<sigint_sim::RandomAgent>(seed + i * 1000);
        sim.setAgent(sigint_sim::NodeId{i}, std::move(agent));
    }

    // --- Run simulation steps ---
    for (int step = 0; step < num_steps && !sim.isFinished(); ++step) {
        sim.step();
    }

    // --- Print summary ---
    const auto& events = sim.getEventLog();
    std::cout << "Simulation finished.\n";
    std::cout << "  Steps taken: " << sim.currentTime() / config.timestep << "\n";
    std::cout << "  Total events logged: " << events.size() << "\n";

    int tx_success = 0, tx_fail = 0, scan_count = 0;
    for (const auto& ev : events) {
        if (ev.type == "TransmissionSuccess") tx_success++;
        else if (ev.type == "TransmissionFail") tx_fail++;
        else if (ev.type == "ScanStarted") scan_count++;
    }
    std::cout << "  Transmission successes: " << tx_success << "\n";
    std::cout << "  Transmission failures:  " << tx_fail << "\n";
    std::cout << "  Scans started:          " << scan_count << "\n";

    // Demonstrate deterministic replay
    sigint_sim::Simulator sim2(config);
    for (int i = 0; i < 2; ++i)
        sim2.setAgent(sigint_sim::NodeId{i}, std::make_unique<sigint_sim::RandomAgent>(seed + i * 1000));
    for (int step = 0; step < num_steps; ++step) sim2.step();
    std::cout << "  Determinism check: " 
              << (events.size() == sim2.getEventLog().size() ? "PASS" : "FAIL")
              << "\n";

    return 0;
}