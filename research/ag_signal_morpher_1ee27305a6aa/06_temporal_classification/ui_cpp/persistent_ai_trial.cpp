#include "cognitive_bridge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using agns::AttentionTarget;
using agns::CognitiveBridge;
using agns::CognitiveCommand;
using agns::CognitiveState;
using agns::NervousSystemConfig;
using agns::PersistentNervousSystem;
using agns::SensorFrame;

struct Episode {
    int first = 0;
    int second = 0;
    int variant = 0;
};

struct Sample {
    std::vector<double> features;
    int label = -1;
};

struct TrialResult {
    double nervousAccuracy = 0.0;
    double noTraceAccuracy = 0.0;
    double noNervousSystemAccuracy = 0.0;
    double experienceActionDiversity = 0.0;
    double retainedAccuracyAfterTaskSwitch = 0.0;
    bool snapshotExact = false;
    std::uint64_t finalStateHash = 0;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

NervousSystemConfig trialConfig(std::uint64_t seed, bool traceEnabled) {
    NervousSystemConfig config;
    config.seed = seed;
    config.baseCurrent = 12.0;
    config.connectionProbability = 0.07;
    config.eligibilityMemoryEnabled = traceEnabled;
    config.eligibilityTauMs = 800.0;
    config.eligibilityTransmissionGain = 10.0;
    config.eligibilityIncrement = 20.0;
    config.generatedOperatorEnabled = false;
    config.longTermPlasticityEnabled = false;
    config.structuralPlasticityEnabled = false;
    config.homeostasisEnabled = false;
    config.validate();
    return config;
}

std::vector<Episode> episodes(std::uint64_t seed, int count) {
    require(count % 4 == 0, "episode count must be divisible by four");
    std::mt19937_64 random(seed);
    std::vector<Episode> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int block = 0; block < count / 4; ++block) {
        std::array<Episode, 4> balanced{{
            {0, 1, block * 4},
            {1, 0, block * 4 + 1},
            {0, 1, block * 4 + 2},
            {1, 0, block * 4 + 3}}};
        std::shuffle(balanced.begin(), balanced.end(), random);
        result.insert(result.end(), balanced.begin(), balanced.end());
    }
    return result;
}

void blank(CognitiveBridge& bridge, int steps) {
    SensorFrame frame;
    frame.internalEnergy = 0.9;
    for (int step = 0; step < steps; ++step) {
        bridge.step(frame, CognitiveCommand{});
    }
}

std::vector<double> responseFeatures(
    CognitiveBridge& bridge,
    const Episode& episode) {
    const double high = 1.0 - 0.015 * static_cast<double>(episode.variant % 5);
    const double low = 0.04 + 0.01 * static_cast<double>(episode.variant % 3);
    SensorFrame first;
    first.internalEnergy = 0.9;
    first.visionEvents = episode.first
        ? std::vector<double>{high, low, 0.0, 0.0}
        : std::vector<double>{low, high, 0.0, 0.0};
    CognitiveCommand visualAttention;
    visualAttention.attention = AttentionTarget::Vision;
    for (int step = 0; step < 40; ++step) {
        bridge.step(first, visualAttention);
    }
    blank(bridge, 20);

    SensorFrame second;
    second.internalEnergy = 0.9;
    second.audioSamples = episode.second
        ? std::vector<double>{high, low}
        : std::vector<double>{low, high};
    CognitiveCommand audioAttention;
    audioAttention.attention = AttentionTarget::Audio;
    for (int step = 0; step < 40; ++step) {
        bridge.step(second, audioAttention);
    }
    blank(bridge, 400);

    SensorFrame recall;
    recall.internalEnergy = 0.9;
    recall.visionEvents = {0.5, 0.5, 0.5, 0.5};
    recall.audioSamples = {0.5, 0.5};
    recall.touch = {1.0, 1.0};
    recall.textBytes = {'R'};
    CognitiveCommand recallCommand;
    recallCommand.recallCue = 1U;
    recallCommand.recallStrength = 0.5;

    constexpr std::size_t groups = CognitiveBridge::recallGroupCount;
    std::vector<double> spikeSums(groups, 0.0);
    std::vector<double> membraneLast(groups, 0.0);
    std::vector<double> synapticRecallLast(groups, 0.0);
    CognitiveState finalState;
    for (int step = 0; step < 80; ++step) {
        finalState = bridge.step(recall, recallCommand).state;
        for (const auto& channel : finalState.recalledStates) {
            if (channel.channel < groups) {
                spikeSums[channel.channel] += channel.strength;
            } else if (channel.channel < 2U * groups) {
                membraneLast[channel.channel - groups] = channel.strength;
            } else if (channel.channel < 3U * groups) {
                synapticRecallLast[channel.channel - 2U * groups] =
                    channel.strength;
            }
        }
    }
    std::vector<double> result;
    result.reserve(63U);
    result.insert(result.end(), spikeSums.begin(), spikeSums.end());
    result.insert(result.end(), membraneLast.begin(), membraneLast.end());
    result.insert(
        result.end(),
        synapticRecallLast.begin(),
        synapticRecallLast.end());
    for (std::size_t index = 0; index < 8U; ++index) {
        result.push_back(
            index < finalState.activeRepresentations.size()
                ? finalState.activeRepresentations[index].activation
                : 0.0);
    }
    result.push_back(finalState.novelty);
    result.push_back(finalState.salience);
    result.push_back(finalState.energyNeed);
    result.push_back(finalState.activityNeed);
    result.push_back(finalState.predictionError);
    result.push_back(finalState.confidence);
    result.push_back(1.0);
    blank(bridge, 4000 + (episode.variant % 4) * 7);
    return result;
}

