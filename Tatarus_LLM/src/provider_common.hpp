#pragma once

#include "tatarus_llm/cognitive_json.hpp"
#include "tatarus_llm/http_client.hpp"

#include <stdexcept>
#include <string>

namespace tatarus::llm::detail {

inline std::string trimSlash(std::string value) { while (!value.empty() && value.back() == '/') value.pop_back(); return value; }
inline json::Value requireSuccess(const HttpResponse& response, const std::string& operation) {
    if (response.status < 200 || response.status >= 300)
        throw std::runtime_error(operation + " failed with HTTP " + std::to_string(response.status) + ": " + response.body.substr(0, 1000));
    return json::Value::parse(response.body);
}
inline PlannerCommand parseOpenAiToolCall(const json::Value& root) {
    const auto& choices = root.at("choices").array();
    if (choices.empty()) throw std::runtime_error("Planner returned no choices");
    const auto& calls = choices.front().at("message").at("tool_calls").array();
    if (calls.size() != 1) throw std::runtime_error("Planner must return exactly one tool call");
    const auto& function = calls.front().at("function");
    if (json::requiredString(function, "name") != "submit_cognitive_command")
        throw std::runtime_error("Planner called a forbidden tool");
    const auto& arguments = function.at("arguments");
    return parseToolArguments(arguments.isString() ? json::Value::parse(arguments.string()) : arguments);
}
inline std::string parseChatCompletionText(const json::Value& root) {
    const auto& choices = root.at("choices").array();
    if (choices.empty()) throw std::runtime_error("Language response returned no choices");
    const auto& content = choices.front().at("message").at("content");
    if (!content.isString() || content.string().empty()) throw std::runtime_error("Language response contained no text");
    return content.string();
}

}  // namespace tatarus::llm::detail
