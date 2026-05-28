// #include "sim/core/simulator.hpp"
// #include "sim/agents/agent_interface.hpp"
// #include "sim/agents/random_agent.hpp"
// #include <gtest/gtest.h>
// #include <memory>
// #include <random>

// using namespace sigint_sim;

// // ---------------------------------------------------------------------------
// // Minimal test agent that returns a pre‑defined action.
// // It also records which world snapshots it received, for verification.
// // ---------------------------------------------------------------------------
// class TestAgent : public IAgent {
// public:
//     Action next_action;
//     // Records of what was passed to selectAction
//     std::vector<NodeState> last_my_state;
//     std::vector<std::vector<NodeState>> last_all_states;
//     std::vector<std::vector<LinkState>> last_links;

//     Action selectAction(const NodeState& my_state,
//                         const std::vector<NodeState>& all_states,
//                         const std::vector<LinkState>& links,
//                         const EventLog& /*recent_events*/) override
//     {
//         last_my_state.push_back(my_state);
//         last_all_states.push_back(all_states);
//         last_links.push_back(links);
//         return next_action;
//     }

//     std::string agentType() const override { return "TestAgent"; }
// };

// // ---------------------------------------------------------------------------
// // Test Fixture
// // ---------------------------------------------------------------------------
// class SimulatorTest : public ::testing::Test {
// protected:
//     void SetUp() override {
//         // Create a minimal config with 2 nodes and one bidirectional link.
//         config_.seed = 42;
//         config_.timestep = 0.1;
//         config_.duration = 2.0;             // 20 steps possible
//         config_.topology_edges = {
//             {NodeId{0}, NodeId{1}},
//             {NodeId{1}, NodeId{0}}
//         };

//         // Channel: always up, deterministic SNR
//         BlockFadingChannel::Params ch_params;
//         ch_params.availability = 1.0;
//         ch_params.avg_snr_db = 30.0;
//         ch_params.snr_std_db = 0.0;
//         ch_params.outage_snr_db = -999.0;
//         ch_params.bandwidth = 10e6;
//         config_.channel = std::make_shared<BlockFadingChannel>(ch_params);

//         simulator_ = std::make_unique<Simulator>(config_);

//         // Create test agents for both nodes
//         // Create agents and save raw pointers for test inspection
//         auto agent0 = std::make_shared<TestAgent>();
//         auto agent1 = std::make_shared<TestAgent>();

//         agent0_ = agent0.get();   // raw non-owning pointer
//         agent1_ = agent1.get();

//         simulator_->setAgent(NodeId{0}, std::move(agent0));  // ownership transferred
//         simulator_->setAgent(NodeId{1}, std::move(agent1));
//     }

//     Simulator::Config config_;
//     std::unique_ptr<Simulator> simulator_;
//     TestAgent* agent0_;   // convenience pointers after ownership transferred
//     TestAgent* agent1_;
// };

// // ---------------------------------------------------------------------------
// // 1. Construction & assertions (death tests)
// // ---------------------------------------------------------------------------

// TEST(SimulatorDeathTest, Constructor_ZeroTimestep_Asserts) {
//     Simulator::Config cfg;
//     cfg.timestep = 0.0;
//     cfg.duration = 1.0;
//     cfg.channel = std::make_shared<BlockFadingChannel>(BlockFadingChannel::Params{});
//     EXPECT_DEATH(Simulator sim(cfg), "Timestep must be positive");
// }

// TEST(SimulatorDeathTest, Constructor_ZeroDuration_Asserts) {
//     Simulator::Config cfg;
//     cfg.timestep = 0.1;
//     cfg.duration = 0.0;
//     cfg.channel = std::make_shared<BlockFadingChannel>(BlockFadingChannel::Params{});
//     EXPECT_DEATH(Simulator sim(cfg), "Duration must be positive");
// }

