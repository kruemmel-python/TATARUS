#pragma once

#include "cognitive_bridge.hpp"
#include "tatarus_llm/planner_types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tatarus::llm {

enum class EpisodicMemoryMode { Anchored, Disabled, LexicalOnly, ShuffledAnchors };

struct EpisodicMemoryConfig {
    EpisodicMemoryMode mode = EpisodicMemoryMode::Anchored;
    std::size_t maximumEpisodes = 512;
    std::size_t maximumContentBytes = 2048;
    std::size_t topK = 3;
    double retrievalThreshold = 0.20;
    double lexicalWeight = 0.55;
    double neuralWeight = 0.40;
    double recencyWeight = 0.05;
    double encodingSynapticWeight = 1.0;
    double decoderSpikeThreshold = 0.5;
    bool unsupervisedPlasticityEnabled = true;
    std::uint32_t plasticityEpochs = 6;
    double hebbianLearningRate = 0.65;
    double recurrentLearningRate = 0.70;
    double heterosynapticDepressionRate = 0.20;
    double eligibilityDecay = 0.85;
    double decodingMargin = 0.10;
    std::uint32_t recurrentFanout = 4;
};

std::string toString(EpisodicMemoryMode mode);
EpisodicMemoryMode parseEpisodicMemoryMode(const std::string& value);

class NeuralEpisodicMemory {
public:
    explicit NeuralEpisodicMemory(EpisodicMemoryConfig config = {});

    void encode(const std::string& role, const std::string& content, const agns::CognitiveState& state);
    [[nodiscard]] std::vector<RecalledEpisode> recall(const std::string& cue, const agns::CognitiveState& state) const;
    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);
    void clear();

    [[nodiscard]] std::size_t episodeCount() const { return episodes_.size(); }
    [[nodiscard]] std::size_t synapseCount() const;
    [[nodiscard]] std::uint64_t lastRecallSpikeCount() const { return lastRecallSpikeCount_; }
    [[nodiscard]] std::uint64_t lastRecallFailureCount() const { return lastRecallFailureCount_; }
    [[nodiscard]] std::uint64_t plasticityUpdateCount() const { return plasticityUpdateCount_; }
    [[nodiscard]] std::uint64_t reservoirTopologyHash() const;
    [[nodiscard]] std::uint64_t reservoirWeightHash() const;
    std::size_t applyWeightLesion(double fraction, std::uint64_t seed);
    [[nodiscard]] const EpisodicMemoryConfig& config() const { return config_; }

private:
    struct AnchorValue { std::uint32_t channel = 0; double value = 0.0; };
    enum class SynapseKind : std::uint8_t { Feedback = 1, Sequence = 2, Sensory = 3 };
    struct Synapse {
        std::uint32_t sourceNeuron = 0;
        std::uint32_t targetNeuron = 0;
        double weight = 0.0;
        double eligibility = 0.0;
        std::uint16_t delaySteps = 1;
        SynapseKind kind = SynapseKind::Feedback;
    };
    struct Episode {
        std::uint64_t id = 0;
        std::uint64_t step = 0;
        std::uint64_t fingerprint = 0;
        std::uint8_t role = 0;
        std::uint32_t firstSourceNeuron = 0;
        std::uint32_t byteCount = 0;
        std::uint64_t contentChecksum = 0;
        std::vector<std::uint64_t> representationIds;
        std::vector<AnchorValue> recallAnchor;
        std::vector<Synapse> synapses;
    };

    EpisodicMemoryConfig config_;
    std::vector<Episode> episodes_;
    std::uint64_t nextId_ = 1;
    std::uint32_t nextSourceNeuron_ = 24;
    mutable std::uint64_t lastRecallSpikeCount_ = 0;
    mutable std::uint64_t lastRecallFailureCount_ = 0;
    std::uint64_t plasticityUpdateCount_ = 0;

    [[nodiscard]] static std::vector<std::string> terms(const std::string& text);
    [[nodiscard]] static double lexicalSimilarity(const std::vector<std::string>& cue, const std::vector<std::string>& episode);
    [[nodiscard]] double neuralSimilarity(const Episode& episode, const agns::CognitiveState& state) const;
    [[nodiscard]] Episode makeEpisode(const std::string& role, const std::string& content, const agns::CognitiveState& state);
    [[nodiscard]] std::string reconstructFromSpikes(const Episode& episode, std::uint64_t& spikeCount) const;
    void loadLegacyJson(const std::filesystem::path& path);
    void loadConstructedV2(const std::filesystem::path& path);
};

}  // namespace tatarus::llm
