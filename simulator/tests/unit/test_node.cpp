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

