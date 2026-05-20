#include "sim/core/simulator.hpp"
#include "sim/agents/agent_interface.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <random>

using namespace sigint_sim;

// ---------------------------------------------------------------------------
// Minimal test agent that returns a pre‑defined action.
// It also records which world snapshots it received, for verification.
// ---------------------------------------------------------------------------
class TestAgent : public IAgent {
public:
    Action next_action;
    // Records of what was passed to selectAction
    std::vector<NodeState> last_my_state;
    std::vector<std::vector<NodeState>> last_all_states;
    std::vector<std::vector<LinkState>> last_links;

    Action selectAction(const NodeState& my_state,
                        const std::vector<NodeState>& all_states,
                        const std::vector<LinkState>& links,
                        const EventLog& /*recent_events*/) override
    {
        last_my_state.push_back(my_state);
        last_all_states.push_back(all_states);
        last_links.push_back(links);
        return next_action;
    }

    std::string agentType() const override { return "TestAgent"; }
};

// ---------------------------------------------------------------------------
// Test Fixture
// ---------------------------------------------------------------------------
class SimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a minimal config with 2 nodes and one bidirectional link.
        config_.seed = 42;
        config_.timestep = 0.1;
        config_.duration = 2.0;             // 20 steps possible
        config_.topology_edges = {
            {NodeId{0}, NodeId{1}},
            {NodeId{1}, NodeId{0}}
        };

        // Channel: always up, deterministic SNR
        BlockFadingChannel::Params ch_params;
        ch_params.availability = 1.0;
        ch_params.avg_snr_db = 30.0;
        ch_params.snr_std_db = 0.0;
        ch_params.outage_snr_db = -999.0;
        ch_params.bandwidth = 10e6;
        config_.channel = std::make_shared<BlockFadingChannel>(ch_params);

        simulator_ = std::make_unique<Simulator>(config_);

        // Create test agents for both nodes
        // Create agents and save raw pointers for test inspection
        auto agent0 = std::make_unique<TestAgent>();
        auto agent1 = std::make_unique<TestAgent>();

        agent0_ = agent0.get();   // raw non-owning pointer
        agent1_ = agent1.get();

        simulator_->setAgent(NodeId{0}, std::move(agent0));  // ownership transferred
        simulator_->setAgent(NodeId{1}, std::move(agent1));
    }

    Simulator::Config config_;
    std::unique_ptr<Simulator> simulator_;
    TestAgent* agent0_;   // convenience pointers after ownership transferred
    TestAgent* agent1_;
};

// ---------------------------------------------------------------------------
// 1. Construction & assertions (death tests)
// ---------------------------------------------------------------------------

TEST(SimulatorDeathTest, Constructor_ZeroTimestep_Asserts) {
    Simulator::Config cfg;
    cfg.timestep = 0.0;
    cfg.duration = 1.0;
    cfg.channel = std::make_shared<BlockFadingChannel>(BlockFadingChannel::Params{});
    EXPECT_DEATH(Simulator sim(cfg), "Timestep must be positive");
}

TEST(SimulatorDeathTest, Constructor_ZeroDuration_Asserts) {
    Simulator::Config cfg;
    cfg.timestep = 0.1;
    cfg.duration = 0.0;
    cfg.channel = std::make_shared<BlockFadingChannel>(BlockFadingChannel::Params{});
    EXPECT_DEATH(Simulator sim(cfg), "Duration must be positive");
}

TEST(SimulatorDeathTest, Constructor_NullChannel_Asserts) {
    Simulator::Config cfg;
    cfg.timestep = 0.1;
    cfg.duration = 1.0;
    // channel left as nullptr
    EXPECT_DEATH(Simulator sim(cfg), "Channel model must not be null");
}

// ---------------------------------------------------------------------------
// 2. Basic stepping – events logged, time advances, agents called
// ---------------------------------------------------------------------------

TEST_F(SimulatorTest, Step_AdvancesTimeAndCallsAgent) {
    // Initially agent has default action (silent)
    simulator_->step();

    EXPECT_DOUBLE_EQ(simulator_->currentTime(), 0.1);
    // Each agent should have been called once
    EXPECT_EQ(agent0_->last_my_state.size(), 1);
    EXPECT_EQ(agent1_->last_my_state.size(), 1);
    // Events: no signal, no transmission → only ScanStarted? Not necessarily, depends on action.
}

TEST_F(SimulatorTest, Step_TenSteps_TimeIncrementsCorrectly) {
    for (int i = 0; i < 10; ++i) {
        simulator_->step();
    }
    EXPECT_DOUBLE_EQ(simulator_->currentTime(), 1.0);
    EXPECT_FALSE(simulator_->isFinished());
}

