#include "tatarus_llm/episodic_memory.hpp"

#include "tatarus_llm/mini_json.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace tatarus::llm {
namespace {
constexpr std::array<char, 8> kSynapticMemoryMagicV2{'T', 'S', 'M', 'E', 'M', 'V', '2', '\0'};
constexpr std::array<char, 8> kSynapticMemoryMagicV3{'T', 'S', 'M', 'E', 'M', 'V', '3', '\0'};
constexpr std::uint64_t kShuffleMask = 0xa5a5f00d9e3779b9ULL;
constexpr std::uint32_t kRecallChannelSpace = 192U;
constexpr std::uint32_t kHammingBitCount = 12U;
constexpr std::uint32_t kCodeNeuronCount = 2U * kHammingBitCount;
constexpr std::uint32_t kFirstSourceNeuron = kCodeNeuronCount;
constexpr std::size_t kMaximumPersistedEpisodes = 16384;
constexpr std::size_t kMaximumContentBytes = 65536;
constexpr std::size_t kMaximumTotalContentBytes = 1024U * 1024U;
constexpr std::size_t kMaximumPersistedRepresentations = 256;
constexpr std::size_t kMaximumPersistedRecallAnchors = 256;
constexpr std::uint64_t kMaximumPersistedSynapses = 64ULL * 1024ULL * 1024ULL;

template <typename T>
void writePod(std::ostream& output, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    output.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
void readPod(std::istream& input, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    input.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!input) throw std::runtime_error("Synaptic episodic memory is truncated");
}

std::uint64_t contentChecksum(const std::string& content) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char byte : content) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t mix64(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

double initialPlasticWeight(std::uint32_t source, std::uint32_t target, std::uint8_t kind) {
    const auto mixed = mix64((static_cast<std::uint64_t>(source) << 32U) ^ target ^ (static_cast<std::uint64_t>(kind) << 56U));
    return 0.01 + 0.04 * static_cast<double>(mixed & 0xffffU) / 65535.0;
}

std::uint16_t hammingEncode(std::uint8_t value) {
    constexpr std::array<unsigned, 8> dataPositions{3, 5, 6, 7, 9, 10, 11, 12};
    std::uint16_t code = 0;
    for (unsigned bit = 0; bit < dataPositions.size(); ++bit) {
        if ((value & (1U << bit)) != 0) code |= static_cast<std::uint16_t>(1U << (dataPositions[bit] - 1U));
    }
    for (const unsigned parityPosition : {1U, 2U, 4U, 8U}) {
        bool parity = false;
        for (unsigned position = 1; position <= 12; ++position) {
            if (position != parityPosition && (position & parityPosition) != 0 && (code & (1U << (position - 1U))) != 0)
                parity = !parity;
        }
        if (parity) code |= static_cast<std::uint16_t>(1U << (parityPosition - 1U));
    }
    return code;
}

std::uint8_t hammingDecode(std::uint16_t code) {
    unsigned syndrome = 0;
    for (unsigned position = 1; position <= 12; ++position) {
        if ((code & (1U << (position - 1U))) != 0) syndrome ^= position;
    }
    if (syndrome > 12) throw std::runtime_error("Synaptic decoder produced an invalid Hamming syndrome");
    if (syndrome != 0) code ^= static_cast<std::uint16_t>(1U << (syndrome - 1U));
    constexpr std::array<unsigned, 8> dataPositions{3, 5, 6, 7, 9, 10, 11, 12};
    std::uint8_t value = 0;
    for (unsigned bit = 0; bit < dataPositions.size(); ++bit) {
        if ((code & (1U << (dataPositions[bit] - 1U))) != 0) value |= static_cast<std::uint8_t>(1U << bit);
    }
    return value;
}

std::uint8_t roleCode(const std::string& role) {
    if (role == "user") return 1;
    if (role == "assistant") return 2;
    throw std::runtime_error("Episodic memory role must be user or assistant");
}

std::string roleName(std::uint8_t role) {
    if (role == 1) return "user";
    if (role == 2) return "assistant";
    throw std::runtime_error("Synaptic episodic memory contains an invalid role");
}

const std::unordered_set<std::string>& stopWords() {
    static const std::unordered_set<std::string> words{
        "a", "an", "and", "are", "as", "at", "be", "but", "by", "for", "from", "i", "in", "is", "it", "my", "of", "on", "or", "the", "this", "to", "was", "what", "with", "you", "your",
        "aber", "als", "am", "an", "auch", "auf", "aus", "bei", "bin", "das", "dem", "den", "der", "des", "die", "dir", "du", "ein", "eine", "einer", "es", "für", "habe", "hat", "ich", "im", "in", "ist", "mein", "mit", "nur", "oder", "sie", "und", "von", "war", "was", "welchen", "wie", "zu", "zuvor",
        "antworte", "answer", "bitte", "please", "nachricht", "message"};
    return words;
}
}  // namespace

std::string toString(EpisodicMemoryMode mode) {
    switch (mode) {
        case EpisodicMemoryMode::Anchored: return "anchored";
        case EpisodicMemoryMode::Disabled: return "disabled";
        case EpisodicMemoryMode::LexicalOnly: return "lexical-only";
        case EpisodicMemoryMode::ShuffledAnchors: return "shuffled-anchors";
    }
    return "anchored";
}

