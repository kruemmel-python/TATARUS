#include "tatarus_llm/bounded_command_validator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tatarus::llm {
namespace {
double finite(double value, const char* field) {
    if (!std::isfinite(value)) throw std::runtime_error(std::string("Non-finite planner field: ") + field);
    return value;
}
agns::AttentionTarget attention(const std::string& value) {
    if (value == "balanced") return agns::AttentionTarget::Balanced;
    if (value == "vision") return agns::AttentionTarget::Vision;
    if (value == "audio") return agns::AttentionTarget::Audio;
    if (value == "touch") return agns::AttentionTarget::Touch;
    if (value == "text") return agns::AttentionTarget::Text;
    if (value == "interoception") return agns::AttentionTarget::Interoception;
    throw std::runtime_error("Invalid attention target: " + value);
}
}

PlannerCommand validateAndBound(const PlannerCommand& input) {
    (void)attention(input.attention);
    PlannerCommand output = input;
    output.motorIntent = std::clamp(finite(input.motorIntent, "motor_intent"), -1.0, 1.0);
    output.intentStrength = std::clamp(finite(input.intentStrength, "intent_strength"), 0.0, 1.0);
    output.recallCue %= static_cast<std::uint32_t>(agns::CognitiveBridge::recallGroupCount);
    output.recallStrength = std::clamp(finite(input.recallStrength, "recall_strength"), 0.0, 1.0);
    return output;
}

agns::CognitiveCommand toNervousCommand(const PlannerCommand& raw, const EnvironmentFeedback& feedback) {
    const PlannerCommand command = validateAndBound(raw);
    agns::CognitiveCommand output;
    output.attention = attention(command.attention);
    output.motorIntent = command.motorIntent;
    output.intentStrength = command.intentStrength;
    output.recallCue = command.recallCue;
    output.recallStrength = command.recallStrength;
    output.reward = std::clamp(finite(feedback.reward, "environment reward"), -1.0, 1.0);
    return output;
}

}  // namespace tatarus::llm
