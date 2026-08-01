#include "tatarus_llm/lmstudio_provider.hpp"

#include "provider_common.hpp"
#include "tatarus_llm/bounded_command_validator.hpp"

#include <chrono>
#include <stdexcept>
#include <vector>

namespace tatarus::llm {

LmStudioProvider::LmStudioProvider(std::string baseUrl, std::shared_ptr<HttpClient> http)
    : baseUrl_(detail::trimSlash(std::move(baseUrl))), http_(http ? std::move(http) : makeDefaultHttpClient()) {
    if (baseUrl_.size() >= 3 && baseUrl_.substr(baseUrl_.size() - 3) == "/v1") baseUrl_.resize(baseUrl_.size() - 3);
}

std::string LmStudioProvider::discoverLoadedModel() {
    std::vector<std::string> loaded;
    try {
        const auto root = detail::requireSuccess(http_->get(baseUrl_ + "/api/v1/models"), "LM Studio model discovery");
        const auto& models = root.isArray() ? root.array() : root.at("models").array();
        for (const auto& model : models) {
            if (model.contains("type") && model.at("type").isString() && model.at("type").string() != "llm") continue;
            if (!model.contains("loaded_instances") || !model.at("loaded_instances").isArray() || model.at("loaded_instances").array().empty()) continue;
            const auto& instance = model.at("loaded_instances").array().front();
            if (instance.isObject() && instance.contains("id") && instance.at("id").isString()) loaded.push_back(instance.at("id").string());
            else if (model.contains("key")) loaded.push_back(model.at("key").string());
            else if (model.contains("model_key")) loaded.push_back(model.at("model_key").string());
            else if (model.contains("id")) loaded.push_back(model.at("id").string());
        }
    } catch (const std::exception&) {
        loaded.clear();
        const auto root = detail::requireSuccess(http_->get(baseUrl_ + "/api/v0/models"), "LM Studio legacy model discovery");
        const auto& models = root.isArray() ? root.array() : root.at("data").array();
        for (const auto& model : models) {
            if (model.contains("state") && model.at("state").isString() && model.at("state").string() == "loaded") {
                if (model.contains("id")) loaded.push_back(model.at("id").string());
                else if (model.contains("modelKey")) loaded.push_back(model.at("modelKey").string());
            }
        }
    }
    if (loaded.empty()) throw std::runtime_error("No LLM is loaded in LM Studio. Load exactly one model and keep the local server running.");
    if (loaded.size() != 1) throw std::runtime_error("Multiple LLMs are loaded in LM Studio. Scientific mode refuses an ambiguous model selection; keep exactly one loaded.");
    currentModel_ = loaded.front();
    return currentModel_;
}

PlannerOutput LmStudioProvider::plan(const PlannerInput& input) {
    const std::string model = discoverLoadedModel();  // Deliberately refreshed for every cognitive step.
    const auto started = std::chrono::steady_clock::now();
    const auto response = http_->post(baseUrl_ + "/v1/chat/completions", buildOpenAiChatRequest(input, model).dump(),
                                      {{"Content-Type", "application/json"}});
    const auto root = detail::requireSuccess(response, "LM Studio planning");
    PlannerOutput result;
    result.command = validateAndBound(detail::parseOpenAiToolCall(root));
    result.provider = providerName(); result.model = model;
    result.latencyMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
    return result;
}

ChatOutput LmStudioProvider::respond(const ChatInput& input) {
    const std::string model = discoverLoadedModel();  // A model swap also applies to the language phase.
    const auto started = std::chrono::steady_clock::now();
    const auto response = http_->post(baseUrl_ + "/v1/chat/completions", buildChatResponseRequest(input, model).dump(),
                                      {{"Content-Type", "application/json"}});
    ChatOutput result;
    result.text = detail::parseChatCompletionText(detail::requireSuccess(response, "LM Studio language response"));
    result.provider = providerName(); result.model = model;
    result.latencyMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
    return result;
}

}  // namespace tatarus::llm
