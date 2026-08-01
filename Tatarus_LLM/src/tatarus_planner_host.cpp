#include "tatarus_llm/tatarus_planner_host.hpp"

#include "tatarus_llm/bounded_command_validator.hpp"
#include "tatarus_llm/mini_json.hpp"

#include <filesystem>
#include <stdexcept>

namespace tatarus::llm {

TatarusPlannerHost::TatarusPlannerHost(agns::NervousSystemConfig config, std::unique_ptr<LlmProvider> provider,
                                       MemoryMode mode, std::uint64_t environmentSeed, EpisodicMemoryConfig memoryConfig)
    : nervousSystem_(std::move(config)), bridge_(nervousSystem_), provider_(std::move(provider)),
      episodicMemory_(std::move(memoryConfig)) {
    if (!provider_) throw std::runtime_error("TatarusPlannerHost requires an LLM provider");
    persistent_.mode = mode;
    persistent_.environment = InteractiveEnvironment(environmentSeed);
}

HostStepResult TatarusPlannerHost::step(const std::string& userInput) {
    HostStepResult result;
    result.before = bridge_.readState();
    result.recalledEpisodes = episodicMemory_.recall(userInput, result.before);
    PlannerInput input{userInput, result.before, persistent_.mode,
                       persistent_.mode == MemoryMode::Product ? persistent_.conversation : std::vector<ConversationTurn>{},
                       result.recalledEpisodes};
    result.planner = provider_->plan(input);
    const agns::SensorFrame observation = persistent_.environment.observe(userInput);
    const agns::CognitiveCommand command = toNervousCommand(result.planner.command, previousFeedback_);
    const agns::CognitiveStep neural = bridge_.step(observation, command);
    result.after = neural.state; result.action = neural.action;
    result.feedback = persistent_.environment.apply(result.action);
    previousFeedback_ = result.feedback;
    const ChatInput chatInput{userInput, result.after, result.planner.command, result.action, result.feedback,
                              persistent_.mode,
                              persistent_.mode == MemoryMode::Product ? persistent_.conversation : std::vector<ConversationTurn>{},
                              result.recalledEpisodes};
    try {
        result.response = provider_->respond(chatInput);
    } catch (const std::exception& error) {
        result.response.provider = provider_->providerName();
        result.response.model = provider_->currentModel();
        result.response.error = error.what();
        result.response.text = "[Sprachantwort nicht verfügbar; der TATARUS-Zustand wurde dennoch verarbeitet.]";
    }
    persistent_.lastProvider = result.response.provider.empty() ? result.planner.provider : result.response.provider;
    persistent_.lastModel = result.response.model.empty() ? result.planner.model : result.response.model;
    if (persistent_.mode == MemoryMode::Product && result.response.error.empty()) {
        persistent_.conversation.push_back({"user", userInput});
        persistent_.conversation.push_back({"assistant", result.response.text});
        if (persistent_.conversation.size() > 24) persistent_.conversation.erase(persistent_.conversation.begin(), persistent_.conversation.begin() + static_cast<std::ptrdiff_t>(persistent_.conversation.size() - 24));
    }
    episodicMemory_.encode("user", userInput, result.after);
    if (result.response.error.empty()) episodicMemory_.encode("assistant", result.response.text, result.after);
    result.reconstructionSpikes = episodicMemory_.lastRecallSpikeCount();
    result.reconstructionFailures = episodicMemory_.lastRecallFailureCount();
    result.plasticityUpdates = episodicMemory_.plasticityUpdateCount();
    result.memorySynapses = episodicMemory_.synapseCount();
    return result;
}

void TatarusPlannerHost::save(const std::filesystem::path& directory) const {
    std::filesystem::create_directories(directory);
    nervousSystem_.saveSnapshot(directory / "nervous_system.agns");
    bridge_.saveState(directory / "cognitive_bridge.bin");
    episodicMemory_.save(directory / "synaptic_memory.bin");
    const auto current = bridge_.readState();
    saveHostState(directory / "host_state.json", persistent_, nervousSystem_.stateHash(), current.functionalFingerprint);
    const auto legacyPath = directory / "episodic_memory.json";
    if (std::filesystem::exists(legacyPath) && !std::filesystem::remove(legacyPath))
        throw std::runtime_error("Legacy plaintext episodic memory could not be removed after synaptic migration");
}

void TatarusPlannerHost::load(const std::filesystem::path& directory) {
    nervousSystem_.loadSnapshot(directory / "nervous_system.agns");
    bridge_.loadState(directory / "cognitive_bridge.bin");
    persistent_ = loadHostState(directory / "host_state.json");
    const auto synapticPath = directory / "synaptic_memory.bin";
    const auto legacyPath = directory / "episodic_memory.json";
    if (std::filesystem::exists(synapticPath)) episodicMemory_.load(synapticPath);
    else if (std::filesystem::exists(legacyPath)) episodicMemory_.load(legacyPath);
    else episodicMemory_.clear();
    previousFeedback_.reward = persistent_.environment.lastReward();
}

agns::CognitiveState TatarusPlannerHost::state() const { return bridge_.readState(); }

}  // namespace tatarus::llm
