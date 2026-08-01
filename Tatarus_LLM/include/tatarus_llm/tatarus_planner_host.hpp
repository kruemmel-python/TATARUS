#pragma once

#include "cognitive_bridge.hpp"
#include "nervous_system.hpp"
#include "tatarus_llm/composite_snapshot.hpp"
#include "tatarus_llm/episodic_memory.hpp"
#include "tatarus_llm/llm_provider.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace tatarus::llm {

struct HostStepResult {
    PlannerOutput planner;
    ChatOutput response;
    std::vector<RecalledEpisode> recalledEpisodes;
    std::uint64_t reconstructionSpikes = 0;
    std::uint64_t reconstructionFailures = 0;
    std::uint64_t plasticityUpdates = 0;
    std::size_t memorySynapses = 0;
    agns::CognitiveState before;
    agns::CognitiveState after;
    agns::MotorAction action;
    EnvironmentFeedback feedback;
};

class TatarusPlannerHost {
public:
    TatarusPlannerHost(agns::NervousSystemConfig config,
                       std::unique_ptr<LlmProvider> provider,
                       MemoryMode mode = MemoryMode::Scientific,
                       std::uint64_t environmentSeed = 9001,
                       EpisodicMemoryConfig memoryConfig = {});
    HostStepResult step(const std::string& userInput);
    void save(const std::filesystem::path& directory) const;
    void load(const std::filesystem::path& directory);
    [[nodiscard]] agns::CognitiveState state() const;
    [[nodiscard]] std::uint64_t stateHash() const { return nervousSystem_.stateHash(); }
    [[nodiscard]] const InteractiveEnvironment& environment() const { return persistent_.environment; }
    [[nodiscard]] LlmProvider& provider() { return *provider_; }
    [[nodiscard]] MemoryMode memoryMode() const { return persistent_.mode; }
    [[nodiscard]] std::size_t episodicMemorySize() const { return episodicMemory_.episodeCount(); }
    [[nodiscard]] std::size_t episodicSynapseCount() const { return episodicMemory_.synapseCount(); }
    [[nodiscard]] std::uint64_t episodicRecallSpikeCount() const { return episodicMemory_.lastRecallSpikeCount(); }
    [[nodiscard]] std::uint64_t episodicRecallFailureCount() const { return episodicMemory_.lastRecallFailureCount(); }
    [[nodiscard]] std::uint64_t episodicPlasticityUpdateCount() const { return episodicMemory_.plasticityUpdateCount(); }
private:
    agns::PersistentNervousSystem nervousSystem_;
    agns::CognitiveBridge bridge_;
    std::unique_ptr<LlmProvider> provider_;
    HostPersistentState persistent_;
    NeuralEpisodicMemory episodicMemory_;
    EnvironmentFeedback previousFeedback_;
};

}  // namespace tatarus::llm