// TEST(SimulatorDeathTest, Constructor_NullChannel_Asserts) {
//     Simulator::Config cfg;
//     cfg.timestep = 0.1;
//     cfg.duration = 1.0;
//     // channel left as nullptr
//     EXPECT_DEATH(Simulator sim(cfg), "Channel model must not be null");
// }

// // ---------------------------------------------------------------------------
// // 2. Basic stepping – events logged, time advances, agents called
// // ---------------------------------------------------------------------------

// TEST_F(SimulatorTest, Step_AdvancesTimeAndCallsAgent) {
//     // Initially agent has default action (silent)
//     simulator_->step();

//     EXPECT_DOUBLE_EQ(simulator_->currentTime(), 0.1);
//     // Each agent should have been called once
//     EXPECT_EQ(agent0_->last_my_state.size(), 1);
//     EXPECT_EQ(agent1_->last_my_state.size(), 1);
//     // Events: no signal, no transmission → only ScanStarted? Not necessarily, depends on action.
// }

// TEST_F(SimulatorTest, Step_TenSteps_TimeIncrementsCorrectly) {
//     for (int i = 0; i < 10; ++i) {
//         simulator_->step();
//     }
//     EXPECT_DOUBLE_EQ(simulator_->currentTime(), 1.0);
//     EXPECT_FALSE(simulator_->isFinished());
// }

// // ---------------------------------------------------------------------------
// // 3. Determinism – same seed + same agents → identical event log
// // ---------------------------------------------------------------------------

// TEST_F(SimulatorTest, Determinism_SameConfigAndAgents_IdenticalEventLog) {
//     Simulator::Config cfg;
//     cfg.seed = 12345;
//     cfg.timestep = 0.1;
//     cfg.duration = 2.0;
//     cfg.topology_edges = {
//         {NodeId{0}, NodeId{1}},
//         {NodeId{1}, NodeId{0}}
//     };

//     BlockFadingChannel::Params ch_params;
//     ch_params.availability = 0.8;
//     ch_params.avg_snr_db = 20.0;
//     ch_params.snr_std_db = 5.0;
//     ch_params.outage_snr_db = -10.0;
//     ch_params.bandwidth = 10e6;
//     cfg.channel = std::make_shared<BlockFadingChannel>(ch_params);

//     Simulator simA(cfg);
//     Simulator simB(cfg);

//     // Attach identical RandomAgents to both sims
//     for (int i = 0; i < 2; ++i) {
//         auto ag = std::make_unique<RandomAgent>(12345 + i * 100);
//         simA.setAgent(NodeId{i}, std::move(ag));
//     }
//     for (int i = 0; i < 2; ++i) {
//         auto ag = std::make_unique<RandomAgent>(12345 + i * 100);
//         simB.setAgent(NodeId{i}, std::move(ag));
//     }

//     // Run 15 steps on both
//     for (int step = 0; step < 15; ++step) {
//         simA.step();
//         simB.step();
//     }

//     const auto& logA = simA.getEventLog();
//     const auto& logB = simB.getEventLog();
//     ASSERT_EQ(logA.size(), logB.size());
//     for (size_t i = 0; i < logA.size(); ++i) {
//         EXPECT_EQ(logA[i].type, logB[i].type);
//         EXPECT_DOUBLE_EQ(logA[i].time, logB[i].time);
//         EXPECT_EQ(logA[i].node_id, logB[i].node_id);
//         EXPECT_EQ(logA[i].params.size(), logB[i].params.size());
//     }
// }

// // ---------------------------------------------------------------------------
// // 5. Sensing: signal detection is recorded when node scans at correct frequency
// // ---------------------------------------------------------------------------

// TEST_F(SimulatorTest, ScanAction_DetectsSignalWhenInBand) {
//     // Agent 0 scans at 2.4 GHz (where we've hardcoded an emitter for t<5s)
//     Action scan_action;
//     scan_action.scan_params = RFParams{2.4e9, 20e6, 40.0, 40e6};
//     agent0_->next_action = scan_action;
//     agent1_->next_action = Action{}; // silent

