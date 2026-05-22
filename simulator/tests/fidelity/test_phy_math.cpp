#include "sim/core/phy_math.hpp"
#include <gtest/gtest.h>
#include <random>

using namespace sigint_sim::phy_math;

TEST(PhyMath, FriisPathLoss_DistanceDependence) {
    double loss1 = friis_path_loss_linear(2.4e9, 1.0);
    double loss2 = friis_path_loss_linear(2.4e9, 10.0);
    EXPECT_GT(loss1, loss2);               // closer → more gain (less loss)
    EXPECT_LE(loss1, 1.0);                 // gain ≤ 1
    EXPECT_GT(loss1, 0.0);
}

TEST(PhyMath, ThermalNoise_BasicSanity) {
    double noise = thermal_noise_power_W(10e6, 0.0);
    EXPECT_GT(noise, 0.0);
    EXPECT_LT(noise, 1e-10);              // plausible noise floor
}

TEST(PhyMath, SNR_Linear_DropsWithDistance) {
    std::mt19937 rng(42);
    double snr_near = snr_linear(20.0, 1.0, 2.4e9, 10e6, 5.0, 0.0);
    double snr_far  = snr_linear(20.0, 100.0, 2.4e9, 10e6, 5.0, 0.0);
    EXPECT_GT(snr_near, snr_far);
}

TEST(PhyMath, BER_QPSK_ZeroSNR_EqualsHalf) {
    double ber = ber_qpsk(1e-6);           // essentially 0 SNR
    EXPECT_NEAR(ber, 0.5, 0.01);
}

TEST(PhyMath, ShannonCapacity_Monotonic) {
    double cap_low  = shannon_capacity_bps(1e6, 0.1);
    double cap_high = shannon_capacity_bps(1e6, 100.0);
    EXPECT_GT(cap_high, cap_low);
}

TEST(PhyMath, OutageProbability_Bounds) {
    double p = outage_probability(10.0, 1.0);
    EXPECT_GT(p, 0.0);
    EXPECT_LT(p, 1.0);
}

TEST(PhyMath, DetectionProbability_Bounds) {
    double pd = detection_probability(100.0);
    EXPECT_GT(pd, 0.9);
    double pd_low = detection_probability(0.01);
    EXPECT_LT(pd_low, 0.5);
}

TEST(PhyMath, ResampleNearest_PreservesCount) {
    std::vector<std::complex<float>> in(100, {1.0f, 0.0f});
    auto out = resample_nearest(in, 1e6, 2e6);   // up by 2
    EXPECT_EQ(out.size(), 200);
}