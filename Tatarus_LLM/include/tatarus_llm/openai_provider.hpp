#pragma once

#include "tatarus_llm/http_client.hpp"
#include "tatarus_llm/llm_provider.hpp"

#include <memory>
#include <string>

namespace tatarus::llm {

class OpenAiProvider final : public LlmProvider {
public:
    OpenAiProvider(std::string model, std::string apiKey,
                   std::string baseUrl = "https://api.openai.com/v1",
                   std::shared_ptr<HttpClient> http = {});
    PlannerOutput plan(const PlannerInput& input) override;
    ChatOutput respond(const ChatInput& input) override;
    [[nodiscard]] std::string providerName() const override { return "openai"; }
    [[nodiscard]] std::string currentModel() const override { return model_; }
private:
    std::string model_, apiKey_, baseUrl_;
    std::shared_ptr<HttpClient> http_;
};

}  // namespace tatarus::llm
