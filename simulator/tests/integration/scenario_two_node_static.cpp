#include "sim/core/simulator.hpp"
#include "sim/agents/random_agent.hpp"
#include "sim/core/channel_model.hpp"
#include <gtest/gtest.h>
#include <string>
#include <set>

using namespace sigint_sim;

// ---------------------------------------------------------------------------
// Integration Test: 2-Node Static Network
//
// Scenario description:
//   - Node 0: weak CPU, VHF/UHF band, close to an emitter.
//   - Node 1: strong FPGA, microwave band, no direct emitter contact.
//   - One bidirectional link between them with moderate availability.
//
// Currently both nodes use RandomAgents, so we only verify that the
// simulation infrastructure runs correctly and produces meaningful events.
// When the PotentialGameAgent is ready, this test will be extended to
// assert that node 0 offloads processing to node 1 when the link is up,
// leading to higher cumulative intelligence value than local processing.
// ---------------------------------------------------------------------------

class TwoNodeStaticIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Build a config matching the scenario
        cfg_.seed = 12345;
        cfg_.timestep = 0.1;
        cfg_.duration = 5.0;                 // 50 steps max
        cfg_.topology_edges = {
            {NodeId{0}, NodeId{1}},
            {NodeId{1}, NodeId{0}}
        };

        BlockFadingChannel::Params ch_params;
        ch_params.availability = 0.9;        // high, but not perfect
        ch_params.avg_snr_db = 20.0;
        ch_params.snr_std_db = 5.0;
        ch_params.outage_snr_db = -10.0;
        ch_params.bandwidth = 10e6;
        cfg_.channel = std::make_shared<BlockFadingChannel>(ch_params);

        simulator_ = std::make_unique<Simulator>(cfg_);

        // Attach RandomAgents with deterministic seeds
        for (int i = 0; i < 2; ++i) {
            auto agent = std::make_unique<RandomAgent>(cfg_.seed + i * 1000);
            simulator_->setAgent(NodeId{i}, std::move(agent));
        }
    }

    Simulator::Config cfg_;
    std::unique_ptr<Simulator> simulator_;
};

// ---------------------------------------------------------------------------
// 1. Smoke test: the simulation runs without crashing and logs events.
// ---------------------------------------------------------------------------
TEST_F(TwoNodeStaticIntegrationTest, Smoke_RunsWithoutCrash) {
    // Run half the duration
    for (int i = 0; i < 25; ++i) {
        simulator_->step();
        ASSERT_FALSE(simulator_->isFinished());
    }
    // After 25 steps, we should have some events
    const auto& events = simulator_->getEventLog();
    EXPECT_GT(events.size(), 0) << "Expected at least one event after 25 steps";
}

// ---------------------------------------------------------------------------
// 2. Determinism: two runs with the same seeds produce identical event logs.
// ---------------------------------------------------------------------------
TEST_F(TwoNodeStaticIntegrationTest, Determinism_IdenticalLogs) {
    // Run the current simulator for a few steps and capture the log
    for (int i = 0; i < 10; ++i) simulator_->step();
    const auto log1 = simulator_->getEventLog();

    // Build a second, identical simulator
    Simulator sim2(cfg_);
    for (int i = 0; i < 2; ++i) {
        auto agent = std::make_unique<RandomAgent>(cfg_.seed + i * 1000);
        sim2.setAgent(NodeId{i}, std::move(agent));
    }
    for (int i = 0; i < 10; ++i) sim2.step();
    const auto log2 = sim2.getEventLog();

    ASSERT_EQ(log1.size(), log2.size());
    for (size_t j = 0; j < log1.size(); ++j) {
        EXPECT_EQ(log1[j].type, log2[j].type);
        EXPECT_DOUBLE_EQ(log1[j].time, log2[j].time);
        EXPECT_EQ(log1[j].node_id, log2[j].node_id);
    }
}
