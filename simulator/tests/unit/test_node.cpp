#include "sim/core/node.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace sigint_sim;

// ---------------------------------------------------------------------------
// Test Fixture: provides a baseline SDRNode and common constants.
// ---------------------------------------------------------------------------
class SDRNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Valid minimal node: name = "TestNode", single band, 1 GFLOPS.
        valid_node_ = std::make_unique<SDRNode>(NodeId{0}, "TestNode",
                                               ComputeCapability{1e9, 0.0, 1ULL << 30},
                                               std::vector<Frequency>{2.4e9});
    }

    void TearDown() override {
        valid_node_.reset();
    }

    std::unique_ptr<SDRNode> valid_node_;
};

// ---------------------------------------------------------------------------
// 1. Construction – equivalence partitioning (valid / invalid)
// ---------------------------------------------------------------------------

TEST_F(SDRNodeTest, Constructor_ValidParameters_InitialStateIsIdle) {
    // Valid node created in fixture. Verify initial state.
    const auto state = valid_node_->getState();
    EXPECT_EQ(state.mode, NodeMode::IDLE);
    EXPECT_EQ(state.name, "TestNode");
    EXPECT_DOUBLE_EQ(state.compute.fft_ops_per_sec, 1e9);
    EXPECT_EQ(state.buffer_size, 0);
    EXPECT_DOUBLE_EQ(state.energy_used, 0.0);
    // Neighbor beliefs map starts empty (not tested further in MVP).
}

// Death tests: verify assertions on invalid construction.
TEST(SDRNodeDeathTest, Constructor_EmptyName_Asserts) {
    // The node name must not be empty – we expect an assertion failure.
    EXPECT_DEATH(
        {
            SDRNode node(NodeId{1}, "", ComputeCapability{1e9,0,1ULL<<30}, {2.4e9});
        },
        "Node name must not be empty"
    );
}

TEST(SDRNodeDeathTest, Constructor_EmptyBands_Asserts) {
    EXPECT_DEATH(
        {
            SDRNode node(NodeId{2}, "Valid", ComputeCapability{1e9,0,1ULL<<30}, {});
        },
        "Node must support at least one frequency band"
    );
}

// ---------------------------------------------------------------------------
// 2. applyAction – state‑based testing of mode transitions
// ---------------------------------------------------------------------------

TEST_F(SDRNodeTest, ApplyAction_ScanAction_SetsModeToScanAndRFParams) {
    Action act;
    act.scan_params = RFParams{2.41e9, 10e6, 30.0, 20e6};
    valid_node_->applyAction(act);

    const auto state = valid_node_->getState();
    EXPECT_EQ(state.mode, NodeMode::SCAN);
    EXPECT_DOUBLE_EQ(state.current_rf.center_freq, 2.41e9);
    EXPECT_DOUBLE_EQ(state.current_rf.bandwidth, 10e6);
}

TEST_F(SDRNodeTest, ApplyAction_ProcessAction_SetsModeToProcess) {
    Action act;
    act.process_task_ids.push_back(42);
    valid_node_->applyAction(act);

    EXPECT_EQ(valid_node_->getState().mode, NodeMode::PROCESS);
}

TEST_F(SDRNodeTest, ApplyAction_TransmitAction_SetsModeToTransmit) {
    Action act;
    act.burst = Action::Burst{1, 0, 20.0, {}};
    valid_node_->applyAction(act);

    EXPECT_EQ(valid_node_->getState().mode, NodeMode::TRANSMIT);
}

TEST_F(SDRNodeTest, ApplyAction_AllFieldsEmpty_ResultsInIdle) {
    Action act; // default – nothing set
    valid_node_->applyAction(act);

    EXPECT_EQ(valid_node_->getState().mode, NodeMode::IDLE);
}

TEST_F(SDRNodeTest, ApplyAction_CombinedScanAndProcess_LastSetWins) {
    // Current logic sets mode sequentially; scan then process -> PROCESS wins.
    Action act;
    act.scan_params = RFParams{2.4e9, 1e6, 0, 2e6};
    act.process_task_ids.push_back(1);
    valid_node_->applyAction(act);

    EXPECT_EQ(valid_node_->getState().mode, NodeMode::PROCESS);
    // RF params should still be stored (we set them before mode overwrite).
    EXPECT_DOUBLE_EQ(valid_node_->getState().current_rf.center_freq, 2.4e9);
}

TEST_F(SDRNodeTest, ApplyAction_ScanThenTransmit_TransmitWins) {
    Action act;
    act.scan_params = RFParams{5e9, 2e6, 0, 4e6};
    act.burst = Action::Burst{2, 1, 10.0, {}};
    valid_node_->applyAction(act);

    EXPECT_EQ(valid_node_->getState().mode, NodeMode::TRANSMIT);
    // RF params still stored.
    EXPECT_DOUBLE_EQ(valid_node_->getState().current_rf.center_freq, 5e9);
}

TEST_F(SDRNodeTest, ApplyAction_Twice_ResetsCorrectly) {
    // Apply scan, then idle – mode should be IDLE afterwards.
    Action scan;
    scan.scan_params = RFParams{1e9, 1e6, 0, 2e6};
    valid_node_->applyAction(scan);
    EXPECT_EQ(valid_node_->getState().mode, NodeMode::SCAN);

    Action idle;
    valid_node_->applyAction(idle);
    EXPECT_EQ(valid_node_->getState().mode, NodeMode::IDLE);
}