std::vector<double> solveLinear(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right) {
    const std::size_t size = right.size();
    for (std::size_t pivot = 0; pivot < size; ++pivot) {
        std::size_t best = pivot;
        for (std::size_t row = pivot + 1; row < size; ++row) {
            if (std::abs(matrix[row][pivot])
                > std::abs(matrix[best][pivot])) {
                best = row;
            }
        }
        std::swap(matrix[pivot], matrix[best]);
        std::swap(right[pivot], right[best]);
        const double divisor = matrix[pivot][pivot];
        if (std::abs(divisor) < 1e-12) continue;
        for (std::size_t column = pivot; column < size; ++column) {
            matrix[pivot][column] /= divisor;
        }
        right[pivot] /= divisor;
        for (std::size_t row = 0; row < size; ++row) {
            if (row == pivot) continue;
            const double factor = matrix[row][pivot];
            for (std::size_t column = pivot; column < size; ++column) {
                matrix[row][column] -= factor * matrix[pivot][column];
            }
            right[row] -= factor * right[pivot];
        }
    }
    return right;
}

class HigherPlanner {
public:
    void fit(const std::vector<Sample>& samples, double ridge = 2.0) {
        require(!samples.empty(), "planner received no experience");
        const std::size_t dimensions = samples.front().features.size();
        mean_.assign(dimensions, 0.0);
        scale_.assign(dimensions, 1.0);
        for (const auto& sample : samples) {
            require(
                sample.features.size() == dimensions,
                "inconsistent cognitive feature count");
            for (std::size_t i = 0; i < dimensions; ++i) {
                mean_[i] += sample.features[i];
            }
        }
        for (double& value : mean_) {
            value /= static_cast<double>(samples.size());
        }
        for (const auto& sample : samples) {
            for (std::size_t i = 0; i < dimensions; ++i) {
                const double difference = sample.features[i] - mean_[i];
                scale_[i] += difference * difference;
            }
        }
        for (double& value : scale_) {
            value = std::sqrt(value / static_cast<double>(samples.size()));
            value = std::max(value, 1e-6);
        }
        std::vector<std::vector<double>> normal(
            dimensions,
            std::vector<double>(dimensions, 0.0));
        std::vector<double> target(dimensions, 0.0);
        for (const auto& sample : samples) {
            std::vector<double> normalized(dimensions, 0.0);
            for (std::size_t i = 0; i < dimensions; ++i) {
                normalized[i] = (sample.features[i] - mean_[i]) / scale_[i];
            }
            for (std::size_t row = 0; row < dimensions; ++row) {
                target[row] +=
                    normalized[row] * static_cast<double>(sample.label);
                for (std::size_t column = 0; column < dimensions; ++column) {
                    normal[row][column] +=
                        normalized[row] * normalized[column];
                }
            }
        }
        for (std::size_t i = 0; i < dimensions; ++i) {
            normal[i][i] += ridge;
        }
        weights_ = solveLinear(std::move(normal), std::move(target));
    }

    int decide(const std::vector<double>& features) const {
        require(
            features.size() == weights_.size(),
            "planner feature count changed");
        double score = 0.0;
        for (std::size_t i = 0; i < features.size(); ++i) {
            score += weights_[i] * (features[i] - mean_[i]) / scale_[i];
        }
        return score >= 0.0 ? 1 : -1;
    }

