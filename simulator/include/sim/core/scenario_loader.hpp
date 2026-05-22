#pragma once
#include "simulator.hpp"
#include <string>
#include <vector>

namespace sigint_sim {

struct Scenario {
    Simulator::Config config;
    std::vector<EmitterDesc> emitters;
};

Scenario loadScenario(const std::string& jsonFilePath);

} // namespace sigint_sim