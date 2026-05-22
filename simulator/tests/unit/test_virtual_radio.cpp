#include "sim/core/virtual_radio.hpp"
#include "sim/core/sample_processing_channel.hpp"
#include <gtest/gtest.h>

using namespace sigint_sim;

class VirtualRadioTest : public ::testing::Test {
protected:
    void SetUp() override {
        SampleProcessingChannel::Params ch_params;
        channel_ = std::make_unique<SampleProcessingChannel>(ch_params);
        channel_->setLinkParams(0, 1, 1.0, 2.4e9, 1e6, 1e6, 0.0);
        radio0_ = std::make_unique<VirtualRadio>(*channel_, 0);
        radio1_ = std::make_unique<VirtualRadio>(*channel_, 1);
    }
    std::unique_ptr<SampleProcessingChannel> channel_;
    std::unique_ptr<VirtualRadio> radio0_;
    std::unique_ptr<VirtualRadio> radio1_;
};

TEST_F(VirtualRadioTest, TransmitAndReceiveRoundTrip) {
    std::vector<std::complex<float>> tx_samples(50, {0.5f, 0.5f});
    radio0_->transmit(tx_samples, 2.4e9, 1e6);

    // Manually update the channel so the link buffer is processed
    std::vector<LinkState> link_states(1);
    link_states[0].from = NodeId{0}; link_states[0].to = NodeId{1};
    std::vector<NodeState> empty_nodes;
    std::mt19937 rng(123);
    channel_->update(link_states, empty_nodes, rng);

    auto rx = radio1_->receive(2.4e9, 1e6, 0.0);
    EXPECT_FALSE(rx.empty());
}