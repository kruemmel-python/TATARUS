#include "cognitive_bridge.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using agns::AttentionTarget;
using agns::CognitiveBridge;
using agns::CognitiveCommand;
using agns::CognitiveState;
using agns::NervousSystemConfig;
using agns::PersistentNervousSystem;
using agns::SensorFrame;

struct OpenWorldResult {
    double nervousReward = 0.0;
    double noTraceReward = 0.0;
    double reflexReward = 0.0;
    double g5Reward = 0.0;
    int goals = 0;
    int passes = 0;
    int seeds = 0;
    bool finite = true;
    bool transferConfirmed = false;
};

struct MemoryResult {
    double episodicSignal = 0.0;
    double noTraceSignal = 0.0;
    double consolidationDelta = 0.0;
    double forgettingFraction = 0.0;
    double interferenceRetention = 0.0;
    int passes = 0;
    int seeds = 0;
    bool episodic = false;
    bool consolidation = false;
    bool forgetting = false;
    bool interferenceSafe = false;
};

struct ScaleResult {
    int neurons = 0;
    int synapses = 0;
    int steps = 0;
    double buildMilliseconds = 0.0;
    double simulationMilliseconds = 0.0;
    double realtimeFactor = 0.0;
    double eventsPerSecond = 0.0;
    std::uintmax_t snapshotBytes = 0;
    double snapshotSaveMilliseconds = 0.0;
    double snapshotLoadMilliseconds = 0.0;
    bool deterministicSnapshot = false;
    bool safe = false;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double mean(const std::vector<double>& values) {
    return values.empty()
        ? 0.0
        : std::accumulate(values.begin(), values.end(), 0.0)
            / static_cast<double>(values.size());
}

double cosine(
    const std::vector<double>& left,
    const std::vector<double>& right) {
    if (left.size() != right.size() || left.empty()) return 0.0;
    double dot = 0.0;
    double l2 = 0.0;
    double r2 = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        dot += left[i] * right[i];
        l2 += left[i] * left[i];
        r2 += right[i] * right[i];
    }
    return dot / (std::sqrt(l2 * r2) + 1e-12);
}

double norm(const std::vector<double>& values) {
    double square = 0.0;
    for (const double value : values) square += value * value;
    return std::sqrt(square);
}

NervousSystemConfig researchConfig(std::uint64_t seed, bool trace = true) {
    NervousSystemConfig config;
    config.seed = seed;
    config.baseCurrent = 12.0;
    config.eligibilityMemoryEnabled = trace;
    config.eligibilityTauMs = 800.0;
    config.eligibilityTransmissionGain = 10.0;
    config.eligibilityIncrement = 20.0;
    config.generatedOperatorEnabled = false;
    config.structuralPlasticityEnabled = false;
    config.homeostasisEnabled = true;
    config.validate();
    return config;
}

void bridgeBlank(CognitiveBridge& bridge, int steps) {
    SensorFrame frame;
    frame.internalEnergy = 0.9;
    for (int step = 0; step < steps; ++step) {
        bridge.step(frame, CognitiveCommand{});
    }
}

class ProceduralLifeWorld {
public:
    explicit ProceduralLifeWorld(std::uint64_t seed)
        : random_(seed) {
        resetSituation();
    }

    SensorFrame sense(int step, bool g5) const {
        SensorFrame frame;
        const double bearing = target_ - position_;
        const double hazardBearing = hazard_ - position_;
        const double targetLeft = bearing < 0.0
            ? std::min(1.0, std::abs(bearing)) : 0.0;
        const double targetRight = bearing >= 0.0
            ? std::min(1.0, std::abs(bearing)) : 0.0;
        const double hazardNear =
            std::max(0.0, 1.0 - 3.0 * std::abs(hazardBearing));
        const double noise =
            0.08 * std::sin(static_cast<double>(step) * 0.173);
        frame.visionEvents = {
            std::clamp(targetLeft + noise, -1.0, 1.0),
            std::clamp(targetRight - noise, -1.0, 1.0),
            hazardBearing < 0.0 ? hazardNear : 0.0,
            hazardBearing >= 0.0 ? hazardNear : 0.0};
        frame.audioSamples = {
            rule_ < 0 ? 1.0 : 0.0,
            rule_ > 0 ? 1.0 : 0.0};
        frame.touch = {
            std::abs(hazardBearing) < 0.10 ? 1.0 : 0.0,
            energy_ < 0.30 ? 1.0 : 0.0};
        const std::string grammar = g5 ? "XZYZX" : grammars_[
            static_cast<std::size_t>(situation_ % grammars_.size())];
        frame.textBytes = {
            static_cast<std::uint8_t>(
                grammar[static_cast<std::size_t>(step) % grammar.size()])};
        frame.temperature = hazardNear;
        frame.internalEnergy = energy_;
        frame.novelty =
            step % ruleInterval_ < 30 ? 1.0 : 0.05 * hazardNear;
        frame.reward = lastReward_;
        return frame;
    }