EpisodicMemoryMode parseEpisodicMemoryMode(const std::string& value) {
    if (value == "anchored" || value == "on") return EpisodicMemoryMode::Anchored;
    if (value == "disabled" || value == "off") return EpisodicMemoryMode::Disabled;
    if (value == "lexical-only" || value == "lexical") return EpisodicMemoryMode::LexicalOnly;
    if (value == "shuffled-anchors" || value == "shuffled") return EpisodicMemoryMode::ShuffledAnchors;
    throw std::runtime_error("Unknown episodic memory mode: " + value);
}

NeuralEpisodicMemory::NeuralEpisodicMemory(EpisodicMemoryConfig config) : config_(std::move(config)) {
    if (config_.maximumEpisodes == 0 || config_.maximumContentBytes == 0 || config_.topK == 0)
        throw std::runtime_error("Episodic memory sizes must be positive");
    if (config_.maximumEpisodes > kMaximumPersistedEpisodes || config_.maximumContentBytes > kMaximumContentBytes ||
        config_.maximumEpisodes > kMaximumTotalContentBytes / config_.maximumContentBytes || config_.topK > config_.maximumEpisodes)
        throw std::runtime_error("Episodic memory size exceeds its safe bound");
    if (!std::isfinite(config_.retrievalThreshold) || config_.retrievalThreshold < 0.0 || config_.retrievalThreshold > 1.0)
        throw std::runtime_error("Episodic retrieval threshold must be in [0,1]");
    const double total = config_.lexicalWeight + config_.neuralWeight + config_.recencyWeight;
    if (!std::isfinite(total) || config_.lexicalWeight < 0.0 || config_.neuralWeight < 0.0 || config_.recencyWeight < 0.0 ||
        std::abs(total - 1.0) > 1e-9)
        throw std::runtime_error("Episodic retrieval weights must be finite, non-negative and sum to 1");
    if (!std::isfinite(config_.encodingSynapticWeight) || !std::isfinite(config_.decoderSpikeThreshold) ||
        config_.decoderSpikeThreshold <= 0.0 || config_.encodingSynapticWeight < config_.decoderSpikeThreshold ||
        config_.encodingSynapticWeight > 100.0)
        throw std::runtime_error("Synaptic encoding weight must reach the positive decoder spike threshold");
    if (config_.plasticityEpochs == 0 || config_.plasticityEpochs > 1000 || config_.recurrentFanout == 0 || config_.recurrentFanout > 64 ||
        !std::isfinite(config_.hebbianLearningRate) || config_.hebbianLearningRate <= 0.0 || config_.hebbianLearningRate > 1.0 ||
        !std::isfinite(config_.recurrentLearningRate) || config_.recurrentLearningRate <= 0.0 || config_.recurrentLearningRate > 1.0 ||
        !std::isfinite(config_.heterosynapticDepressionRate) || config_.heterosynapticDepressionRate < 0.0 || config_.heterosynapticDepressionRate > 1.0 ||
        !std::isfinite(config_.eligibilityDecay) || config_.eligibilityDecay < 0.0 || config_.eligibilityDecay > 1.0 ||
        !std::isfinite(config_.decodingMargin) || config_.decodingMargin <= 0.0 || config_.decodingMargin >= config_.encodingSynapticWeight)
        throw std::runtime_error("Unsupervised reservoir-plasticity parameters are invalid");
    nextSourceNeuron_ = kFirstSourceNeuron;
}

std::vector<std::string> NeuralEpisodicMemory::terms(const std::string& text) {
    std::set<std::string> unique;
    std::string token;
    auto flush = [&]() {
        if (token.size() >= 2 && !stopWords().contains(token)) unique.insert(token);
        token.clear();
    };
    for (const unsigned char byte : text) {
        if (std::isalnum(byte) || byte >= 0x80U) token.push_back(byte < 0x80U ? static_cast<char>(std::tolower(byte)) : static_cast<char>(byte));
        else flush();
    }
    flush();
    return {unique.begin(), unique.end()};
}

double NeuralEpisodicMemory::lexicalSimilarity(const std::vector<std::string>& cue, const std::vector<std::string>& episode) {
    if (cue.empty() || episode.empty()) return 0.0;
    std::size_t left = 0, right = 0, intersection = 0;
    while (left < cue.size() && right < episode.size()) {
        if (cue[left] == episode[right]) { ++intersection; ++left; ++right; }
        else if (cue[left] < episode[right]) ++left;
        else ++right;
    }
    return static_cast<double>(intersection) / static_cast<double>(cue.size());
}

