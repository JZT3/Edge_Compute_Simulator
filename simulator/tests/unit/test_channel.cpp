#include "sim/core/channel_model.hpp"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <cmath>

using namespace sigint_sim;

// ---------------------------------------------------------------------------
// Helper: create a simple vector of LinkState for testing.
// ---------------------------------------------------------------------------
std::vector<LinkState> makeTestLinks(size_t count) {
    std::vector<LinkState> links;
    links.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        LinkState ls;
        ls.id = LinkId{static_cast<int>(i)};
        ls.from = NodeId{0};
        ls.to = NodeId{static_cast<int>(i + 1)};
        ls.band_center = 2.4e9;
        ls.snr = 0.0;
        ls.capacity_bps = 0.0;
        ls.outage_prob = 1.0;
        ls.active = false;
        links.push_back(ls);
    }
    return links;
}

// ---------------------------------------------------------------------------
// Test Fixture: provides a channel model with typical parameters.
// ---------------------------------------------------------------------------
class BlockFadingChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        params_.availability = 0.9;
        params_.avg_snr_db = 20.0;
        params_.snr_std_db = 3.0;
        params_.outage_snr_db = -10.0;
        params_.bandwidth = 10e6;
        channel_ = std::make_unique<BlockFadingChannel>(params_);
        rng_.seed(42);   // fixed seed for determinism
    }

    BlockFadingChannel::Params params_;
    std::unique_ptr<BlockFadingChannel> channel_;
    std::mt19937 rng_;
    std::vector<NodeState> empty_nodes_; // nodes not used by block fading
};

// ---------------------------------------------------------------------------
// 1. Construction and naming (trivial, but verifies polymorphism)
// ---------------------------------------------------------------------------

TEST_F(BlockFadingChannelTest, NameReturnsBlockFading) {
    EXPECT_EQ(channel_->name(), "BlockFading");
}