    void save(const std::filesystem::path& path) const {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("planner state save failed");
        const std::uint64_t size = weights_.size();
        output.write(reinterpret_cast<const char*>(&size), sizeof(size));
        for (const auto* values : {&weights_, &mean_, &scale_}) {
            output.write(
                reinterpret_cast<const char*>(values->data()),
                static_cast<std::streamsize>(
                    values->size() * sizeof(double)));
        }
    }

    void load(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("planner state load failed");
        std::uint64_t size = 0;
        input.read(reinterpret_cast<char*>(&size), sizeof(size));
        require(size > 0U && size < 10000U, "invalid planner state");
        for (auto* values : {&weights_, &mean_, &scale_}) {
            values->resize(static_cast<std::size_t>(size));
            input.read(
                reinterpret_cast<char*>(values->data()),
                static_cast<std::streamsize>(
                    values->size() * sizeof(double)));
        }
        require(static_cast<bool>(input), "truncated planner state");
    }

private:
    std::vector<double> weights_;
    std::vector<double> mean_;
    std::vector<double> scale_;
};

void taskSwitch(CognitiveBridge& bridge, std::uint64_t seed) {
    std::mt19937_64 random(seed);
    const std::array<std::string, 5> unknownGrammar{
        "ZXQ|", "QXZ|", "ZZXQ|", "XQZQ|", "QZXX|"};
    for (int epoch = 0; epoch < 12; ++epoch) {
        const auto& sequence = unknownGrammar[
            static_cast<std::size_t>(random() % unknownGrammar.size())];
        for (char symbol : sequence) {
            SensorFrame frame;
            frame.internalEnergy = 0.9;
            frame.textBytes = {static_cast<std::uint8_t>(symbol)};
            frame.temperature =
                0.25 * std::sin(static_cast<double>(epoch + symbol));
            for (int step = 0; step < 24; ++step) {
                bridge.step(frame, CognitiveCommand{});
            }
        }
    }
    blank(bridge, 4000);
}

double constantBaselineAccuracy(const std::vector<Episode>& values) {
    int correctPositive = 0;
    int correctNegative = 0;
    for (std::size_t index = 48U; index < values.size(); ++index) {
        const int label = values[index].first ? 1 : -1;
        correctPositive += label == 1 ? 1 : 0;
        correctNegative += label == -1 ? 1 : 0;
    }
    return static_cast<double>(std::max(correctPositive, correctNegative))
        / static_cast<double>(values.size() - 48U);
}

struct SessionResult {
    double accuracy = 0.0;
    double diversity = 0.0;
    bool snapshotExact = true;
    std::uint64_t finalHash = 0;
};

