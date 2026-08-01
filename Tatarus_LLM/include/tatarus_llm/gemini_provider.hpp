#pragma once

#include "tatarus_llm/http_client.hpp"
#include "tatarus_llm/llm_provider.hpp"

#include <memory>
#include <string>

namespace tatarus::llm {

class GeminiProvider final : public LlmProvider {
public:
    GeminiProvider(std::string model, std::string apiKey,
                   std::string baseUrl = "https://generativelanguage.googleapis.com/v1beta",
                   std::shared_ptr<HttpClient> http = {});
    PlannerOutput plan(const PlannerInput& input) override;
    ChatOutput respond(const ChatInput& input) override;
    [[nodiscard]] std::string providerName() const override { return "gemini"; }
    [[nodiscard]] std::string currentModel() const override { return model_; }
private:
    std::string model_, apiKey_, baseUrl_;
    std::shared_ptr<HttpClient> http_;
};

}  // namespace tatarus::llm
