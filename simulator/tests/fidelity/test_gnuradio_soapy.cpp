// tests/fidelity/test_gnuradio_soapy.cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <gnuradio/top_block.h>
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <SoapySDR/Device.hpp>
#pragma GCC diagnostic pop

// Prevents error
/*
/opt/homebrew/include/gnuradio/rpcregisterhelpers.h:1285:39: error: unused parameter 'functionbase' [-Werror,-Wunused-parameter]
 1285 |                           const char* functionbase,
      |                                       ^
fatal error: too many errors emitted, stopping now [-ferror-limit=]
*/

#include <iostream>
#include <memory>

int main() {
    // Create a GNU Radio top block using the factory function
    auto tb = gr::make_top_block("phase0_gr_test"); // gr::top_block has a protected constructor and cannot be directly instantiated with std::make_shared. GNU Radio uses a factory pattern.
    std::cout << "GNU Radio top block created: " << tb->name() << std::endl;

    // Create a SoapySDR null device (no hardware needed)
    SoapySDR::Device* dev = SoapySDR::Device::make("driver=null");
    if (dev) {
        std::cout << "SoapySDR null device created." << std::endl;
        SoapySDR::Device::unmake(dev);
    } else {
        std::cerr << "Failed to create SoapySDR device." << std::endl;
        return 1;
    }

    std::cout << "Phase 0 bootstrapping successful." << std::endl;
    return 0;
}