SessionResult runSession(
    std::uint64_t seed,
    bool traceEnabled,
    const std::filesystem::path& checkpoint,
    bool verifySnapshot) {
    PersistentNervousSystem system(trialConfig(seed, traceEnabled));
    CognitiveBridge bridge(system);
    blank(bridge, 130);
    const auto schedule = episodes(seed + 77U, 64);
    std::vector<Sample> experience;
    experience.reserve(32U);
    for (std::size_t index = 0; index < 32U; ++index) {
        const auto features = responseFeatures(bridge, schedule[index]);
        experience.push_back(Sample{
            features,
            schedule[index].first ? 1 : -1});
    }
    HigherPlanner planner;
    planner.fit(experience);
    taskSwitch(bridge, seed + 991U);

    bool snapshotExact = true;
    if (verifySnapshot) {
        std::filesystem::create_directories(checkpoint);
        const auto nervousPath = checkpoint / "nervous_system.agns";
        const auto bridgePath = checkpoint / "cognitive_bridge.bin";
        const auto plannerPath = checkpoint / "higher_planner.bin";
        system.saveSnapshot(nervousPath);
        bridge.saveState(bridgePath);
        planner.save(plannerPath);
        const auto checkpointHash = system.stateHash();
        const auto previewFeatures = responseFeatures(bridge, schedule[32]);
        const int previewDecision = planner.decide(previewFeatures);
        const auto previewHash = system.stateHash();
        system.loadSnapshot(nervousPath);
        bridge.loadState(bridgePath);
        planner.load(plannerPath);
        snapshotExact =
            system.stateHash() == checkpointHash;
        const auto replayFeatures = responseFeatures(bridge, schedule[32]);
        const int replayDecision = planner.decide(replayFeatures);
        snapshotExact =
            snapshotExact
            && previewDecision == replayDecision
            && previewHash == system.stateHash()
            && previewFeatures == replayFeatures;
        system.loadSnapshot(nervousPath);
        bridge.loadState(bridgePath);
        planner.load(plannerPath);
    }

    int correct = 0;
    int positiveActions = 0;
    int measuredEpisodes = 0;
    for (std::size_t index = 32U; index < schedule.size(); ++index) {
        const auto features = responseFeatures(bridge, schedule[index]);
        const int decision = planner.decide(features);
        const int label =
            schedule[index].first ? 1 : -1;
        if (index >= 48U) {
            correct += decision == label ? 1 : 0;
            positiveActions += decision == 1 ? 1 : 0;
            ++measuredEpisodes;
        }
        SensorFrame consequence;
        consequence.internalEnergy = decision == label ? 0.95 : 0.75;
        consequence.reward = decision == label ? 1.0 : -1.0;
        consequence.touch = decision == label
            ? std::vector<double>{1.0, 0.0}
            : std::vector<double>{0.0, 1.0};
        CognitiveCommand command;
        command.motorIntent = static_cast<double>(decision);
        command.intentStrength = 0.8;
        command.reward = consequence.reward;
        for (int step = 0; step < 20; ++step) {
            bridge.step(consequence, command);
        }
        // Binary consequence reveals whether the selected intention was the
        // correct one. The planner updates only after acting and keeps a
        // bounded recent history so it can follow slow neural-state drift.
        const int inferredLabel =
            consequence.reward > 0.0 ? decision : -decision;
        experience.push_back(Sample{features, inferredLabel});
        if (experience.size() > 32U) {
            experience.erase(experience.begin());
        }
        planner.fit(experience);
    }
    const double fractionPositive =
        static_cast<double>(positiveActions)
        / static_cast<double>(measuredEpisodes);
    return SessionResult{
        static_cast<double>(correct)
            / static_cast<double>(measuredEpisodes),
        2.0 * std::min(fractionPositive, 1.0 - fractionPositive),
        snapshotExact,
        system.stateHash()};
}

TrialResult runTrial(
    std::uint64_t seed,
    const std::filesystem::path& output) {
    const auto full = runSession(
        seed,
        true,
        output / ("checkpoint_" + std::to_string(seed)),
        true);
    const auto noTrace = runSession(
        seed,
        false,
        output / "unused",
        false);
    const auto schedule = episodes(seed + 77U, 64);
    TrialResult result;
    result.nervousAccuracy = full.accuracy;
    result.noTraceAccuracy = noTrace.accuracy;
    result.noNervousSystemAccuracy = constantBaselineAccuracy(schedule);
    result.experienceActionDiversity = full.diversity;
    result.retainedAccuracyAfterTaskSwitch = full.accuracy;
    result.snapshotExact = full.snapshotExact;
    result.finalStateHash = full.finalHash;
    return result;
}

bool passes(const TrialResult& result) {
    return result.nervousAccuracy >= 0.75
        && result.noTraceAccuracy <= 0.65
        && result.noNervousSystemAccuracy <= 0.60
        && result.nervousAccuracy - std::max(
            result.noTraceAccuracy,
            result.noNervousSystemAccuracy) >= 0.15
        && result.experienceActionDiversity >= 0.50
        && result.snapshotExact;
}

