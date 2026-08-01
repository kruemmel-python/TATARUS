#pragma once

#include "tatarus_llm/planner_types.hpp"

namespace tatarus::llm {

PlannerCommand validateAndBound(const PlannerCommand& command);
agns::CognitiveCommand toNervousCommand(const PlannerCommand& command, const EnvironmentFeedback& feedback);

}  // namespace tatarus::llm
