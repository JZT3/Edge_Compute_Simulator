#include "sim/core/simulator.hpp"
#include "sim/core/sample_processing_channel.hpp"
#include "sim/core/hardware_profile.hpp"
#include "sim/agents/random_agent.hpp"
#include "sim/agents/agent_interface.hpp"
#include <gtest/gtest.h>
#include <string>
#include <set>

using namespace sigint_sim;

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
class TransmitAgent : public IAgent {
public:
    explicit TransmitAgent(int target) : target_(target) {}
    Action selectAction(const NodeState&,
                        const std::vector<NodeState>&,
                        const std::vector<LinkState>&,
                        const EventLog&) override {
        Action act;
        act.burst = Action::Burst{target_, 0, 20.0, {}};
        return act;
    }
    std::string agentType() const override { return "TransmitAgent"; }
private:
    int target_;
};

// ---------------------------------------------------------------------------
// Unified integration test fixture – uses SampleProcessingChannel
// ---------------------------------------------------------------------------
class TwoNodeIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg_.seed = 12345;
        cfg_.timestep = 0.1;
        cfg_.duration = 5.0;                // 50 steps max
        cfg_.topology_edges = {
            {NodeId{0}, NodeId{1}},
            {NodeId{1}, NodeId{0}}
        };

        // SampleProcessingChannel (distance‑aware, sample‑driven)
        SampleProcessingChannel::Params chp;
        chp.bandwidth_Hz = 1e6;
        cfg_.channel = std::make_shared<SampleProcessingChannel>(chp);

        // Nodes placed 1 m apart → very strong link
        cfg_.node_positions[0] = {0.0, 0.0};
        cfg_.node_positions[1] = {1.0, 0.0};

        // Identical hardware profiles (20 dBm TX, 10 dB NF)
        HardwareProfile prof;
        prof.noise_figure_dB = 10.0;
        prof.tx_power_dBm    = 20.0;
        cfg_.node_profiles[0] = prof;
        cfg_.node_profiles[1] = prof;

        simulator_ = std::make_unique<Simulator>(cfg_);

        // By default, attach RandomAgents (used by most tests)
        for (int i = 0; i < 2; ++i) {
            auto agent = std::make_unique<RandomAgent>(cfg_.seed + i * 1000);
            simulator_->setAgent(NodeId{i}, std::move(agent));
        }
    }

    Simulator::Config cfg_;
    std::unique_ptr<Simulator> simulator_;
};

// ---------------------------------------------------------------------------
// Original general tests (adapted to the new channel)
// ---------------------------------------------------------------------------

TEST_F(TwoNodeIntegrationTest, Smoke_RunsWithoutCrash) {
    for (int i = 0; i < 25; ++i) {
        simulator_->step();
        ASSERT_FALSE(simulator_->isFinished());
    }
    const auto& events = simulator_->getEventLog();
    EXPECT_GT(events.size(), 0) << "Expected at least one event after 25 steps";
}

TEST_F(TwoNodeIntegrationTest, Determinism_IdenticalLogs) {
    for (int i = 0; i < 10; ++i) simulator_->step();
    const auto log1 = simulator_->getEventLog();

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

TEST_F(TwoNodeIntegrationTest, EventDiversity_AllTypesAppear) {
    while (!simulator_->isFinished()) {
        simulator_->step();
    }

    const auto& events = simulator_->getEventLog();
    std::set<std::string> types;
    for (const auto& ev : events) {
        types.insert(ev.type);
    }

    EXPECT_TRUE(types.count("SignalDetected") > 0);
    EXPECT_TRUE(types.count("TransmissionSuccess") > 0 ||
                types.count("TransmissionFail") > 0);
}

TEST_F(TwoNodeIntegrationTest, NodeStates_BothNodesPresent) {
    auto states = simulator_->getNodeStates();
    ASSERT_EQ(states.size(), 2);
    EXPECT_EQ(states[0].id, NodeId{0});
    EXPECT_EQ(states[1].id, NodeId{1});
    EXPECT_FALSE(states[0].name.empty());
    EXPECT_FALSE(states[1].name.empty());
}

// ---------------------------------------------------------------------------
// New physical‑layer tests (transmission‑driven)
// ---------------------------------------------------------------------------

TEST_F(TwoNodeIntegrationTest, CloseNodes_HaveActiveLink) {
    // Node 0 transmits every step, node 1 stays silent
    simulator_->setAgent(NodeId{0}, std::make_unique<TransmitAgent>(1));
    class SilentAgent : public IAgent {
    public:
        Action selectAction(const NodeState&, const std::vector<NodeState>&,
                            const std::vector<LinkState>&, const EventLog&) override {
            return Action{};
        }
        std::string agentType() const override { return "Silent"; }
    };
    simulator_->setAgent(NodeId{1}, std::make_unique<SilentAgent>());

    // Step enough times for transmission → channel processing → logging
    bool saw_success = false;
    for (int i = 0; i < 5; ++i) {
        simulator_->step();
        const auto& events = simulator_->getEventLog();
        for (const auto& ev : events) {
            if (ev.type == "TransmissionSuccess" && ev.node_id == 0) {
                saw_success = true;
                break;
            }
        }
        if (saw_success) break;
    }
    EXPECT_TRUE(saw_success) << "Expected at least one TransmissionSuccess event";
}

TEST_F(TwoNodeIntegrationTest, FarNodes_HaveInactiveLink) {
    // Move node 1 far away and restart
    cfg_.node_positions[1] = {1000.0, 0.0};
    Simulator sim(cfg_);

    sim.setAgent(NodeId{0}, std::make_unique<TransmitAgent>(1));
    class SilentAgent : public IAgent {
    public:
        Action selectAction(const NodeState&, const std::vector<NodeState>&,
                            const std::vector<LinkState>&, const EventLog&) override {
            return Action{};
        }
        std::string agentType() const override { return "Silent"; }
    };
    sim.setAgent(NodeId{1}, std::make_unique<SilentAgent>());

    sim.step();
    auto links = sim.getLinkStates();
    ASSERT_EQ(links.size(), 2);
    EXPECT_FALSE(links[0].active) << "SNR = " << links[0].snr << " dB";
}

TEST_F(TwoNodeIntegrationTest, Determinism_SameSeedSameEventLog) {
    // Use RandomAgents as in the general determinism test
    Simulator simA(cfg_);
    Simulator simB(cfg_);
    for (int i = 0; i < 2; ++i) {
        simA.setAgent(NodeId{i}, std::make_unique<RandomAgent>(cfg_.seed + i * 100));
        simB.setAgent(NodeId{i}, std::make_unique<RandomAgent>(cfg_.seed + i * 100));
    }
    for (int step = 0; step < 5; ++step) {
        simA.step();
        simB.step();
    }
    const auto& logA = simA.getEventLog();
    const auto& logB = simB.getEventLog();
    ASSERT_EQ(logA.size(), logB.size());
    for (size_t k = 0; k < logA.size(); ++k) {
        EXPECT_EQ(logA[k].type, logB[k].type);
        EXPECT_DOUBLE_EQ(logA[k].time, logB[k].time);
    }
}