void writeReports(
    const std::filesystem::path& output,
    const std::vector<std::pair<std::uint64_t, TrialResult>>& results,
    int requiredPasses) {
    int passCount = 0;
    double full = 0.0;
    double noTrace = 0.0;
    double noSystem = 0.0;
    double diversity = 0.0;
    std::ofstream csv(
        output / "persistent_ai_seeds.csv",
        std::ios::binary | std::ios::trunc);
    csv << "seed,nervous_accuracy,no_trace_accuracy,no_system_accuracy,"
           "action_diversity,snapshot_exact,state_hash,pass\n"
        << std::fixed << std::setprecision(9);
    for (const auto& [seed, result] : results) {
        const bool passed = passes(result);
        passCount += passed ? 1 : 0;
        full += result.nervousAccuracy;
        noTrace += result.noTraceAccuracy;
        noSystem += result.noNervousSystemAccuracy;
        diversity += result.experienceActionDiversity;
        csv << seed << "," << result.nervousAccuracy << ","
            << result.noTraceAccuracy << ","
            << result.noNervousSystemAccuracy << ","
            << result.experienceActionDiversity << ","
            << (result.snapshotExact ? 1 : 0) << ","
            << result.finalStateHash << ","
            << (passed ? 1 : 0) << "\n";
    }
    const double divisor = static_cast<double>(results.size());
    const bool confirmed = passCount >= requiredPasses;
    std::ofstream report(
        output / "STAGE19_PERSISTENT_AI_TRIAL.md",
        std::ios::binary | std::ios::trunc);
    report << "# Forschungsstufe 19 – Persistent AI Nervous System Trial\n\n"
        << "Status: `" << (confirmed ? "confirmed" : "not_confirmed")
        << "`\n\n"
        << "Eine höhere lineare Planungsschicht erhielt ausschließlich die "
           "funktionalen Cognitive-Bridge-Kanäle. Neuronen, Synapsen und "
           "Eligibility-Werte waren nicht sichtbar. Alle Episoden liefen "
           "ohne Systemreset in einem zusammenhängenden Lebenslauf.\n\n"
        << std::fixed << std::setprecision(6)
        << "| Messung | Mittelwert |\n|---|---:|\n"
        << "| gekoppelte KI mit lokalem Nervengedächtnis | "
        << full / divisor << " |\n"
        << "| identische Kopplung ohne Eligibility | "
        << noTrace / divisor << " |\n"
        << "| höhere KI ohne Nervensystem | "
        << noSystem / divisor << " |\n"
        << "| erfahrungsabhängige Aktionsdiversität | "
        << diversity / divisor << " |\n"
        << "| bestandene Seeds | " << passCount << "/"
        << results.size() << " |\n\n"
        << "Vor der Holdout-Entscheidung gelten pro Seed: Accuracy >=0,75, "
           "beide Kontrollen <=0,65/0,60, Vorteil >=0,15, beide "
           "Handlungsrichtungen aufgrund der Vorgeschichte und exakte "
           "komposite Snapshot-Fortsetzung.\n\n"
        << "Der Aufgabenwechsel bestand aus einer unbekannten rohen "
           "Bytegrammatik zwischen Lernen und Test. Der aktuelle Reiz im "
           "Entscheidungsfenster war klassenidentisch; nur die frühere "
           "multimodale Reihenfolge A->B oder B->A unterschied die richtige "
           "Handlung. Pulszahl und Eingabeenergie waren identisch.\n\n"
        << "Dies ist ein technischer Kopplungsnachweis, kein Nachweis "
           "allgemeiner Intelligenz oder einer offenen realen Umwelt.\n";
    std::ofstream json(
        output / "stage19_results.json",
        std::ios::binary | std::ios::trunc);
    json << "{\n"
        << "  \"status\": \"" << (confirmed ? "confirmed" : "not_confirmed")
        << "\",\n"
        << "  \"seeds\": " << results.size() << ",\n"
        << "  \"passes\": " << passCount << ",\n"
        << "  \"nervous_accuracy\": " << full / divisor << ",\n"
        << "  \"no_trace_accuracy\": " << noTrace / divisor << ",\n"
        << "  \"no_system_accuracy\": " << noSystem / divisor << ",\n"
        << "  \"snapshot_exact\": "
        << (std::all_of(
                results.begin(),
                results.end(),
                [](const auto& item) { return item.second.snapshotExact; })
            ? "true" : "false")
        << "\n}\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path output =
            argc > 1 ? argv[1] : "stage19_persistent_ai";
        const bool confirm =
            argc > 2 && std::string(argv[2]) == "--confirm";
        std::filesystem::create_directories(output);
        const int seedCount = confirm ? 8 : 4;
        const std::uint64_t firstSeed = confirm ? 31001U : 29001U;
        std::vector<std::pair<std::uint64_t, TrialResult>> results;
        for (int index = 0; index < seedCount; ++index) {
            const auto seed =
                firstSeed + static_cast<std::uint64_t>(index * 101);
            std::cout << "persistent AI seed " << (index + 1)
                << "/" << seedCount << "\n";
            results.push_back({seed, runTrial(seed, output)});
        }
        writeReports(output, results, confirm ? 6 : 3);
        const bool confirmed = static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& item) { return passes(item.second); }))
            >= (confirm ? 6 : 3);
        std::cout << "report="
            << (output / "STAGE19_PERSISTENT_AI_TRIAL.md").string()
            << "\n";
        return confirmed ? 0 : 3;
    } catch (const std::exception& error) {
        std::cerr << "persistent AI trial failed: " << error.what() << "\n";
        return 1;
    }
}
