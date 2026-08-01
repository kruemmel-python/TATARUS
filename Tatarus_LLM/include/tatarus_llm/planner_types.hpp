#pragma once

#include "cognitive_bridge.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tatarus::llm {

enum class MemoryMode { Scientific, Product, Demonstration };

struct ConversationTurn {
    std::string role;
    std::string content;
};

struct RecalledEpisode {
    std::uint64_t id = 0;
    std::string role;
    std::string content;
    double score = 0.0;
    std::uint64_t ageSteps = 0;
};

struct PlannerInput {
    std::string userInput;
    agns::CognitiveState nervousState;
    MemoryMode memoryMode = MemoryMode::Scientific;
    std::vector<ConversationTurn> conversation;
    std::vector<RecalledEpisode> recalledEpisodes;
};

// Intentionally contains no reward. Reward is owned by the environment.
struct PlannerCommand {
    std::string attention = "balanced";
    double motorIntent = 0.0;
    double intentStrength = 0.0;
    std::uint32_t recallCue = 0;
    double recallStrength = 0.0;
};

struct PlannerOutput {
    PlannerCommand command;
    std::string provider;
    std::string model;
    std::uint64_t latencyMs = 0;
};

struct EnvironmentFeedback {
    double reward = 0.0;
};

struct ChatInput {
    std::string userInput;
    agns::CognitiveState nervousState;
    PlannerCommand command;
    agns::MotorAction action;
    EnvironmentFeedback feedback;
    MemoryMode memoryMode = MemoryMode::Scientific;
    std::vector<ConversationTurn> conversation;
    std::vector<RecalledEpisode> recalledEpisodes;
};

// Text is display-only. It is never interpreted as a command or reward.
struct ChatOutput {
    std::string text;
    std::string provider;
    std::string model;
    std::uint64_t latencyMs = 0;
    std::string error;
};

std::string toString(MemoryMode mode);
MemoryMode parseMemoryMode(const std::string& value);

}  // namespace tatarus::llm
