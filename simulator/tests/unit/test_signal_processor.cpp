#include "sim/core/signal_processor.hpp"
#include <gtest/gtest.h>
#include <random>

using namespace sigint_sim;

TEST(SignalProcessor, DetectionHighSnrYieldsSignalPresent) {
    SignalTask task{SignalTaskType::DETECT, 2.4e9, 1e6, 0.001};
    HardwareProfile profile;
    std::mt19937 rng(67);
    TaskResult res = SignalProcessor::execute(task, 100.0, profile, rng);
    EXPECT_TRUE(res.signal_present);
    EXPECT_GT(res.confidence, 0.9);
    EXPECT_GT(res.gflops_consumed, 0.0);
}

TEST(SignalProcessor, ScanProducesNoData) {
    SignalTask task{SignalTaskType::SCAN};
    HardwareProfile profile;
    std::mt19937 rng(25);
    TaskResult res = SignalProcessor::execute(task, 10.0, profile, rng);
    EXPECT_TRUE(res.data.empty());
}

TEST(SignalProcessor, RawIQProducesLargeData) {
    SignalTask task{SignalTaskType::RAW_IQ, 2.4e9, 1e6, 0.01};
    HardwareProfile profile;
    std::mt19937 rng(111);
    TaskResult res = SignalProcessor::execute(task, 10.0, profile, rng);
    EXPECT_GT(res.data.size(), 100);
}

TEST(SignalProcessor, TDOA_ErrorIncreasesWithLowSNR) {
    SignalTask task{SignalTaskType::TDOA, 2.4e9, 1e6, 0.001};
    HardwareProfile profile;
    std::mt19937 rng(222);
    TaskResult res_high = SignalProcessor::execute(task, 100.0, profile, rng);
    TaskResult res_low  = SignalProcessor::execute(task, 0.1, profile, rng);
    EXPECT_LT(res_high.time_error_std_s, res_low.time_error_std_s);
}

TEST(SignalProcessor, Determinism) {
    SignalTask task{SignalTaskType::DETECT};
    HardwareProfile profile;
    std::mt19937 rng1(123), rng2(123);
    TaskResult r1 = SignalProcessor::execute(task, 10.0, profile, rng1);
    TaskResult r2 = SignalProcessor::execute(task, 10.0, profile, rng2);
    EXPECT_EQ(r1.signal_present, r2.signal_present);
    EXPECT_DOUBLE_EQ(r1.confidence, r2.confidence);
}