#include "../include/sim/core/virtual_radio.hpp"
#include "../include/sim/core/sample_processing_channel.hpp"  
#include <cassert>

namespace sigint_sim {

VirtualRadio::VirtualRadio(SampleProcessingChannel& channel, int node_id)
    : channel_(channel), node_id_(node_id)
{
    assert(node_id >= 0);
}

void VirtualRadio::transmit(const std::vector<std::complex<float>>& samples,
                            double /*center_freq*/, double sample_rate)
{
    // Delegate to the channel – it knows which links are from this node.
    channel_.pushTxSamples(node_id_, samples, sample_rate);
}

std::vector<std::complex<float>> VirtualRadio::receive(double /*center_freq*/,
                                                       double /*sample_rate*/,
                                                       double /*duration_s*/)
{
    // Collect all samples that have arrived for this node from any link.
    return channel_.collectRxSamples(node_id_);
}

} // namespace sigint_sim