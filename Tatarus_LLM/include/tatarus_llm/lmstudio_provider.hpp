#pragma once

#include "tatarus_llm/http_client.hpp"
#include "tatarus_llm/llm_provider.hpp"

#include <memory>
#include <string>

namespace tatarus::llm {

class LmStudioProvider final : public LlmProvider {
public:
    explicit LmStudioProvider(
        std::string baseUrl = "http://127.0.0.1:1234",
        std::shared_ptr<HttpClient> http = {});

    PlannerOutput plan(const PlannerInput& input) override;
    ChatOutput respond(const ChatInput& input) override;
    [[nodiscard]] std::string providerName() const override { return "lmstudio"; }
    [[nodiscard]] std::string currentModel() const override { return currentModel_; }
    std::string discoverLoadedModel();

private:
    std::string baseUrl_;
    std::shared_ptr<HttpClient> http_;
    std::string currentModel_;
};

}  // namespace tatarus::llm