NeuralEpisodicMemory::Episode NeuralEpisodicMemory::makeEpisode(
    const std::string& role, const std::string& content, const agns::CognitiveState& state) {
    const std::string boundedContent = content.substr(0, config_.maximumContentBytes);
    if (boundedContent.size() > std::numeric_limits<std::uint32_t>::max() - nextSourceNeuron_)
        throw std::runtime_error("Synaptic source-neuron address space exhausted");
    Episode episode;
    episode.id = nextId_++;
    episode.step = state.step;
    episode.fingerprint = state.functionalFingerprint;
    episode.role = roleCode(role);
    episode.firstSourceNeuron = nextSourceNeuron_;
    episode.byteCount = static_cast<std::uint32_t>(boundedContent.size());
    episode.contentChecksum = contentChecksum(boundedContent);
    for (const auto& representation : state.activeRepresentations) episode.representationIds.push_back(representation.id);
    auto recalls = state.recalledStates;
    std::sort(recalls.begin(), recalls.end(), [](const auto& a, const auto& b) { return std::abs(a.strength) > std::abs(b.strength); });
    if (recalls.size() > 24) recalls.resize(24);
    for (const auto& recall : recalls) episode.recallAnchor.push_back({recall.channel, recall.strength});

    episode.synapses.reserve(boundedContent.size() * (2U * kCodeNeuronCount + config_.recurrentFanout));
    for (std::uint32_t offset = 0; offset < episode.byteCount; ++offset) {
        const std::uint32_t assembly = episode.firstSourceNeuron + offset;
        for (std::uint32_t channel = 0; channel < kCodeNeuronCount; ++channel) {
            const auto feedbackKind = static_cast<std::uint8_t>(SynapseKind::Feedback);
            const auto sensoryKind = static_cast<std::uint8_t>(SynapseKind::Sensory);
            episode.synapses.push_back({assembly, channel, initialPlasticWeight(assembly, channel, feedbackKind), 0.0, 1, SynapseKind::Feedback});
            episode.synapses.push_back({channel, assembly, initialPlasticWeight(channel, assembly, sensoryKind), 0.0, 1, SynapseKind::Sensory});
        }
    }

    for (std::uint32_t offset = 0; offset + 1U < episode.byteCount; ++offset) {
        const std::uint32_t source = episode.firstSourceNeuron + offset;
        const std::uint32_t correctTarget = source + 1U;
        std::set<std::uint32_t> candidates{correctTarget};
        for (std::uint32_t attempt = 0; candidates.size() < config_.recurrentFanout && attempt < episode.byteCount * 2U; ++attempt) {
            const auto mixed = mix64(static_cast<std::uint64_t>(source) ^ (static_cast<std::uint64_t>(attempt) << 32U));
            const std::uint32_t target = episode.firstSourceNeuron + static_cast<std::uint32_t>(mixed % episode.byteCount);
            if (target != source) candidates.insert(target);
        }
        for (const auto target : candidates) {
            const auto kind = static_cast<std::uint8_t>(SynapseKind::Sequence);
            episode.synapses.push_back({source, target, initialPlasticWeight(source, target, kind), 0.0, 1, SynapseKind::Sequence});
        }
    }

    if (config_.unsupervisedPlasticityEnabled) {
        for (std::uint32_t offset = 0; offset < episode.byteCount; ++offset) {
            const std::uint32_t assembly = episode.firstSourceNeuron + offset;
            const auto code = hammingEncode(static_cast<std::uint8_t>(boundedContent[offset]));
            for (std::uint32_t epoch = 0; epoch < config_.plasticityEpochs; ++epoch) {
                for (auto& synapse : episode.synapses) {
                    std::uint32_t channel = 0;
                    bool belongsToAssembly = false;
                    if (synapse.kind == SynapseKind::Feedback && synapse.sourceNeuron == assembly) {
                        channel = synapse.targetNeuron;
                        belongsToAssembly = true;
                    } else if (synapse.kind == SynapseKind::Sensory && synapse.targetNeuron == assembly) {
                        channel = synapse.sourceNeuron;
                        belongsToAssembly = true;
                    }
                    if (!belongsToAssembly) continue;
                    const std::uint32_t hammingBit = channel / 2U;
                    const std::uint32_t encodedValue = (code >> hammingBit) & 1U;
                    const bool coactive = (channel % 2U) == encodedValue;
                    synapse.eligibility = std::clamp(config_.eligibilityDecay * synapse.eligibility + (coactive ? 1.0 : 0.0), 0.0, 1.0);
                    if (coactive) {
                        synapse.weight += config_.hebbianLearningRate * synapse.eligibility * (config_.encodingSynapticWeight - synapse.weight);
                    } else {
                        synapse.weight *= 1.0 - config_.heterosynapticDepressionRate;
                    }
                    ++plasticityUpdateCount_;
                }
            }
        }
        for (std::uint32_t offset = 0; offset + 1U < episode.byteCount; ++offset) {
            const std::uint32_t source = episode.firstSourceNeuron + offset;
            const std::uint32_t correctTarget = source + 1U;
            for (std::uint32_t epoch = 0; epoch < config_.plasticityEpochs; ++epoch) {
                for (auto& synapse : episode.synapses) {
                    if (synapse.kind != SynapseKind::Sequence || synapse.sourceNeuron != source) continue;
                    const bool causal = synapse.targetNeuron == correctTarget;
                    synapse.eligibility = std::clamp(config_.eligibilityDecay * synapse.eligibility + (causal ? 1.0 : 0.0), 0.0, 1.0);
                    if (causal) {
                        synapse.weight += config_.recurrentLearningRate * synapse.eligibility * (config_.encodingSynapticWeight - synapse.weight);
                    } else {
                        synapse.weight *= 1.0 - config_.heterosynapticDepressionRate;
                    }
                    ++plasticityUpdateCount_;
                }
            }
        }
    }
    nextSourceNeuron_ += episode.byteCount;
    return episode;
}