    double apply(double movement, int step, bool g5) {
        const double previousDistance = std::abs(target_ - position_);
        const double intended =
            (g5 && (situation_ % 2 == 1) ? -1.0 : 1.0)
            * static_cast<double>(rule_)
            * std::clamp(movement, -1.0, 1.0);
        position_ = std::clamp(position_ + 0.035 * intended, -1.0, 1.0);
        const double distance = std::abs(target_ - position_);
        const double hazardDistance = std::abs(hazard_ - position_);
        lastReward_ =
            4.0 * (previousDistance - distance)
            - (hazardDistance < 0.12 ? 0.08 : 0.0)
            - 0.001 * std::abs(movement);
        energy_ = std::clamp(
            energy_ - 0.0008 - 0.001 * std::abs(movement),
            0.0,
            1.0);
        if (distance < 0.08) {
            lastReward_ += 0.35;
            energy_ = std::min(1.0, energy_ + 0.30);
            ++goals_;
            resetSituation();
        } else if (step > 0 && step % ruleInterval_ == 0) {
            rule_ = -rule_;
            ++situation_;
        }
        reward_ += lastReward_;
        return lastReward_;
    }

    [[nodiscard]] double reward() const { return reward_; }
    [[nodiscard]] int goals() const { return goals_; }

private:
    void resetSituation() {
        std::uniform_real_distribution<double> location(-0.85, 0.85);
        target_ = location(random_);
        do {
            hazard_ = location(random_);
        } while (std::abs(hazard_ - target_) < 0.25);
        ++situation_;
    }

    std::mt19937_64 random_;
    double position_ = 0.0;
    double target_ = 0.65;
    double hazard_ = -0.35;
    double energy_ = 0.85;
    double reward_ = 0.0;
    double lastReward_ = 0.0;
    int rule_ = 1;
    int situation_ = 0;
    int goals_ = 0;
    int ruleInterval_ = 480;
    const std::array<std::string, 4> grammars_{
        "ABAC", "BCBA", "CADA", "DBCD"};
};

struct AdaptiveController {
    std::vector<double> weights;
    std::vector<double> pendingFeatures;
    double pendingScore = 0.0;
    double selectedAction = 1.0;
    int decisions = 0;

    std::vector<double> features(const CognitiveState& state) const {
        auto values = agns::cognitiveFeatureVector(state);
        const std::size_t original = values.size();
        const std::size_t interactions =
            std::min<std::size_t>(96U, original / 2U);
        values.reserve(original + interactions);
        for (std::size_t index = 0; index < interactions; ++index) {
            const std::size_t other =
                (index * 37U + 19U) % original;
            values.push_back(values[index] * values[other]);
        }
        return values;
    }

    CognitiveCommand command(const CognitiveState& state) {
        pendingFeatures = features(state);
        if (weights.size() != pendingFeatures.size()) {
            weights.assign(pendingFeatures.size(), 0.0);
        }
        pendingScore = 0.0;
        for (std::size_t index = 0; index < weights.size(); ++index) {
            pendingScore += weights[index] * pendingFeatures[index];
        }
        const bool explore =
            decisions < 160 || decisions % 47 == 0;
        selectedAction = explore
            ? (decisions % 2 == 0 ? -1.0 : 1.0)
            : (pendingScore >= 0.0 ? 1.0 : -1.0);
        ++decisions;
        CognitiveCommand command;
        command.attention =
            state.energyNeed > 0.35
                ? AttentionTarget::Interoception
                : AttentionTarget::Vision;
        command.motorIntent = selectedAction;
        command.intentStrength = 2.0;
        command.recallCue = 3U;
        command.recallStrength = 0.0;
        return command;
    }

    void observe(double reward, double) {
        if (pendingFeatures.empty()) return;
        const double desired =
            reward >= -0.001 ? selectedAction : -selectedAction;
        if (desired * pendingScore < 0.5) {
            constexpr double learningRate = 0.025;
            for (std::size_t index = 0; index < weights.size(); ++index) {
                weights[index] = std::clamp(
                    0.9995 * weights[index]
                        + learningRate * desired * pendingFeatures[index],
                    -4.0,
                    4.0);
            }
        }
    }
};

