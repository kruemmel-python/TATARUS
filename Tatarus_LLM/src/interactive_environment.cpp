#include "tatarus_llm/interactive_environment.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace tatarus::llm {

InteractiveEnvironment::InteractiveEnvironment(std::uint64_t seed) : seed_(seed) {}

agns::SensorFrame InteractiveEnvironment::observe(const std::string& userInput) const {
    agns::SensorFrame frame;
    frame.visionEvents = {position_, target_, target_ - position_, std::abs(target_ - position_)};
    frame.audioSamples = {std::sin(static_cast<double>(step_) * 0.07), std::cos(static_cast<double>(step_) * 0.03)};
    frame.touch = {lastReward_, std::clamp(cumulativeReward_ / 100.0, -1.0, 1.0)};
    const std::size_t limit = std::min<std::size_t>(userInput.size(), 512);
    frame.textBytes.assign(userInput.begin(), userInput.begin() + static_cast<std::ptrdiff_t>(limit));
    frame.temperature = std::sin(static_cast<double>(step_) * 0.01) * 0.25;
    frame.internalEnergy = std::clamp(1.0 - 0.2 * std::abs(lastReward_), 0.0, 1.0);
    frame.reward = lastReward_;
    frame.novelty = userInput.empty() ? 0.0 : static_cast<double>(std::hash<std::string>{}(userInput) % 1000U) / 999.0;
    return frame;
}

EnvironmentFeedback InteractiveEnvironment::apply(const agns::MotorAction& action) {
    const double oldDistance = std::abs(target_ - position_);
    position_ = std::clamp(position_ + std::clamp(action.movement, -1.0, 1.0) * 0.08, -1.0, 1.0);
    const double newDistance = std::abs(target_ - position_);
    lastReward_ = std::clamp((oldDistance - newDistance) * 8.0 - 0.002 * std::abs(action.movement), -1.0, 1.0);
    if (newDistance < 0.05) {
        lastReward_ = 1.0;
        const std::uint64_t mixed = seed_ ^ ((step_ + 1U) * 0x9e3779b97f4a7c15ULL);
        target_ = (static_cast<double>((mixed >> 11U) % 1901U) / 1000.0) - 0.95;
    }
    cumulativeReward_ += lastReward_;
    ++step_;
    return {lastReward_};
}

json::Value InteractiveEnvironment::toJson() const {
    return json::Value::Object{{"seed", std::to_string(seed_)}, {"step", std::to_string(step_)},
        {"position", position_}, {"target", target_}, {"cumulative_reward", cumulativeReward_}, {"last_reward", lastReward_}};
}

void InteractiveEnvironment::fromJson(const json::Value& value) {
    seed_ = std::stoull(json::requiredString(value, "seed"));
    step_ = std::stoull(json::requiredString(value, "step"));
    position_ = json::requiredNumber(value, "position"); target_ = json::requiredNumber(value, "target");
    cumulativeReward_ = json::requiredNumber(value, "cumulative_reward"); lastReward_ = json::requiredNumber(value, "last_reward");
}

}  // namespace tatarus::llm