//     simulator_->step(); // time 0.0 → 0.1 (t < 5s)

//     const auto& events = simulator_->getEventLog();
//     bool signal_detected = false;
//     for (const auto& ev : events) {
//         if (ev.type == "SignalDetected" && ev.node_id == 0) {
//             signal_detected = true;
//             break;
//         }
//     }
//     EXPECT_TRUE(signal_detected);
// }

// TEST_F(SimulatorTest, ScanAction_NoSignalWhenOutOfBand) {
//     // Agent 0 scans at 1 GHz (outside the emitter's band)
//     Action scan_action;
//     scan_action.scan_params = RFParams{1.0e9, 20e6, 40.0, 40e6};
//     agent0_->next_action = scan_action;
//     agent1_->next_action = Action{}; // silent

//     simulator_->step();

//     const auto& events = simulator_->getEventLog();
//     bool signal_detected = false;
//     for (const auto& ev : events) {
//         if (ev.type == "SignalDetected") {
//             signal_detected = true;
//             break;
//         }
//     }
//     EXPECT_FALSE(signal_detected);
// }

// TEST_F(SimulatorTest, NoScan_NoSignalDetected) {
//     // No scanning action → no signal detection event
//     agent0_->next_action = Action{}; // idle
//     simulator_->step();

//     const auto& events = simulator_->getEventLog();
//     for (const auto& ev : events) {
//         EXPECT_NE(ev.type, "SignalDetected");
//     }
// }

// // ---------------------------------------------------------------------------
// // 6. Processing: when an agent processes a signal, buffer drains (integration)
// // ---------------------------------------------------------------------------

// TEST_F(SimulatorTest, ProcessAfterScan_DrainsBufferAndAddsEvent) {
//     // Step 1: scan to detect signal
//     Action scan;
//     scan.scan_params = RFParams{2.4e9, 20e6, 40.0, 40e6};
//     agent0_->next_action = scan;
//     simulator_->step();   // time 0.1

//     // Verify signal detected and node has buffer
//     auto node_states = simulator_->getNodeStates();
//     EXPECT_EQ(node_states[0].buffer_size, 1);

//     // Step 2: process the signal
//     Action proc;
//     proc.process_task_ids.push_back(0);
//     agent0_->next_action = proc;
//     simulator_->step();   // time 0.2

//     // the new SignalProcessor, a DETECT task produces 1 byte of output data (a classification result)
//     node_states = simulator_->getNodeStates();
//     EXPECT_EQ(node_states[0].buffer_size, 1);
//     // Energy should have increased
//     EXPECT_GT(node_states[0].energy_used, 0.0);
// }

// // ---------------------------------------------------------------------------
// // 7. Finished condition & reset
// // ---------------------------------------------------------------------------

// TEST_F(SimulatorTest, IsFinished_WhenTimeReachesDuration) {
//     // Advance until just at duration
//     while (simulator_->currentTime() < config_.duration) {
//         simulator_->step();
//     }
//     EXPECT_TRUE(simulator_->isFinished());
//     // Stepping again should do nothing
//     size_t event_count_before = simulator_->getEventLog().size();
//     simulator_->step();
//     EXPECT_EQ(simulator_->getEventLog().size(), event_count_before);
// }

// TEST_F(SimulatorTest, Reset_ClearsLogAndResetsTime) {
//     // Force an action that produces a “SignalDetected” event
//     Action scan;
//     scan.scan_params = RFParams{2.4e9, 20e6, 40.0, 40e6};
//     agent0_->next_action = scan;
//     simulator_->step();

//     // there should be at least one event
//     ASSERT_GT(simulator_->getEventLog().size(), 0);
//     ASSERT_GT(simulator_->currentTime(), 0.0);

//     simulator_->reset(999);

//     EXPECT_EQ(simulator_->getEventLog().size(), 0);
//     EXPECT_DOUBLE_EQ(simulator_->currentTime(), 0.0);
//     EXPECT_FALSE(simulator_->isFinished());
// }