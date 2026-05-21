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