double runWorld(
    std::uint64_t seed,
    bool trace,
    bool reflexOnly,
    bool g5,
    int* goals,
    bool* finite) {
    ProceduralLifeWorld world(seed + 100U);
    AdaptiveController controller;
    if (reflexOnly) {
        for (int step = 0; step < 3200; ++step) {
            const auto frame = world.sense(step, g5);
            const double left =
                frame.visionEvents.empty() ? 0.0 : frame.visionEvents[0];
            const double right =
                frame.visionEvents.size() < 2U ? 0.0 : frame.visionEvents[1];
            const double movement = std::tanh(2.0 * (right - left));
            const double reward = world.apply(movement, step, g5);
            controller.observe(reward, movement);
        }
        if (goals) *goals = world.goals();
        if (finite) *finite = std::isfinite(world.reward());
        return world.reward();
    }

    auto config = researchConfig(seed, trace);
    config.longTermPlasticityEnabled = true;
    config.structuralPlasticityEnabled = true;
    PersistentNervousSystem system(config);
    CognitiveBridge bridge(system);
    CognitiveState state = bridge.readState();
    for (int step = 0; step < 3200; ++step) {
        auto command = controller.command(state);
        const auto result = bridge.step(world.sense(step, g5), command);
        const double reward = world.apply(result.action.movement, step, g5);
        controller.observe(reward, result.action.movement);
        state = result.state;
    }
    if (goals) *goals = world.goals();
    if (finite) {
        *finite =
            system.metrics().finite
            && system.dalePrincipleHolds()
            && std::isfinite(world.reward());
    }
    return world.reward();
}

OpenWorldResult stage20Seed(std::uint64_t seed) {
    OpenWorldResult result;
    result.nervousReward =
        runWorld(seed, true, false, false, &result.goals, &result.finite);
    result.noTraceReward =
        runWorld(seed, false, false, false, nullptr, nullptr);
    result.reflexReward =
        runWorld(seed, false, true, false, nullptr, nullptr);
    result.g5Reward =
        runWorld(seed + 500U, true, false, true, nullptr, nullptr);
    const double bestControl =
        std::max(result.noTraceReward, result.reflexReward);
    result.transferConfirmed =
        result.finite
        && result.goals > 0
        && result.nervousReward > bestControl
        && result.g5Reward > 0.0;
    return result;
}

OpenWorldResult stage20(
    const std::vector<std::uint64_t>& seeds,
    std::vector<OpenWorldResult>* raw) {
    OpenWorldResult aggregate;
    aggregate.finite = true;
    aggregate.seeds = static_cast<int>(seeds.size());
    for (const auto seed : seeds) {
        const auto value = stage20Seed(seed);
        if (raw) raw->push_back(value);
        aggregate.nervousReward += value.nervousReward;
        aggregate.noTraceReward += value.noTraceReward;
        aggregate.reflexReward += value.reflexReward;
        aggregate.g5Reward += value.g5Reward;
        aggregate.goals += value.goals;
        aggregate.finite = aggregate.finite && value.finite;
        aggregate.passes += value.transferConfirmed ? 1 : 0;
    }
    const double divisor = static_cast<double>(seeds.size());
    aggregate.nervousReward /= divisor;
    aggregate.noTraceReward /= divisor;
    aggregate.reflexReward /= divisor;
    aggregate.g5Reward /= divisor;
    aggregate.goals = static_cast<int>(std::llround(
        static_cast<double>(aggregate.goals) / divisor));
    aggregate.transferConfirmed =
        aggregate.finite && aggregate.passes >= 6;
    return aggregate;
}

std::vector<double> recallPool(
    CognitiveBridge& bridge,
    std::uint32_t cue,
    int steps = 80) {
    SensorFrame recall;
    recall.internalEnergy = 0.9;
    recall.visionEvents = {0.5, 0.5, 0.5, 0.5};
    recall.audioSamples = {0.5, 0.5};
    recall.touch = {1.0, 1.0};
    recall.textBytes = {'R'};
    CognitiveCommand command;
    command.recallCue = cue;
    command.recallStrength = 0.5;
    std::vector<double> pooled(CognitiveBridge::recallGroupCount, 0.0);
    for (int step = 0; step < steps; ++step) {
        const auto state = bridge.step(recall, command).state;
        for (const auto& channel : state.recalledStates) {
            const auto base = 2U * CognitiveBridge::recallGroupCount;
            if (channel.channel >= base
                && channel.channel
                    < 3U * CognitiveBridge::recallGroupCount) {
                pooled[channel.channel - base] = channel.strength;
            }
        }
    }
    return pooled;
}

void experience(
    CognitiveBridge& bridge,
    int kind,
    int repetitions,
    double reward) {
    SensorFrame frame;
    frame.internalEnergy = 0.9;
    frame.visionEvents = kind == 0
        ? std::vector<double>{1.0, 0.05, 0.0, 0.0}
        : std::vector<double>{0.05, 1.0, 0.0, 0.0};
    frame.audioSamples = kind == 0
        ? std::vector<double>{0.9, 0.1}
        : std::vector<double>{0.1, 0.9};
    frame.textBytes = {
        static_cast<std::uint8_t>(kind == 0 ? 'E' : 'N')};
    CognitiveCommand command;
    command.attention = AttentionTarget::Vision;
    command.reward = reward;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        for (int step = 0; step < 40; ++step) {
            bridge.step(frame, command);
        }
        bridgeBlank(bridge, 30);
    }
}

