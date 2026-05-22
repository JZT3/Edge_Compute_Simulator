#include "sim/core/simulator.hpp"
#include "sim/core/sample_processing_channel.hpp"
#include "sim/core/hardware_profile.hpp"
#include "sim/agents/random_agent.hpp"
#include <gtest/gtest.h>

using namespace sigint_sim;  

TEST(PhyRegression, ReplayProducesIdenticalLog) {
    // Build a config with known positions, profiles, and channel parameters
    Simulator::Config cfg;
    cfg.seed = 123;
    cfg.timestep = 0.1;
    cfg.duration = 1.0;
    cfg.topology_edges = {{NodeId{0}, NodeId{1}}};

    // Place nodes 1 m apart → active link after a transmission
    cfg.node_positions[0] = {0.0, 0.0};
    cfg.node_positions[1] = {1.0, 0.0};

    // Provide realistic hardware profiles (20 dBm TX, 10 dB NF)
    HardwareProfile prof;
    prof.noise_figure_dB = 10.0;
    prof.tx_power_dBm    = 20.0;
    cfg.node_profiles[0] = prof;
    cfg.node_profiles[1] = prof;

    // Channel parameters
    SampleProcessingChannel::Params chp;
    chp.bandwidth_Hz = 1e6;          // 1 MHz
    cfg.channel = std::make_shared<SampleProcessingChannel>(chp);

    // Function to run a simulation and capture the event log
    auto run_sim = [&]() {
        Simulator sim(cfg);
        // Use deterministic seeds for the agents
        sim.setAgent(NodeId{0}, std::make_unique<RandomAgent>(cfg.seed));
        sim.setAgent(NodeId{1}, std::make_unique<RandomAgent>(cfg.seed + 1000));
        for (int i = 0; i < 10; ++i) sim.step();
        return sim.getEventLog();
    };

    // Two identical runs must produce identical event logs
    auto log1 = run_sim();
    auto log2 = run_sim();

    ASSERT_EQ(log1.size(), log2.size());
    for (size_t i = 0; i < log1.size(); ++i) {
        EXPECT_EQ(log1[i].type,  log2[i].type);
        EXPECT_DOUBLE_EQ(log1[i].time, log2[i].time);
        EXPECT_EQ(log1[i].node_id, log2[i].node_id);
        EXPECT_EQ(log1[i].params, log2[i].params);   // extra check
    }
}