void NeuralEpisodicMemory::encode(const std::string& role, const std::string& content, const agns::CognitiveState& state) {
    if (config_.mode == EpisodicMemoryMode::Disabled || content.empty()) return;
    episodes_.push_back(makeEpisode(role, content, state));
    if (episodes_.size() > config_.maximumEpisodes)
        episodes_.erase(episodes_.begin(), episodes_.begin() + static_cast<std::ptrdiff_t>(episodes_.size() - config_.maximumEpisodes));
}

double NeuralEpisodicMemory::neuralSimilarity(const Episode& episode, const agns::CognitiveState& state) const {
    std::unordered_set<std::uint64_t> currentRepresentations;
    for (const auto& representation : state.activeRepresentations) currentRepresentations.insert(representation.id);
    std::size_t representationMatches = 0;
    for (std::uint64_t id : episode.representationIds) {
        if (config_.mode == EpisodicMemoryMode::ShuffledAnchors) id ^= kShuffleMask;
        if (currentRepresentations.contains(id)) ++representationMatches;
    }
    const std::size_t representationUnion = currentRepresentations.size() + episode.representationIds.size() - representationMatches;
    const double representationScore = representationUnion == 0 ? 0.0 : static_cast<double>(representationMatches) / static_cast<double>(representationUnion);

    std::unordered_map<std::uint32_t, double> currentRecall;
    for (const auto& recall : state.recalledStates) currentRecall[recall.channel] = recall.strength;
    double dot = 0.0, storedNorm = 0.0, currentNorm = 0.0;
    for (const auto& anchor : episode.recallAnchor) {
        const std::uint32_t channel = config_.mode == EpisodicMemoryMode::ShuffledAnchors
            ? (anchor.channel + 67U) % kRecallChannelSpace : anchor.channel;
        const double current = currentRecall.contains(channel) ? currentRecall[channel] : 0.0;
        dot += anchor.value * current;
        storedNorm += anchor.value * anchor.value;
        currentNorm += current * current;
    }
    const double recallScore = storedNorm > 1e-18 && currentNorm > 1e-18
        ? std::clamp(dot / std::sqrt(storedNorm * currentNorm), 0.0, 1.0) : 0.0;
    const std::uint64_t storedFingerprint = config_.mode == EpisodicMemoryMode::ShuffledAnchors
        ? episode.fingerprint ^ kShuffleMask : episode.fingerprint;
    const double fingerprintScore = 1.0 - static_cast<double>(std::popcount(storedFingerprint ^ state.functionalFingerprint)) / 64.0;
    return std::clamp(0.45 * representationScore + 0.45 * recallScore + 0.10 * fingerprintScore, 0.0, 1.0);
}

std::string NeuralEpisodicMemory::reconstructFromSpikes(const Episode& episode, std::uint64_t& spikeCount) const {
    std::string content;
    content.reserve(episode.byteCount);
    std::uint32_t activeSource = episode.firstSourceNeuron;
    for (std::uint32_t offset = 0; offset < episode.byteCount; ++offset) {
        const std::uint32_t expectedSource = episode.firstSourceNeuron + offset;
        if (activeSource != expectedSource) throw std::runtime_error("Synaptic sequence chain failed to activate the expected address neuron");
        ++spikeCount;
        std::array<double, kCodeNeuronCount> decoderMembranes{};
        std::optional<std::pair<std::uint32_t, double>> nextSource;
        double secondSequenceWeight = -std::numeric_limits<double>::infinity();
        for (const auto& synapse : episode.synapses) {
            if (synapse.sourceNeuron != activeSource) continue;
            if (synapse.delaySteps != 1 || !std::isfinite(synapse.weight) || synapse.weight <= 0.0)
                throw std::runtime_error("Synaptic episodic memory contains an invalid transmission");
            if (synapse.kind == SynapseKind::Feedback) {
                if (synapse.targetNeuron >= kCodeNeuronCount) throw std::runtime_error("Synaptic decoder target is invalid");
                decoderMembranes[synapse.targetNeuron] += synapse.weight;
            } else if (synapse.kind == SynapseKind::Sequence) {
                if (!nextSource.has_value() || synapse.weight > nextSource->second) {
                    if (nextSource.has_value()) secondSequenceWeight = std::max(secondSequenceWeight, nextSource->second);
                    nextSource = std::pair{synapse.targetNeuron, synapse.weight};
                } else {
                    secondSequenceWeight = std::max(secondSequenceWeight, synapse.weight);
                }
            } else if (synapse.kind != SynapseKind::Sensory) {
                throw std::runtime_error("Synaptic episode contains an unknown synapse kind");
            }
        }
        std::uint16_t code = 0;
        for (std::uint32_t hammingBit = 0; hammingBit < kHammingBitCount; ++hammingBit) {
            const double zeroPotential = decoderMembranes[2U * hammingBit];
            const double onePotential = decoderMembranes[2U * hammingBit + 1U];
            const double winner = std::max(zeroPotential, onePotential);
            if (winner < config_.decoderSpikeThreshold || std::abs(onePotential - zeroPotential) < config_.decodingMargin)
                throw std::runtime_error("Plastic reservoir did not form a decodable byte assembly");
            if (onePotential > zeroPotential) code |= static_cast<std::uint16_t>(1U << hammingBit);
            ++spikeCount;
        }
        content.push_back(static_cast<char>(hammingDecode(code)));
        if (offset + 1U < episode.byteCount) {
            if (!nextSource.has_value() || nextSource->second < config_.decoderSpikeThreshold ||
                nextSource->second - secondSequenceWeight < config_.decodingMargin || nextSource->first != expectedSource + 1U)
                throw std::runtime_error("Plastic recurrent reservoir did not learn the causal sequence transition");
            activeSource = nextSource->first;
        } else if (nextSource.has_value()) {
            throw std::runtime_error("Synaptic sequence chain continues beyond the stored episode");
        }
    }
    if (contentChecksum(content) != episode.contentChecksum)
        throw std::runtime_error("Spike-reconstructed episode failed its content checksum");
    return content;
}

