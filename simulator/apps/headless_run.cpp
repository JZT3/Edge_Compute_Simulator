#include "../include/sim/core/simulator.hpp"
#include "../include/sim/agents/random_agent.hpp"
#include "../include/sim/logging/logger.hpp"
#include "../include/sim/core/sim_config.hpp"
#include <iostream>
#include <string>
#include <sys/stat.h>   // for mkdir
#include <cstdlib>

using namespace sigint_sim;

int main(int argc, char* argv[]) {
    // --- Setup logging (writes to "sim_run.log") ---
    mkdir("output", 0755);
    Logger::init(DEFAULT_LOG_PATH);

    auto logger = Logger::get();
    logger->info("Headless run started.");

    // --- Parse minimal command-line args (for demo, use hardcoded config) ---
    uint64_t seed = (argc > 1) ? std::stoull(argv[1]) : 42;
    int num_steps = (argc > 2) ? std::stoi(argv[2]) : 50;

    // --- Build simulator configuration ---
    Simulator::Config config;
    config.seed = seed;
    config.timestep = 0.1;
    config.duration = static_cast<double>(num_steps) * config.timestep;

    // Two nodes: node 0 and node 1, with one directed link from 0→1.
    config.topology_edges = {
        {NodeId{0}, NodeId{1}},
        {NodeId{1}, NodeId{0}}   // bidirectional
    };

    // Use a block-fading channel with high availability
    BlockFadingChannel::Params ch_params;
    ch_params.availability = 0.9;
    ch_params.avg_snr_db = 25.0;
    ch_params.snr_std_db = 3.0;
    ch_params.outage_snr_db = -5.0;
    ch_params.bandwidth = 10e6;
    config.channel = std::make_shared<BlockFadingChannel>(ch_params);

    // --- Create simulator ---
    Simulator sim(config);

    // --- Attach random agents to each node ---
    for (int i = 0; i < 2; ++i) {
        auto agent = std::make_unique<RandomAgent>(seed + i * 1000);
        sim.setAgent(NodeId{i}, std::move(agent));
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
    Simulator sim2(config);
    for (int i = 0; i < 2; ++i)
        sim2.setAgent(NodeId{i}, std::make_unique<RandomAgent>(seed + i * 1000));
    for (int step = 0; step < num_steps; ++step) sim2.step();
    std::cout << "  Determinism check: " 
              << (events.size() == sim2.getEventLog().size() ? "PASS" : "FAIL")
              << "\n";

    return 0;
}
