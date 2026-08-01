#pragma once

#include "nervous_system.hpp"
#include "tatarus_llm/mini_json.hpp"
#include "tatarus_llm/planner_types.hpp"

#include <cstdint>
#include <string>

namespace tatarus::llm {

class InteractiveEnvironment {
public:
    explicit InteractiveEnvironment(std::uint64_t seed = 9001);
    agns::SensorFrame observe(const std::string& userInput) const;
    EnvironmentFeedback apply(const agns::MotorAction& action);
    [[nodiscard]] json::Value toJson() const;
    void fromJson(const json::Value& value);
    [[nodiscard]] double position() const { return position_; }
    [[nodiscard]] double target() const { return target_; }
    [[nodiscard]] double cumulativeReward() const { return cumulativeReward_; }
    [[nodiscard]] double lastReward() const { return lastReward_; }
private:
    std::uint64_t seed_;
    std::uint64_t step_ = 0;
    double position_ = -0.75;
    double target_ = 0.65;
    double cumulativeReward_ = 0.0;
    double lastReward_ = 0.0;
};

}  // namespace tatarus::llm