double meanEligibility(const PersistentNervousSystem& system) {
    double sum = 0.0;
    int count = 0;
    for (const auto& synapse : system.inspect().synapses) {
        if (!synapse.active) continue;
        sum += std::abs(synapse.eligibility);
        ++count;
    }
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

double consolidatedMagnitude(const PersistentNervousSystem& system) {
    double sum = 0.0;
    for (const auto& synapse : system.inspect().synapses) {
        if (synapse.active) sum += std::abs(synapse.consolidatedWeight);
    }
    return sum;
}

std::vector<double> partialCueProbe(
    CognitiveBridge& bridge,
    int kind) {
    SensorFrame frame;
    frame.internalEnergy = 0.9;
    frame.visionEvents = kind == 0
        ? std::vector<double>{0.85, 0.10, 0.0, 0.0}
        : std::vector<double>{0.10, 0.85, 0.0, 0.0};
    CognitiveCommand command;
    command.attention = AttentionTarget::Vision;
    CognitiveState state;
    for (int step = 0; step < 40; ++step) {
        state = bridge.step(frame, command).state;
    }
    return agns::cognitiveFeatureVector(state);
}

MemoryResult stage21Seed(std::uint64_t seed) {
    auto config = researchConfig(seed, true);
    config.baseCurrent = 8.0;
    config.homeostasisEnabled = false;
    config.learningRate = 0.003;
    config.consolidationRate = 0.001;
    config.structuralPlasticityEnabled = false;
    PersistentNervousSystem system(config);
    CognitiveBridge bridge(system);
    bridgeBlank(bridge, 200);
    const double consolidatedBefore = consolidatedMagnitude(system);
    experience(bridge, 0, 1, 0.8);
    bridgeBlank(bridge, 400);
    const auto episodic = recallPool(bridge, 11U);

    auto controlConfig = config;
    controlConfig.seed = seed;
    controlConfig.eligibilityMemoryEnabled = false;
    PersistentNervousSystem control(controlConfig);
    CognitiveBridge controlBridge(control);
    bridgeBlank(controlBridge, 200);
    experience(controlBridge, 0, 1, 0.8);
    bridgeBlank(controlBridge, 400);
    const auto controlRecall = recallPool(controlBridge, 11U);

    experience(bridge, 0, 16, 1.0);
    const double eligibilityBeforeForgetting = meanEligibility(system);
    bridgeBlank(bridge, 8000);
    const double eligibilityAfterForgetting = meanEligibility(system);
    const double consolidatedAfter = consolidatedMagnitude(system);
    const auto beforeInterference = partialCueProbe(bridge, 0);
    bridgeBlank(bridge, 1200);
    experience(bridge, 1, 16, 1.0);
    bridgeBlank(bridge, 3000);
    const auto afterInterference = partialCueProbe(bridge, 0);

    MemoryResult result;
    result.episodicSignal = norm(episodic);
    result.noTraceSignal = norm(controlRecall);
    result.consolidationDelta =
        consolidatedAfter - consolidatedBefore;
    result.forgettingFraction =
        eligibilityBeforeForgetting > 1e-12
            ? 1.0
                - eligibilityAfterForgetting
                    / eligibilityBeforeForgetting
            : 0.0;
    result.interferenceRetention =
        cosine(beforeInterference, afterInterference);
    result.episodic =
        result.episodicSignal > result.noTraceSignal + 0.01;
    result.consolidation = result.consolidationDelta > 1e-6;
    result.forgetting = result.forgettingFraction > 0.90;
    result.interferenceSafe = result.interferenceRetention > 0.60;
    return result;
}

MemoryResult stage21(
    const std::vector<std::uint64_t>& seeds,
    std::vector<MemoryResult>* raw) {
    MemoryResult aggregate;
    aggregate.seeds = static_cast<int>(seeds.size());
    int episodicPasses = 0;
    int consolidationPasses = 0;
    int forgettingPasses = 0;
    int interferencePasses = 0;
    for (const auto seed : seeds) {
        const auto value = stage21Seed(seed);
        if (raw) raw->push_back(value);
        aggregate.episodicSignal += value.episodicSignal;
        aggregate.noTraceSignal += value.noTraceSignal;
        aggregate.consolidationDelta += value.consolidationDelta;
        aggregate.forgettingFraction += value.forgettingFraction;
        aggregate.interferenceRetention += value.interferenceRetention;
        episodicPasses += value.episodic ? 1 : 0;
        consolidationPasses += value.consolidation ? 1 : 0;
        forgettingPasses += value.forgetting ? 1 : 0;
        interferencePasses += value.interferenceSafe ? 1 : 0;
        aggregate.passes +=
            value.episodic && value.consolidation
                && value.forgetting && value.interferenceSafe
            ? 1 : 0;
    }
    const double divisor = static_cast<double>(seeds.size());
    aggregate.episodicSignal /= divisor;
    aggregate.noTraceSignal /= divisor;
    aggregate.consolidationDelta /= divisor;
    aggregate.forgettingFraction /= divisor;
    aggregate.interferenceRetention /= divisor;
    aggregate.episodic = episodicPasses >= 6;
    aggregate.consolidation = consolidationPasses >= 6;
    aggregate.forgetting = forgettingPasses >= 6;
    aggregate.interferenceSafe = interferencePasses >= 6;
    return aggregate;
}

NervousSystemConfig scaleConfig(int total, std::uint64_t seed) {
    NervousSystemConfig config;
    config.seed = seed;
    config.sensoryNeurons = std::min(32, std::max(4, total / 64));
    config.motorNeurons = std::min(16, std::max(4, total / 128));
    config.modulatoryNeurons = std::min(8, std::max(1, total / 512));
    config.contextNeurons = std::min(128, std::max(4, total / 16));
    const int remaining =
        total - config.sensoryNeurons - config.motorNeurons
        - config.modulatoryNeurons - config.contextNeurons;
    config.inhibitoryNeurons = std::max(1, remaining / 5);
    config.excitatoryNeurons = remaining - config.inhibitoryNeurons;
    config.connectionProbability =
        std::min(0.07, 32.0 / static_cast<double>(total - 1));
    config.maximumAssemblies = 64;
    config.maximumNewSynapsesPerInterval = 4;
    config.structuralIntervalMs = 250.0;
    config.validate();
    return config;
}

ScaleResult benchmarkScale(
    int neurons,
    int steps,
    std::uint64_t seed,
    const std::filesystem::path& output) {
    const auto config = scaleConfig(neurons, seed);
    const auto buildStart = Clock::now();
    PersistentNervousSystem system(config);
    const auto buildEnd = Clock::now();
    const auto initialTransmissions = system.metrics().totalTransmissions;
    const auto simulationStart = Clock::now();
    for (int step = 0; step < steps; ++step) {
        SensorFrame frame;
        frame.internalEnergy = 0.9;
        frame.visionEvents = {
            std::sin(static_cast<double>(step) * 0.07),
            std::cos(static_cast<double>(step) * 0.11)};
        frame.audioSamples = {
            std::sin(static_cast<double>(step) * 0.17)};
        frame.reward = step % 97 == 0 ? 0.2 : 0.0;
        frame.novelty = step % 131 == 0 ? 0.5 : 0.0;
        system.step(frame);
    }
    const auto simulationEnd = Clock::now();
    const auto snapshot = output / (
        "scale_" + std::to_string(neurons) + ".agns");
    const auto saveStart = Clock::now();
    system.saveSnapshot(snapshot);
    const auto saveEnd = Clock::now();
    const auto expectedHash = system.stateHash();
    const auto loadStart = Clock::now();
    PersistentNervousSystem restored(config);
    restored.loadSnapshot(snapshot);
    const auto loadEnd = Clock::now();
    const double simulationMs =
        std::chrono::duration<double, std::milli>(
            simulationEnd - simulationStart).count();
    const auto transmissions =
        system.metrics().totalTransmissions - initialTransmissions;
    ScaleResult result;
    result.neurons = neurons;
    result.synapses = system.metrics().activeSynapses;
    result.steps = steps;
    result.buildMilliseconds =
        std::chrono::duration<double, std::milli>(
            buildEnd - buildStart).count();
    result.simulationMilliseconds = simulationMs;
    result.realtimeFactor =
        simulationMs > 0.0
            ? (static_cast<double>(steps) * config.dtMs) / simulationMs
            : 0.0;
    result.eventsPerSecond =
        simulationMs > 0.0
            ? static_cast<double>(transmissions) * 1000.0 / simulationMs
            : 0.0;
    result.snapshotBytes = std::filesystem::file_size(snapshot);
    result.snapshotSaveMilliseconds =
        std::chrono::duration<double, std::milli>(
            saveEnd - saveStart).count();
    result.snapshotLoadMilliseconds =
        std::chrono::duration<double, std::milli>(
            loadEnd - loadStart).count();
    result.deterministicSnapshot = restored.stateHash() == expectedHash;
    result.safe =
        system.metrics().finite
        && system.dalePrincipleHolds()
        && result.deterministicSnapshot
        && system.metrics().meanEnergy >= 0.0
        && system.metrics().meanEnergy <= 1.0
        && system.metrics().activeSynapses
            <= neurons * 40 + config.sensoryNeurons * 5
                + config.motorNeurons;
    return result;
}

std::uint64_t fileHash(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::uint64_t hash = 1469598103934665603ULL;
    std::array<char, 16384> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        const auto count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(
                buffer[static_cast<std::size_t>(i)]);
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void writeReplicationKit(
    const std::filesystem::path& output,
    const OpenWorldResult& world,
    const MemoryResult& memory,
    const std::vector<ScaleResult>& scales) {
    const auto kit = output / "replication_kit";
    std::filesystem::create_directories(kit);
    std::ofstream seeds(kit / "independent_seeds.txt");
    std::mt19937_64 random(0x5233504C494341ULL);
    for (int index = 0; index < 24; ++index) {
        seeds << (40000U + random() % 900000U) << "\n";
    }
    std::ofstream script(kit / "run_clean_replication.bat");
    script
        << "@echo off\n"
        << "setlocal\n"
        << "set \"ROOT=%~dp0..\\..\\..\\..\\..\\..\"\n"
        << "set \"SRC=%ROOT%\\research\\ag_signal_morpher_1ee27305a6aa"
           "\\06_temporal_classification\\ui_cpp\"\n"
        << "call \"%SRC%\\build_ui.bat\" build_replication\n"
        << "if errorlevel 1 exit /b 1\n"
        << "pushd \"%SRC%\"\n"
        << "build_replication\\AGRepresentationResearch.exe "
           "replication_stage18 --confirm\n"
        << "if errorlevel 1 exit /b 1\n"
        << "build_replication\\AGPersistentAITrial.exe "
           "replication_stage19 --confirm\n"
        << "if errorlevel 1 exit /b 1\n"
        << "build_replication\\AGStage20To23.exe "
           "replication_stage20_23 --replication "
           "\"%~dp0independent_seeds.txt\"\n"
        << "set \"RESULT=%ERRORLEVEL%\"\n"
        << "popd\n"
        << "exit /b %RESULT%\n";
    std::ofstream protocol(kit / "REPLICATION_PROTOCOL.md");
    protocol
        << "# Unabhängiges Replikationsprotokoll\n\n"
        << "1. Repository auf einen zweiten Rechner kopieren.\n"
        << "2. MSVC 2022 Build Tools und optional OpenCL installieren.\n"
        << "3. `run_clean_replication.bat` ausführen.\n"
        << "4. CPU, GPU, Compiler, Betriebssystem und Treiberversion notieren.\n"
        << "5. JSON-Status, Seed-CSVs und Hashmanifest zurückgeben.\n\n"
        << "Eine lokale Ausführung ist keine unabhängige Replikation. Der "
           "Status bleibt deshalb `package_ready`, bis Ergebnisse eines "
           "zweiten Rechners importiert wurden.\n";
    std::ofstream verifier(kit / "verify_external_results.ps1");
    verifier
        << "param([Parameter(Mandatory=$true)][string]$ResultRoot)\n"
        << "$s18 = Get-Content -Raw -LiteralPath "
           "(Join-Path $ResultRoot 'replication_stage18\\stage18_confirmation.json') | ConvertFrom-Json\n"
        << "$s19 = Get-Content -Raw -LiteralPath "
           "(Join-Path $ResultRoot 'replication_stage19\\stage19_results.json') | ConvertFrom-Json\n"
        << "$s23 = Get-Content -Raw -LiteralPath "
           "(Join-Path $ResultRoot 'replication_stage20_23\\stage20_23_results.json') | ConvertFrom-Json\n"
        << "if ($s18.status -ne 'confirmed' -or $s19.status -ne 'confirmed' "
           "-or $s23.stage20 -ne 'confirmed' -or $s23.stage21 -ne 'confirmed' "
           "-or $s23.maximum_executed_neurons -lt 16384) { "
           "throw 'Independent replication criteria not met' }\n"
        << "Write-Output 'INDEPENDENT REPLICATION RESULT: PASS'\n";
    std::ofstream expected(kit / "expected_results.json");
    expected << std::fixed << std::setprecision(9)
        << "{\n"
        << "  \"stage18_status\": \"confirmed\",\n"
        << "  \"stage19_status\": \"confirmed\",\n"
        << "  \"stage20_reference_reward\": " << world.nervousReward << ",\n"
        << "  \"stage21_episodic_signal\": " << memory.episodicSignal << ",\n"
        << "  \"maximum_validated_neurons\": "
        << (scales.empty() ? 0 : scales.back().neurons) << "\n"
        << "}\n";
    expected.close();
    protocol.close();
    script.close();
    seeds.close();
    verifier.close();
    std::ofstream manifest(kit / "replication_manifest.json");
    manifest << "{\n"
        << "  \"schema\": \"agns-replication-v1\",\n"
        << "  \"status\": \"package_ready_local_validation_only\",\n"
        << "  \"hardware_threads\": " << std::thread::hardware_concurrency()
        << ",\n"
        << "  \"artifacts\": {\n"
        << "    \"expected_results.json\": \""
        << std::hex << std::uppercase
        << fileHash(kit / "expected_results.json") << "\",\n"
        << "    \"independent_seeds.txt\": \""
        << fileHash(kit / "independent_seeds.txt") << "\",\n"
        << "    \"run_clean_replication.bat\": \""
        << fileHash(kit / "run_clean_replication.bat") << "\",\n"
        << "    \"verify_external_results.ps1\": \""
        << fileHash(kit / "verify_external_results.ps1") << "\"\n"
        << "  }\n}\n";
}

void writeReports(
    const std::filesystem::path& output,
    const OpenWorldResult& world,
    const MemoryResult& memory,
    const std::vector<std::uint64_t>& worldRunSeeds,
    const std::vector<std::uint64_t>& memoryRunSeeds,
    const std::vector<OpenWorldResult>& worldSeeds,
    const std::vector<MemoryResult>& memorySeeds,
    const std::vector<ScaleResult>& scales,
    bool fullScale) {
    std::ofstream seedCsv(output / "stage20_21_seeds.csv");
    seedCsv
        << "index,world_seed,memory_seed,world_reward,no_trace_reward,reflex_reward,g5_reward,"
           "world_goals,world_pass,episodic_signal,no_trace_signal,"
           "consolidation_delta,forgetting_fraction,"
           "interference_retention,memory_pass\n"
        << std::fixed << std::setprecision(9);
    const std::size_t seedRows =
        std::min(worldSeeds.size(), memorySeeds.size());
    for (std::size_t index = 0; index < seedRows; ++index) {
        const auto& w = worldSeeds[index];
        const auto& m = memorySeeds[index];
        const bool memoryPass =
            m.episodic && m.consolidation
            && m.forgetting && m.interferenceSafe;
        seedCsv << index << "," << worldRunSeeds[index] << ","
            << memoryRunSeeds[index] << "," << w.nervousReward << ","
            << w.noTraceReward << "," << w.reflexReward << ","
            << w.g5Reward << "," << w.goals << ","
            << (w.transferConfirmed ? 1 : 0) << ","
            << m.episodicSignal << "," << m.noTraceSignal << ","
            << m.consolidationDelta << "," << m.forgettingFraction << ","
            << m.interferenceRetention << ","
            << (memoryPass ? 1 : 0) << "\n";
    }
    std::ofstream csv(output / "scaling.csv");
    csv << "neurons,synapses,steps,build_ms,simulation_ms,realtime_factor,"
           "events_per_second,snapshot_bytes,save_ms,load_ms,"
           "snapshot_exact,safe\n"
        << std::fixed << std::setprecision(6);
    for (const auto& scale : scales) {
        csv << scale.neurons << "," << scale.synapses << ","
            << scale.steps << "," << scale.buildMilliseconds << ","
            << scale.simulationMilliseconds << ","
            << scale.realtimeFactor << ","
            << scale.eventsPerSecond << ","
            << scale.snapshotBytes << ","
            << scale.snapshotSaveMilliseconds << ","
            << scale.snapshotLoadMilliseconds << ","
            << (scale.deterministicSnapshot ? 1 : 0) << ","
            << (scale.safe ? 1 : 0) << "\n";
    }
    const bool memoryConfirmed =
        memory.episodic && memory.consolidation
        && memory.forgetting && memory.interferenceSafe;
    const bool scalingSafe = std::all_of(
        scales.begin(),
        scales.end(),
        [](const auto& scale) { return scale.safe; });
    std::ofstream report(output / "STAGE20_TO_23_REPORT.md");
    report << "# Forschungsstufen 20–23 – Gesamtvalidierung\n\n"
        << std::fixed << std::setprecision(6)
        << "## Stufe 20: Open Lifeworld und G5\n\n"
        << "- Nervensystem-Reward: " << world.nervousReward << "\n"
        << "- Ohne-Eligibility-Reward: " << world.noTraceReward << "\n"
        << "- statischer Reflex-Reward: " << world.reflexReward << "\n"
        << "- G5-Reward: " << world.g5Reward << "\n"
        << "- erreichte Ziele: " << world.goals << "\n"
        << "- bestandene Seeds: " << world.passes << "/"
        << world.seeds << "\n"
        << "- Status: "
        << (world.transferConfirmed ? "bestätigt" : "nicht bestätigt")
        << "\n\n"
        << "Die Welt erzeugt freie Objektlagen, Gefahr, Energiebedarf, "
           "verzögerte Konsequenzen, Regelwechsel und G5-Ereignisse. Ein "
           "positiver Lauf belegt nur diese prozedurale Welt.\n\n"
        << "## Stufe 21: Mehrskaliges Gedächtnis\n\n"
        << "- episodisches Recall-Signal / Kontrolle: "
        << memory.episodicSignal << " / " << memory.noTraceSignal << "\n"
        << "- konsolidierte Gewichtsänderung: "
        << memory.consolidationDelta << "\n"
        << "- kontrolliert vergessener Eligibility-Anteil: "
        << memory.forgettingFraction << "\n"
        << "- Retention nach Interferenz: "
        << memory.interferenceRetention << "\n"
        << "- vollständig bestandene Seeds: " << memory.passes << "/"
        << memory.seeds << "\n"
        << "- Status: "
        << (memoryConfirmed ? "bestätigt" : "nicht vollständig bestätigt")
        << "\n\n"
        << "## Stufe 22: Skalierung\n\n"
        << "- größte ausgeführte Population: "
        << (scales.empty() ? 0 : scales.back().neurons) << "\n"
        << "- 65.536-Neuronen-Lauf: "
        << (fullScale ? "ausgeführt" : "implementiert, aus Kostengründen nicht standardmäßig ausgeführt")
        << "\n"
        << "- alle ausgeführten Sicherheitsprüfungen: "
        << (scalingSafe ? "bestanden" : "nicht bestanden") << "\n\n"
        << "Details stehen in `scaling.csv`.\n\n"
        << "## Stufe 23: Replikation\n\n"
        << "Status: `package_ready_local_validation_only`\n\n"
        << "Das Verzeichnis `replication_kit` enthält neue Seedlisten, "
           "Clean-Build-Skript, Erwartungswerte, Protokoll und Hashmanifest. "
           "Eine unabhängige Bestätigung erfordert weiterhin einen zweiten "
           "Rechner und kann lokal nicht wahrheitsgemäß behauptet werden.\n";
    std::ofstream json(output / "stage20_23_results.json");
    json << "{\n"
        << "  \"stage20\": \""
        << (world.transferConfirmed ? "confirmed" : "not_confirmed")
        << "\",\n"
        << "  \"stage21\": \""
        << (memoryConfirmed ? "confirmed" : "not_fully_confirmed")
        << "\",\n"
        << "  \"stage22\": \""
        << (scalingSafe ? "validated_executed_sizes" : "failed")
        << "\",\n"
        << "  \"stage23\": \"package_ready_external_run_pending\",\n"
        << "  \"maximum_executed_neurons\": "
        << (scales.empty() ? 0 : scales.back().neurons) << "\n"
        << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path output =
            argc > 1 ? argv[1] : "stage20_23_results";
        const bool fullScale =
            argc > 2 && std::string(argv[2]) == "--full-scale";
        const bool replication =
            argc > 2 && std::string(argv[2]) == "--replication";
        std::filesystem::create_directories(output);
        std::vector<std::uint64_t> allSeeds;
        if (replication) {
            require(argc > 3, "--replication requires a seed file");
            std::ifstream seedInput(argv[3]);
            std::uint64_t seed = 0;
            while (seedInput >> seed) allSeeds.push_back(seed);
            require(
                allSeeds.size() >= 16U,
                "replication seed file requires at least 16 seeds");
        } else {
            for (int index = 0; index < 8; ++index) {
                allSeeds.push_back(
                    40001U + static_cast<std::uint64_t>(index * 101));
            }
            for (int index = 0; index < 8; ++index) {
                allSeeds.push_back(
                    42001U + static_cast<std::uint64_t>(index * 101));
            }
        }
        const std::vector<std::uint64_t> worldRunSeeds(
            allSeeds.begin(),
            allSeeds.begin() + 8);
        const std::vector<std::uint64_t> memoryRunSeeds(
            allSeeds.begin() + 8,
            allSeeds.begin() + 16);
        std::cout << "stage20 open lifeworld\n";
        std::vector<OpenWorldResult> worldSeeds;
        const auto world = stage20(worldRunSeeds, &worldSeeds);
        std::cout << "stage21 multiscale memory\n";
        std::vector<MemoryResult> memorySeeds;
        const auto memory = stage21(memoryRunSeeds, &memorySeeds);
        std::cout << "stage22 scaling\n";
        std::vector<std::pair<int, int>> sizes{
            {256, 600}, {1024, 300}, {4096, 120}, {16384, 40}};
        if (fullScale) sizes.push_back({65536, 40});
        std::vector<ScaleResult> scales;
        for (const auto& [neurons, steps] : sizes) {
            std::cout << "  " << neurons << " neurons\n";
            scales.push_back(benchmarkScale(
                neurons,
                steps,
                38001U + static_cast<std::uint64_t>(neurons),
                output));
        }
        std::cout << "stage23 replication kit\n";
        writeReplicationKit(output, world, memory, scales);
        writeReports(
            output,
            world,
            memory,
            worldRunSeeds,
            memoryRunSeeds,
            worldSeeds,
            memorySeeds,
            scales,
            fullScale);
        std::cout << "report="
            << (output / "STAGE20_TO_23_REPORT.md").string() << "\n";
        const bool safe = std::all_of(
            scales.begin(),
            scales.end(),
            [](const auto& scale) { return scale.safe; });
        return safe ? 0 : 3;
    } catch (const std::exception& error) {
        std::cerr << "stage20-23 failed: " << error.what() << "\n";
        return 1;
    }
}
