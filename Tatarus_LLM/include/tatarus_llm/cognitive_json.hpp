#pragma once

#include "tatarus_llm/mini_json.hpp"
#include "tatarus_llm/planner_types.hpp"

namespace tatarus::llm {

json::Value cognitiveStateJson(const agns::CognitiveState& state);
json::Value recalledEpisodesJson(const std::vector<RecalledEpisode>& episodes);
json::Value commandToolDefinition();
json::Value buildOpenAiChatRequest(const PlannerInput& input, const std::string& model);
json::Value buildChatResponseRequest(const ChatInput& input, const std::string& model);
json::Value chatContextJson(const ChatInput& input);
PlannerCommand parseToolArguments(const json::Value& arguments);

}  // namespace tatarus::llm