std::vector<RecalledEpisode> NeuralEpisodicMemory::recall(const std::string& cue, const agns::CognitiveState& state) const {
    lastRecallSpikeCount_ = 0;
    lastRecallFailureCount_ = 0;
    if (config_.mode == EpisodicMemoryMode::Disabled) return {};
    const auto cueTerms = terms(cue);
    std::vector<RecalledEpisode> result;
    for (const auto& episode : episodes_) {
        std::string reconstructed;
        try {
            reconstructed = reconstructFromSpikes(episode, lastRecallSpikeCount_);
        } catch (const std::exception&) {
            ++lastRecallFailureCount_;
            continue;
        }
        const double lexical = lexicalSimilarity(cueTerms, terms(reconstructed));
        const double neural = config_.mode == EpisodicMemoryMode::LexicalOnly ? 0.0 : neuralSimilarity(episode, state);
        if (config_.mode != EpisodicMemoryMode::LexicalOnly && neural < 0.08) continue;
        const std::uint64_t age = state.step >= episode.step ? state.step - episode.step : 0;
        const double recency = std::exp(-static_cast<double>(age) / 2000.0);
        const double score = config_.mode == EpisodicMemoryMode::LexicalOnly
            ? 0.95 * lexical + 0.05 * recency
            : config_.lexicalWeight * lexical + config_.neuralWeight * neural + config_.recencyWeight * recency;
        if (score >= config_.retrievalThreshold)
            result.push_back({episode.id, roleName(episode.role), reconstructed, score, age});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.score > b.score || (a.score == b.score && a.id > b.id); });
    if (result.size() > config_.topK) result.resize(config_.topK);
    return result;
}

std::size_t NeuralEpisodicMemory::synapseCount() const {
    std::size_t count = 0;
    for (const auto& episode : episodes_) count += episode.synapses.size();
    return count;
}

