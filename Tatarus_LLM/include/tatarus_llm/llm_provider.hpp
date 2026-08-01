#pragma once

#include "tatarus_llm/planner_types.hpp"

#include <string>

namespace tatarus::llm {

class LlmProvider {
public:
    virtual ~LlmProvider() = default;
    virtual PlannerOutput plan(const PlannerInput& input) = 0;
    virtual ChatOutput respond(const ChatInput& input) = 0;
    [[nodiscard]] virtual std::string providerName() const = 0;
    [[nodiscard]] virtual std::string currentModel() const = 0;
};

}  // namespace tatarus::llm