std::uint64_t NeuralEpisodicMemory::reservoirTopologyHash() const {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& episode : episodes_) {
        for (const auto& synapse : episode.synapses) {
            hash ^= mix64((static_cast<std::uint64_t>(synapse.sourceNeuron) << 32U) ^ synapse.targetNeuron ^
                          (static_cast<std::uint64_t>(static_cast<std::uint8_t>(synapse.kind)) << 56U));
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

std::uint64_t NeuralEpisodicMemory::reservoirWeightHash() const {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& episode : episodes_) {
        for (const auto& synapse : episode.synapses) {
            hash ^= std::bit_cast<std::uint64_t>(synapse.weight);
            hash *= 1099511628211ULL;
            hash ^= std::bit_cast<std::uint64_t>(synapse.eligibility);
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

std::size_t NeuralEpisodicMemory::applyWeightLesion(double fraction, std::uint64_t seed) {
    if (!std::isfinite(fraction) || fraction < 0.0 || fraction > 1.0)
        throw std::runtime_error("Synaptic lesion fraction must be in [0,1]");
    const std::uint64_t threshold = static_cast<std::uint64_t>(fraction * static_cast<long double>(std::numeric_limits<std::uint64_t>::max()));
    std::size_t damaged = 0;
    for (auto& episode : episodes_) {
        for (auto& synapse : episode.synapses) {
            if (synapse.kind == SynapseKind::Sensory) continue;
            const auto selector = mix64(seed ^ (static_cast<std::uint64_t>(synapse.sourceNeuron) << 32U) ^
                                        synapse.targetNeuron ^ static_cast<std::uint8_t>(synapse.kind));
            if (selector <= threshold) {
                synapse.weight = 0.01;
                synapse.eligibility = 0.0;
                ++damaged;
            }
        }
    }
    return damaged;
}

void NeuralEpisodicMemory::save(const std::filesystem::path& path) const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Cannot write synaptic episodic memory: " + path.string());
    output.write(kSynapticMemoryMagicV3.data(), kSynapticMemoryMagicV3.size());
    const auto savedMode = static_cast<std::uint8_t>(config_.mode);
    writePod(output, savedMode);
    writePod(output, nextId_);
    writePod(output, nextSourceNeuron_);
    writePod(output, plasticityUpdateCount_);
    const std::uint64_t episodeCount = episodes_.size();
    writePod(output, episodeCount);
    for (const auto& episode : episodes_) {
        writePod(output, episode.id);
        writePod(output, episode.step);
        writePod(output, episode.fingerprint);
        writePod(output, episode.role);
        writePod(output, episode.firstSourceNeuron);
        writePod(output, episode.byteCount);
        writePod(output, episode.contentChecksum);
        const std::uint64_t representationCount = episode.representationIds.size();
        writePod(output, representationCount);
        for (const auto id : episode.representationIds) writePod(output, id);
        const std::uint64_t recallCount = episode.recallAnchor.size();
        writePod(output, recallCount);
        for (const auto& anchor : episode.recallAnchor) {
            writePod(output, anchor.channel);
            writePod(output, anchor.value);
        }
        const std::uint64_t synapseCount = episode.synapses.size();
        writePod(output, synapseCount);
        for (const auto& synapse : episode.synapses) {
            writePod(output, synapse.sourceNeuron);
            writePod(output, synapse.targetNeuron);
            writePod(output, synapse.weight);
            writePod(output, synapse.eligibility);
            writePod(output, synapse.delaySteps);
            const auto kind = static_cast<std::uint8_t>(synapse.kind);
            writePod(output, kind);
        }
    }
    if (!output) throw std::runtime_error("Synaptic episodic memory write failed");
}

void NeuralEpisodicMemory::loadLegacyJson(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open legacy episodic memory: " + path.string());
    const std::string source((std::istreambuf_iterator<char>(stream)), {});
    const auto root = json::Value::parse(source);
    if (json::requiredString(root, "format") != "tatarus-neural-episodic-memory-v1")
        throw std::runtime_error("Unsupported legacy episodic memory format");
    const auto& serializedEpisodes = root.at("episodes").array();
    if (serializedEpisodes.size() > kMaximumPersistedEpisodes) throw std::runtime_error("Legacy episodic memory snapshot is too large");
    clear();
    std::uint64_t greatestId = 0;
    for (const auto& item : serializedEpisodes) {
        const auto role = json::requiredString(item, "role");
        const auto content = json::requiredString(item, "content");
        if (content.empty() || content.size() > config_.maximumContentBytes)
            throw std::runtime_error("Invalid legacy episodic content size");
        agns::CognitiveState state;
        state.step = std::stoull(json::requiredString(item, "step"));
        state.functionalFingerprint = std::stoull(json::requiredString(item, "fingerprint"));
        for (const auto& id : item.at("representations").array())
            state.activeRepresentations.push_back({std::stoull(id.string()), 0.0, 0.0, 0.0});
        for (const auto& anchor : item.at("recall_anchor").array()) {
            const double channel = json::requiredNumber(anchor, "channel");
            const double value = json::requiredNumber(anchor, "value");
            if (!std::isfinite(channel) || !std::isfinite(value) || channel < 0.0 || channel >= kRecallChannelSpace || std::floor(channel) != channel)
                throw std::runtime_error("Invalid legacy episodic recall anchor");
            state.recalledStates.push_back({static_cast<std::uint32_t>(channel), value});
        }
        auto episode = makeEpisode(role, content, state);
        episode.id = std::stoull(json::requiredString(item, "id"));
        if (episode.id == 0) throw std::runtime_error("Invalid legacy episodic id");
        greatestId = std::max(greatestId, episode.id);
        episodes_.push_back(std::move(episode));
    }
    const auto legacyNextId = std::stoull(json::requiredString(root, "next_id"));
    if (legacyNextId == 0 || legacyNextId <= greatestId) throw std::runtime_error("Invalid legacy episodic next_id");
    nextId_ = legacyNextId;
    if (episodes_.size() > config_.maximumEpisodes)
        episodes_.erase(episodes_.begin(), episodes_.begin() + static_cast<std::ptrdiff_t>(episodes_.size() - config_.maximumEpisodes));
}

void NeuralEpisodicMemory::loadConstructedV2(const std::filesystem::path& path) {
    struct V2Synapse {
        std::uint32_t source = 0;
        std::uint32_t target = 0;
        double weight = 0.0;
        std::uint16_t delay = 0;
        std::uint8_t kind = 0;
    };
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open V2 synaptic episodic memory");
    std::array<char, 8> magic{};
    input.read(magic.data(), magic.size());
    if (magic != kSynapticMemoryMagicV2) throw std::runtime_error("Invalid V2 synaptic memory magic");
    std::uint8_t savedMode = 0;
    std::uint64_t loadedNextId = 0;
    std::uint32_t ignoredNextSource = 0;
    std::uint64_t episodeCount = 0;
    readPod(input, savedMode);
    readPod(input, loadedNextId);
    readPod(input, ignoredNextSource);
    readPod(input, episodeCount);
    if (savedMode > static_cast<std::uint8_t>(EpisodicMemoryMode::ShuffledAnchors) ||
        episodeCount > config_.maximumEpisodes || loadedNextId == 0)
        throw std::runtime_error("Invalid V2 synaptic memory header");
    clear();
    std::uint64_t greatestId = 0;
    for (std::uint64_t index = 0; index < episodeCount; ++index) {
        std::uint64_t id = 0, step = 0, fingerprint = 0, checksum = 0;
        std::uint8_t role = 0;
        std::uint32_t firstSource = 0, byteCount = 0;
        readPod(input, id);
        readPod(input, step);
        readPod(input, fingerprint);
        readPod(input, role);
        readPod(input, firstSource);
        readPod(input, byteCount);
        readPod(input, checksum);
        if (id == 0 || (role != 1 && role != 2) || byteCount == 0 || byteCount > config_.maximumContentBytes)
            throw std::runtime_error("Invalid V2 synaptic episode");
        agns::CognitiveState state;
        state.step = step;
        state.functionalFingerprint = fingerprint;
        std::uint64_t representationCount = 0;
        readPod(input, representationCount);
        if (representationCount > kMaximumPersistedRepresentations) throw std::runtime_error("Too many V2 representations");
        for (std::uint64_t item = 0; item < representationCount; ++item) {
            std::uint64_t representationId = 0;
            readPod(input, representationId);
            state.activeRepresentations.push_back({representationId, 0.0, 0.0, 0.0});
        }
        std::uint64_t recallCount = 0;
        readPod(input, recallCount);
        if (recallCount > kMaximumPersistedRecallAnchors) throw std::runtime_error("Too many V2 recall anchors");
        for (std::uint64_t item = 0; item < recallCount; ++item) {
            std::uint32_t channel = 0;
            double value = 0.0;
            readPod(input, channel);
            readPod(input, value);
            if (channel >= kRecallChannelSpace || !std::isfinite(value)) throw std::runtime_error("Invalid V2 recall anchor");
            state.recalledStates.push_back({channel, value});
        }
        std::uint64_t synapseCount = 0;
        readPod(input, synapseCount);
        if (synapseCount > static_cast<std::uint64_t>(byteCount) * 13ULL) throw std::runtime_error("Too many V2 synapses");
        std::vector<V2Synapse> synapses(static_cast<std::size_t>(synapseCount));
        for (auto& synapse : synapses) {
            readPod(input, synapse.source);
            readPod(input, synapse.target);
            readPod(input, synapse.weight);
            readPod(input, synapse.delay);
            readPod(input, synapse.kind);
        }
        std::string content;
        content.reserve(byteCount);
        std::uint32_t activeSource = firstSource;
        for (std::uint32_t offset = 0; offset < byteCount; ++offset) {
            if (activeSource != firstSource + offset) throw std::runtime_error("V2 sequence is invalid");
            std::uint16_t code = 0;
            std::optional<std::uint32_t> next;
            for (const auto& synapse : synapses) {
                if (synapse.source != activeSource || synapse.delay != 1 || !std::isfinite(synapse.weight)) continue;
                if (synapse.kind == 1 && synapse.target < kHammingBitCount && synapse.weight >= config_.decoderSpikeThreshold)
                    code |= static_cast<std::uint16_t>(1U << synapse.target);
                else if (synapse.kind == 2 && synapse.weight >= config_.decoderSpikeThreshold)
                    next = synapse.target;
            }
            content.push_back(static_cast<char>(hammingDecode(code)));
            if (offset + 1U < byteCount) {
                if (!next.has_value() || *next != activeSource + 1U) throw std::runtime_error("V2 sequence is interrupted");
                activeSource = *next;
            }
        }
        if (contentChecksum(content) != checksum) throw std::runtime_error("V2 episode checksum failed during migration");
        auto episode = makeEpisode(roleName(role), content, state);
        episode.id = id;
        greatestId = std::max(greatestId, id);
        episodes_.push_back(std::move(episode));
    }
    if (loadedNextId <= greatestId) throw std::runtime_error("Invalid V2 next_id");
    nextId_ = loadedNextId;
}

void NeuralEpisodicMemory::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open synaptic episodic memory: " + path.string());
    std::array<char, 8> magic{};
    input.read(magic.data(), magic.size());
    if (!input) throw std::runtime_error("Synaptic episodic memory is truncated");
    if (magic[0] == '{') {
        input.close();
        loadLegacyJson(path);
        return;
    }
    if (magic == kSynapticMemoryMagicV2) {
        input.close();
        loadConstructedV2(path);
        return;
    }
    if (magic != kSynapticMemoryMagicV3) throw std::runtime_error("Unsupported synaptic episodic memory format");
    std::uint8_t savedMode = 0;
    std::uint64_t loadedNextId = 0;
    std::uint32_t loadedNextSource = 0;
    std::uint64_t loadedPlasticityUpdates = 0;
    std::uint64_t episodeCount = 0;
    readPod(input, savedMode);
    readPod(input, loadedNextId);
    readPod(input, loadedNextSource);
    readPod(input, loadedPlasticityUpdates);
    readPod(input, episodeCount);
    if (savedMode > static_cast<std::uint8_t>(EpisodicMemoryMode::ShuffledAnchors) ||
        episodeCount > kMaximumPersistedEpisodes || episodeCount > config_.maximumEpisodes ||
        loadedNextId == 0 || loadedNextSource < kFirstSourceNeuron)
        throw std::runtime_error("Invalid synaptic episodic memory header");
    std::vector<Episode> loaded;
    loaded.reserve(static_cast<std::size_t>(episodeCount));
    std::uint64_t greatestId = 0;
    std::uint64_t totalSynapses = 0;
    std::uint32_t greatestSourceEnd = kFirstSourceNeuron;
    for (std::uint64_t index = 0; index < episodeCount; ++index) {
        Episode episode;
        readPod(input, episode.id);
        readPod(input, episode.step);
        readPod(input, episode.fingerprint);
        readPod(input, episode.role);
        readPod(input, episode.firstSourceNeuron);
        readPod(input, episode.byteCount);
        readPod(input, episode.contentChecksum);
        if (episode.id == 0 || (episode.role != 1 && episode.role != 2) || episode.byteCount == 0 ||
            episode.byteCount > config_.maximumContentBytes || episode.firstSourceNeuron < kFirstSourceNeuron ||
            episode.firstSourceNeuron > std::numeric_limits<std::uint32_t>::max() - episode.byteCount)
            throw std::runtime_error("Invalid synaptic episodic entry");
        std::uint64_t representationCount = 0;
        readPod(input, representationCount);
        if (representationCount > kMaximumPersistedRepresentations) throw std::runtime_error("Too many synaptic episode representations");
        episode.representationIds.resize(static_cast<std::size_t>(representationCount));
        for (auto& id : episode.representationIds) readPod(input, id);
        std::uint64_t recallCount = 0;
        readPod(input, recallCount);
        if (recallCount > kMaximumPersistedRecallAnchors) throw std::runtime_error("Too many synaptic episode recall anchors");
        episode.recallAnchor.resize(static_cast<std::size_t>(recallCount));
        for (auto& anchor : episode.recallAnchor) {
            readPod(input, anchor.channel);
            readPod(input, anchor.value);
            if (anchor.channel >= kRecallChannelSpace || !std::isfinite(anchor.value))
                throw std::runtime_error("Invalid synaptic episode recall anchor");
        }
        std::uint64_t synapseCount = 0;
        readPod(input, synapseCount);
        totalSynapses += synapseCount;
        if (totalSynapses > kMaximumPersistedSynapses || synapseCount > static_cast<std::uint64_t>(episode.byteCount) * 128ULL)
            throw std::runtime_error("Synaptic episodic memory contains too many synapses");
        episode.synapses.resize(static_cast<std::size_t>(synapseCount));
        for (auto& synapse : episode.synapses) {
            std::uint8_t kind = 0;
            readPod(input, synapse.sourceNeuron);
            readPod(input, synapse.targetNeuron);
            readPod(input, synapse.weight);
            readPod(input, synapse.eligibility);
            readPod(input, synapse.delaySteps);
            readPod(input, kind);
            synapse.kind = static_cast<SynapseKind>(kind);
            const std::uint32_t sourceEnd = episode.firstSourceNeuron + episode.byteCount;
            const bool feedbackValid = synapse.kind == SynapseKind::Feedback &&
                synapse.sourceNeuron >= episode.firstSourceNeuron && synapse.sourceNeuron < sourceEnd && synapse.targetNeuron < kCodeNeuronCount;
            const bool sensoryValid = synapse.kind == SynapseKind::Sensory &&
                synapse.sourceNeuron < kCodeNeuronCount && synapse.targetNeuron >= episode.firstSourceNeuron && synapse.targetNeuron < sourceEnd;
            const bool sequenceValid = synapse.kind == SynapseKind::Sequence &&
                synapse.sourceNeuron >= episode.firstSourceNeuron && synapse.sourceNeuron < sourceEnd &&
                synapse.targetNeuron >= episode.firstSourceNeuron && synapse.targetNeuron < sourceEnd && synapse.targetNeuron != synapse.sourceNeuron;
            if (!std::isfinite(synapse.weight) || !std::isfinite(synapse.eligibility) || synapse.weight <= 0.0 || synapse.weight > 100.0 ||
                synapse.eligibility < 0.0 || synapse.eligibility > 1.0 || synapse.delaySteps != 1 ||
                (!feedbackValid && !sensoryValid && !sequenceValid))
                throw std::runtime_error("Invalid persisted synaptic transmission");
        }
        greatestId = std::max(greatestId, episode.id);
        greatestSourceEnd = std::max(greatestSourceEnd, episode.firstSourceNeuron + episode.byteCount);
        loaded.push_back(std::move(episode));
    }
    if (loadedNextId <= greatestId || loadedNextSource < greatestSourceEnd)
        throw std::runtime_error("Synaptic episodic memory counters are inconsistent");
    episodes_ = std::move(loaded);
    nextId_ = loadedNextId;
    nextSourceNeuron_ = loadedNextSource;
    plasticityUpdateCount_ = loadedPlasticityUpdates;
    lastRecallSpikeCount_ = 0;
    lastRecallFailureCount_ = 0;
}

void NeuralEpisodicMemory::clear() {
    episodes_.clear();
    nextId_ = 1;
    nextSourceNeuron_ = kFirstSourceNeuron;
    lastRecallSpikeCount_ = 0;
    lastRecallFailureCount_ = 0;
    plasticityUpdateCount_ = 0;
}

}  // namespace tatarus::llm
