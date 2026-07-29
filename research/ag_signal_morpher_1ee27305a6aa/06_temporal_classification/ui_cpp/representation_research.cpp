#include "nervous_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using agns::NervousSystemConfig;
using agns::PersistentNervousSystem;
using agns::RepresentationState;
using agns::SensorFrame;

struct Sample {
    std::vector<double> feature;
    int label = 0;
    int group = 0;
};

struct AssemblyResult {
    double withinSimilarity = 0.0;
    double betweenSimilarity = 0.0;
    double similarCueReactivation = 0.0;
    double crossModalSimilarity = 0.0;
    double postDamageSimilarity = 0.0;
    int assembliesBefore = 0;
    int assembliesAfter = 0;
    bool snapshotExact = false;
};

struct SequenceResult {
    double transitionAccuracy = 0.0;
    double boundaryResponseRatio = 0.0;
    double repetitionStability = 0.0;
    int transitionClasses = 0;
};

struct MemoryResult {
    double developmentAccuracy = 0.0;
    double traceAccuracy = 0.0;
    double noTraceAccuracy = 0.0;
    double advantage = 0.0;
    double meanAbsoluteTraceAtRecall = 0.0;
    double selectedTauMs = 0.0;
    double selectedGain = 0.0;
    double selectedIncrement = 0.0;
    bool traceEssential = false;
};

struct MemoryParameters {
    double tauMs = 800.0;
    double gain = 6.0;
    double increment = 20.0;
};

struct RecoveryResult {
    double baselineFunction = 0.0;
    double immediateFunction = 0.0;
    double recoveredFunction = 0.0;
    double lostFunction = 0.0;
    double regainedFraction = 0.0;
    double pathwayJaccard = 0.0;
    int disabledNeurons = 0;
    int disabledSynapses = 0;
    int newActiveSynapses = 0;
    int newUsedSynapses = 0;
    int inheritedRepairSynapses = 0;
    bool functionRestored = false;
    bool sameMappingRepair = false;
    bool alternativeSolution = false;
};

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    const std::size_t size = std::min(a.size(), b.size());
    double value = 0.0;
    for (std::size_t i = 0; i < size; ++i) value += a[i] * b[i];
    return value;
}

double norm(const std::vector<double>& value) {
    return std::sqrt(dot(value, value));
}

double cosine(const std::vector<double>& a, const std::vector<double>& b) {
    return dot(a, b) / (norm(a) * norm(b) + 1e-12);
}

double distance(const std::vector<double>& a, const std::vector<double>& b) {
    const std::size_t size = std::min(a.size(), b.size());
    double sum = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        const double delta = a[i] - b[i];
        sum += delta * delta;
    }
    return std::sqrt(sum);
}

std::vector<double> centroid(
    const std::vector<Sample>& samples,
    int label,
    int maximumGroup,
    int minimumGroup = std::numeric_limits<int>::min()) {
    std::vector<double> result;
    int count = 0;
    for (const auto& sample : samples) {
        if (sample.label != label
            || sample.group > maximumGroup
            || sample.group < minimumGroup) {
            continue;
        }
        if (result.empty()) result.assign(sample.feature.size(), 0.0);
        for (std::size_t i = 0; i < result.size(); ++i) {
            result[i] += sample.feature[i];
        }
        ++count;
    }
    if (count > 0) {
        for (double& value : result) value /= static_cast<double>(count);
    }
    return result;
}

double nearestCentroidAccuracy(
    const std::vector<Sample>& samples,
    int classCount,
    int trainMaximumGroup,
    int testMinimumGroup) {
    std::vector<std::vector<double>> centroids;
    for (int label = 0; label < classCount; ++label) {
        centroids.push_back(centroid(samples, label, trainMaximumGroup));
    }
    int correct = 0;
    int count = 0;
    for (const auto& sample : samples) {
        if (sample.group < testMinimumGroup) continue;
        int best = -1;
        double bestDistance = std::numeric_limits<double>::infinity();
        for (int label = 0; label < classCount; ++label) {
            if (centroids[static_cast<std::size_t>(label)].empty()) continue;
            const double current = distance(
                sample.feature,
                centroids[static_cast<std::size_t>(label)]);
            if (current < bestDistance) {
                bestDistance = current;
                best = label;
            }
        }
        correct += best == sample.label ? 1 : 0;
        ++count;
    }
    return count > 0
        ? static_cast<double>(correct) / static_cast<double>(count)
        : 0.0;
}

std::vector<double> solveLinearSystem(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right) {
    const std::size_t size = right.size();
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row][column])
                > std::abs(matrix[pivot][column])) {
                pivot = row;
            }
        }
        if (pivot != column) {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(right[pivot], right[column]);
        }
        const double diagonal = matrix[column][column];
        if (std::abs(diagonal) < 1e-14) continue;
        for (std::size_t row = column + 1; row < size; ++row) {
            const double factor = matrix[row][column] / diagonal;
            if (factor == 0.0) continue;
            for (std::size_t item = column; item < size; ++item) {
                matrix[row][item] -= factor * matrix[column][item];
            }
            right[row] -= factor * right[column];
        }
    }
    std::vector<double> solution(size, 0.0);
    for (std::size_t reverse = 0; reverse < size; ++reverse) {
        const std::size_t row = size - 1 - reverse;
        double value = right[row];
        for (std::size_t column = row + 1; column < size; ++column) {
            value -= matrix[row][column] * solution[column];
        }
        if (std::abs(matrix[row][row]) > 1e-14) {
            solution[row] = value / matrix[row][row];
        }
    }
    return solution;
}

double ridgeAccuracy(
    const std::vector<Sample>& samples,
    int trainMaximumGroup,
    int testMinimumGroup,
    double regularization) {
    if (samples.empty()) return 0.0;
    const std::size_t rawDimensions = samples.front().feature.size();
    const std::size_t dimensions = rawDimensions + 1;
    std::vector<double> mean(rawDimensions, 0.0);
    std::vector<double> scale(rawDimensions, 0.0);
    int trainCount = 0;
    for (const auto& sample : samples) {
        if (sample.group > trainMaximumGroup) continue;
        for (std::size_t i = 0; i < rawDimensions; ++i) {
            mean[i] += sample.feature[i];
        }
        ++trainCount;
    }
    if (trainCount == 0) return 0.0;
    for (double& value : mean) value /= static_cast<double>(trainCount);
    for (const auto& sample : samples) {
        if (sample.group > trainMaximumGroup) continue;
        for (std::size_t i = 0; i < rawDimensions; ++i) {
            const double delta = sample.feature[i] - mean[i];
            scale[i] += delta * delta;
        }
    }
    for (double& value : scale) {
        value = std::sqrt(value / static_cast<double>(trainCount));
        if (value < 1e-9) value = 1.0;
    }
    auto transformed = [&](const Sample& sample) {
        std::vector<double> feature(dimensions, 1.0);
        for (std::size_t i = 0; i < rawDimensions; ++i) {
            feature[i] = (sample.feature[i] - mean[i]) / scale[i];
        }
        return feature;
    };
    std::vector<std::vector<double>> gram(
        dimensions,
        std::vector<double>(dimensions, 0.0));
    std::vector<double> target(dimensions, 0.0);
    for (const auto& sample : samples) {
        if (sample.group > trainMaximumGroup) continue;
        const auto feature = transformed(sample);
        const double label = sample.label == 1 ? 1.0 : -1.0;
        for (std::size_t row = 0; row < dimensions; ++row) {
            target[row] += feature[row] * label;
            for (std::size_t column = 0; column < dimensions; ++column) {
                gram[row][column] += feature[row] * feature[column];
            }
        }
    }
    for (std::size_t index = 0; index + 1 < dimensions; ++index) {
        gram[index][index] += regularization;
    }
    const auto weights = solveLinearSystem(std::move(gram), std::move(target));
    int correct = 0;
    int count = 0;
    for (const auto& sample : samples) {
        if (sample.group < testMinimumGroup) continue;
        const auto feature = transformed(sample);
        const int prediction = dot(feature, weights) >= 0.0 ? 1 : 0;
        correct += prediction == sample.label ? 1 : 0;
        ++count;
    }
    return count > 0
        ? static_cast<double>(correct) / static_cast<double>(count)
        : 0.0;
}

NervousSystemConfig researchConfig(std::uint64_t seed) {
    NervousSystemConfig config;
    config.sensoryNeurons = 16;
    config.excitatoryNeurons = 28;
    config.inhibitoryNeurons = 8;
    config.contextNeurons = 12;
    config.motorNeurons = 6;
    config.modulatoryNeurons = 2;
    config.seed = seed;
    config.connectionProbability = 0.10;
    config.baseCurrent = 7.0;
    config.generatedOperatorEnabled = false;
    config.eligibilityTauMs = 400.0;
    config.structuralIntervalMs = 250.0;
    config.maximumNewSynapsesPerInterval = 5;
    config.maximumAssemblies = 32;
    config.validate();
    return config;
}

std::vector<double> evokedFeature(
    const RepresentationState& before,
    const RepresentationState& after) {
    std::vector<double> feature;
    for (std::size_t i = 0; i < after.neurons.size(); ++i) {
        const auto role = after.neurons[i].role;
        if (role == agns::PopulationRole::Sensory
            || role == agns::PopulationRole::Modulatory) {
            continue;
        }
        feature.push_back(static_cast<double>(
            after.neurons[i].spikeCount - before.neurons[i].spikeCount));
        feature.push_back(
            (after.neurons[i].dendriteMv - before.neurons[i].dendriteMv) / 20.0);
    }
    return feature;
}

void runFrame(PersistentNervousSystem& system, const SensorFrame& frame, int steps) {
    for (int step = 0; step < steps; ++step) system.step(frame);
}

SensorFrame cue(int kind, bool similar = false, bool vision = true, bool audio = false) {
    SensorFrame frame;
    frame.internalEnergy = 0.9;
    if (vision) {
        frame.visionEvents = kind == 0
            ? std::vector<double>{1.0, similar ? 0.12 : 0.0, 0.35, 0.0}
            : std::vector<double>{0.0, 1.0, 0.0, 0.35};
    }
    if (audio) {
        frame.audioSamples = kind == 0
            ? std::vector<double>{0.9, -0.2, 0.4}
            : std::vector<double>{-0.2, 0.9, -0.4};
    }
    return frame;
}

void neutral(PersistentNervousSystem& system, int steps) {
    SensorFrame frame;
    frame.internalEnergy = 0.9;
    runFrame(system, frame, steps);
}

double pairSimilarity(const std::vector<std::vector<double>>& values) {
    if (values.size() < 2) return 0.0;
    double sum = 0.0;
    int count = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            sum += cosine(values[i], values[j]);
            ++count;
        }
    }
    return sum / static_cast<double>(count);
}

AssemblyResult assemblyExperiment(
    const std::filesystem::path& output,
    std::uint64_t seed = 17001) {
    auto config = researchConfig(seed);
    config.homeostasisEnabled = false;
    config.assemblySimilarityThreshold = 0.92;
    PersistentNervousSystem system(config);
    neutral(system, 300);
    std::array<std::vector<std::vector<double>>, 2> responses;
    for (int repetition = 0; repetition < 20; ++repetition) {
        const int kind = repetition % 2;
        const auto baseline = system.inspect();
        runFrame(system, cue(kind, false, true, true), 24);
        responses[static_cast<std::size_t>(kind)].push_back(
            evokedFeature(baseline, system.inspect()));
        neutral(system, 36);
    }
    const auto centroidA = centroid(
        [&]() {
            std::vector<Sample> samples;
            for (const auto& feature : responses[0]) samples.push_back({feature, 0, 0});
            return samples;
        }(), 0, 0);
    const auto centroidB = centroid(
        [&]() {
            std::vector<Sample> samples;
            for (const auto& feature : responses[1]) samples.push_back({feature, 1, 0});
            return samples;
        }(), 1, 0);

    const auto similarBaseline = system.inspect();
    runFrame(system, cue(0, true, true, false), 24);
    const auto similarResponse = evokedFeature(
        similarBaseline, system.inspect());
    neutral(system, 40);
    const auto audioBaseline = system.inspect();
    runFrame(system, cue(0, false, false, true), 24);
    const auto audioOnlyResponse = evokedFeature(
        audioBaseline, system.inspect());

    const auto before = system.inspect();
    const auto snapshot = output / (
        "representation_snapshot_" + std::to_string(seed) + ".agns");
    system.saveSnapshot(snapshot);
    PersistentNervousSystem restored(config);
    restored.loadSnapshot(snapshot);
    const bool snapshotExact = system.stateHash() == restored.stateHash();
    restored.applyDamage(0.10, 0.15, seed + 1);
    neutral(restored, 300);
    const auto damagedBaseline = restored.inspect();
    runFrame(restored, cue(0, true, true, false), 24);
    const auto damagedResponse = evokedFeature(
        damagedBaseline, restored.inspect());

    AssemblyResult result;
    result.withinSimilarity =
        0.5 * (pairSimilarity(responses[0]) + pairSimilarity(responses[1]));
    result.betweenSimilarity = cosine(centroidA, centroidB);
    result.similarCueReactivation = cosine(centroidA, similarResponse);
    result.crossModalSimilarity = cosine(centroidA, audioOnlyResponse);
    result.postDamageSimilarity = cosine(centroidA, damagedResponse);
    result.assembliesBefore = static_cast<int>(before.assemblies.size());
    result.assembliesAfter = static_cast<int>(restored.inspect().assemblies.size());
    result.snapshotExact = snapshotExact;
    return result;
}

SequenceResult sequenceExperiment(std::uint64_t seed = 17101) {
    auto config = researchConfig(seed);
    config.structuralPlasticityEnabled = false;
    config.homeostasisEnabled = false;
    PersistentNervousSystem system(config);
    neutral(system, 200);
    const std::array<std::string, 2> streams{"ABABABAB|", "ABACABAC|"};
    const std::array<std::string, 6> transitions{"AB", "BA", "AC", "CA", "B|", "C|"};
    std::vector<Sample> samples;
    std::vector<double> boundaryChanges;
    std::vector<double> ordinaryChanges;
    std::vector<std::vector<double>> earlyAb;
    std::vector<std::vector<double>> lateAb;
    for (int epoch = 0; epoch < 36; ++epoch) {
        const auto& stream = streams[static_cast<std::size_t>(epoch % 2)];
        char previous = '\0';
        auto previousState = system.inspect();
        for (char current : stream) {
            SensorFrame frame;
            frame.internalEnergy = 0.9;
            frame.textBytes.push_back(static_cast<std::uint8_t>(current));
            runFrame(system, frame, 24);
            const auto currentState = system.inspect();
            const auto feature = evokedFeature(previousState, currentState);
            if (previous != '\0') {
                const auto transitionFeature = feature;
                std::string transition;
                transition.push_back(previous);
                transition.push_back(current);
                const auto found = std::find(
                    transitions.begin(), transitions.end(), transition);
                if (found != transitions.end()) {
                    const int label = static_cast<int>(
                        std::distance(transitions.begin(), found));
                    samples.push_back({transitionFeature, label, epoch});
                    if (transition == "AB") {
                        (epoch < 8 ? earlyAb : lateAb).push_back(transitionFeature);
                    }
                }
                const double change = norm(feature);
                (current == '|' ? boundaryChanges : ordinaryChanges).push_back(change);
            }
            previous = current;
            previousState = currentState;
        }
        neutral(system, 20);
    }
    SequenceResult result;
    result.transitionClasses = static_cast<int>(transitions.size());
    result.transitionAccuracy = nearestCentroidAccuracy(samples, 6, 23, 24);
    const auto average = [](const std::vector<double>& values) {
        return values.empty() ? 0.0
            : std::accumulate(values.begin(), values.end(), 0.0)
                / static_cast<double>(values.size());
    };
    result.boundaryResponseRatio =
        average(boundaryChanges) / (average(ordinaryChanges) + 1e-12);
    result.repetitionStability = cosine(
        centroid(
            [&]() {
                std::vector<Sample> converted;
                for (const auto& f : earlyAb) converted.push_back({f, 0, 0});
                return converted;
            }(), 0, 0),
        centroid(
            [&]() {
                std::vector<Sample> converted;
                for (const auto& f : lateAb) converted.push_back({f, 0, 0});
                return converted;
            }(), 0, 0));
    return result;
}

std::vector<double> recallTrial(
    std::uint64_t seed,
    int firstBit,
    int secondBit,
    bool eligibilityEnabled,
    const MemoryParameters& parameters,
    int variant,
    double* meanAbsoluteEligibility = nullptr) {
    auto config = researchConfig(seed);
    config.baseCurrent = 12.0;
    config.eligibilityMemoryEnabled = eligibilityEnabled;
    config.eligibilityTauMs = parameters.tauMs;
    config.eligibilityTransmissionGain = parameters.gain;
    config.eligibilityIncrement = parameters.increment;
    config.generatedOperatorEnabled = false;
    config.longTermPlasticityEnabled = false;
    config.structuralPlasticityEnabled = false;
    config.homeostasisEnabled = false;
    PersistentNervousSystem system(config);
    neutral(system, 130 + variant * 7);
    const double high = 1.0 - 0.02 * static_cast<double>(variant % 4);
    const double low = 0.04 + 0.01 * static_cast<double>(variant % 3);
    SensorFrame first;
    first.internalEnergy = 0.9;
    first.visionEvents = firstBit
        ? std::vector<double>{high, low, 0.0, 0.0}
        : std::vector<double>{low, high, 0.0, 0.0};
    runFrame(system, first, 40);
    neutral(system, 20);
    SensorFrame second;
    second.internalEnergy = 0.9;
    second.audioSamples = secondBit
        ? std::vector<double>{high, low}
        : std::vector<double>{low, high};
    runFrame(system, second, 40);
    neutral(system, 400);
    const auto before = system.inspect();
    if (meanAbsoluteEligibility) {
        double sum = 0.0;
        int count = 0;
        for (const auto& synapse : before.synapses) {
            if (!synapse.active) continue;
            sum += std::abs(synapse.eligibility);
            ++count;
        }
        *meanAbsoluteEligibility =
            count > 0 ? sum / static_cast<double>(count) : 0.0;
    }
    SensorFrame recall;
    recall.internalEnergy = 0.9;
    recall.visionEvents = {0.5, 0.5, 0.5, 0.5};
    recall.audioSamples = {0.5, 0.5};
    recall.touch = {1.0, 1.0};
    recall.textBytes = {'R'};
    runFrame(system, recall, 80);
    const auto after = system.inspect();
    std::vector<double> response;
    for (std::size_t i = 0; i < after.neurons.size(); ++i) {
        const auto role = after.neurons[i].role;
        if (role == agns::PopulationRole::Sensory
            || role == agns::PopulationRole::Modulatory) continue;
        response.push_back(static_cast<double>(
            after.neurons[i].spikeCount - before.neurons[i].spikeCount));
        response.push_back((after.neurons[i].somaMv + 65.0) / 20.0);
    }
    return response;
}

double memoryAccuracy(
    bool eligibilityEnabled,
    const MemoryParameters& parameters,
    std::uint64_t firstNetworkSeed,
    int networkSeedCount,
    double* meanAbsoluteEligibility = nullptr) {
    double accuracySum = 0.0;
    double eligibilitySum = 0.0;
    int eligibilityCount = 0;
    for (int seedIndex = 0; seedIndex < networkSeedCount; ++seedIndex) {
        const auto seed =
            firstNetworkSeed + static_cast<std::uint64_t>(seedIndex);
        std::vector<Sample> samples;
        for (int variant = 0; variant < 8; ++variant) {
            for (int first = 0; first < 2; ++first) {
                for (int second = 0; second < 2; ++second) {
                    double eligibility = 0.0;
                    samples.push_back({
                        recallTrial(
                            seed,
                            first,
                            second,
                            eligibilityEnabled,
                            parameters,
                            variant,
                            &eligibility),
                        first ^ second,
                        variant});
                    eligibilitySum += eligibility;
                    ++eligibilityCount;
                }
            }
        }
        accuracySum += ridgeAccuracy(samples, 4, 5, 1.0);
    }
    if (meanAbsoluteEligibility) {
        *meanAbsoluteEligibility = eligibilityCount > 0
            ? eligibilitySum / static_cast<double>(eligibilityCount)
            : 0.0;
    }
    return networkSeedCount > 0
        ? accuracySum / static_cast<double>(networkSeedCount)
        : 0.0;
}

MemoryResult memoryExperiment() {
    MemoryResult result;
    MemoryParameters best;
    const std::array<double, 3> taus{400.0, 800.0, 1600.0};
    const std::array<double, 3> gains{2.0, 6.0, 10.0};
    const std::array<double, 2> increments{8.0, 20.0};
    for (double tau : taus) {
        for (double gain : gains) {
            for (double increment : increments) {
                const MemoryParameters candidate{tau, gain, increment};
                const double accuracy = memoryAccuracy(
                    true,
                    candidate,
                    17200U,
                    5);
                if (accuracy > result.developmentAccuracy) {
                    result.developmentAccuracy = accuracy;
                    best = candidate;
                }
            }
        }
    }
    result.selectedTauMs = best.tauMs;
    result.selectedGain = best.gain;
    result.selectedIncrement = best.increment;
    result.traceAccuracy = memoryAccuracy(
        true,
        best,
        18000U,
        12,
        &result.meanAbsoluteTraceAtRecall);
    result.noTraceAccuracy = memoryAccuracy(
        false,
        best,
        18000U,
        12);
    result.advantage = result.traceAccuracy - result.noTraceAccuracy;
    result.traceEssential =
        result.traceAccuracy >= 0.70
        && result.noTraceAccuracy <= 0.60
        && result.advantage >= 0.15;
    return result;
}

double functionalProbe(const std::filesystem::path& snapshot,
                       const NervousSystemConfig& config) {
    double score = 0.0;
    for (int direction = 0; direction < 2; ++direction) {
        PersistentNervousSystem probe(config);
        probe.loadSnapshot(snapshot);
        constexpr int probeSteps = 120;
        double movement = 0.0;
        for (int step = 0; step < probeSteps; ++step) {
            SensorFrame frame;
            frame.internalEnergy = 0.9;
            frame.visionEvents = direction == 0
                ? std::vector<double>{1.0, 0.0, 0.0, 0.0}
                : std::vector<double>{0.0, 1.0, 0.0, 0.0};
            movement += probe.step(frame).movement;
        }
        const double signedMovement =
            direction == 0 ? -movement : movement;
        score += std::clamp(
            signedMovement / static_cast<double>(probeSteps),
            -1.0,
            1.0);
    }
    return score / 2.0;
}

std::vector<std::size_t> topUsed(const RepresentationState& state, std::size_t count) {
    std::vector<std::size_t> indices;
    for (const auto& synapse : state.synapses) if (synapse.active) indices.push_back(synapse.index);
    std::sort(indices.begin(), indices.end(), [&](std::size_t left, std::size_t right) {
        return state.synapses[left].usage > state.synapses[right].usage;
    });
    if (indices.size() > count) indices.resize(count);
    std::sort(indices.begin(), indices.end());
    return indices;
}

RecoveryResult recoveryExperiment(
    const std::filesystem::path& output,
    std::uint64_t seed = 17301) {
    auto config = researchConfig(seed);
    config.baseCurrent = 12.5;
    config.homeostasisEnabled = false;
    config.structuralIntervalMs = 400.0;
    config.maximumNewSynapsesPerInterval = 8;
    config.learningRate = 0.003;
    config.eligibilityTauMs = 30.0;
    config.dopamineTauMs = 20.0;
    config.motorRateScaleHz = 2.0;
    PersistentNervousSystem system(config);
    // Establish an actually used sensor-motor function before lesioning it.
    // Structural repair may only inherit a path that carried activity before
    // the damage; otherwise the provenance claim would be circular.
    for (int step = 0; step < 300; ++step) {
        const int direction = (step / 30) % 2;
        SensorFrame frame;
        frame.internalEnergy = 0.9;
        frame.visionEvents = direction == 0
            ? std::vector<double>{1.0, 0.0, 0.0, 0.0}
            : std::vector<double>{0.0, 1.0, 0.0, 0.0};
        system.step(frame);
    }
    neutral(system, 300);
    const auto baselineState = system.inspect();
    const auto baselineSnapshot = output / (
        "recovery_baseline_" + std::to_string(seed) + ".agns");
    system.saveSnapshot(baselineSnapshot);
    const double baselineFunction = functionalProbe(baselineSnapshot, config);

    const int motorStart =
        config.sensoryNeurons + config.excitatoryNeurons
        + config.inhibitoryNeurons + config.contextNeurons;
    const int motorEnd = motorStart + config.motorNeurons;
    std::vector<std::size_t> functionalSynapses;
    for (const auto& synapse : baselineState.synapses) {
        if (!synapse.active) continue;
        const bool entersMotor =
            synapse.post >= static_cast<std::uint32_t>(motorStart)
            && synapse.post < static_cast<std::uint32_t>(motorEnd);
        const bool directDirectional =
            (synapse.pre == 0U || synapse.pre == 1U)
            && entersMotor;
        if (directDirectional) functionalSynapses.push_back(synapse.index);
    }
    std::sort(functionalSynapses.begin(), functionalSynapses.end());
    functionalSynapses.erase(
        std::unique(functionalSynapses.begin(), functionalSynapses.end()),
        functionalSynapses.end());
    const auto targetedDamage = system.disableSynapses(functionalSynapses);
    const auto neuronDamage = system.applyDamageWithReport(
        0.10,
        0.0,
        seed + 2);
    const auto damagedSnapshot = output / (
        "recovery_damaged_" + std::to_string(seed) + ".agns");
    system.saveSnapshot(damagedSnapshot);
    const double immediateFunction = functionalProbe(damagedSnapshot, config);

    for (int step = 0; step < 300; ++step) {
        const int direction = (step / 30) % 2;
        SensorFrame frame;
        frame.internalEnergy = 0.9;
        frame.novelty = 0.5;
        frame.visionEvents = direction == 0
            ? std::vector<double>{1.0, 0.0, 0.0, 0.0}
            : std::vector<double>{0.0, 1.0, 0.0, 0.0};
        system.step(frame);
    }
    system.setStructuralPlasticityEnabled(false);
    neutral(system, 1000);
    const auto recoveredState = system.inspect();
    const auto recoveredSnapshot = output / (
        "recovery_after_learning_" + std::to_string(seed) + ".agns");
    system.saveSnapshot(recoveredSnapshot);
    const double recoveredFunction = functionalProbe(recoveredSnapshot, config);

    const auto baselineTop = topUsed(baselineState, 100);
    const auto recoveredTop = topUsed(recoveredState, 100);
    std::vector<std::size_t> intersection;
    std::set_intersection(
        baselineTop.begin(), baselineTop.end(),
        recoveredTop.begin(), recoveredTop.end(),
        std::back_inserter(intersection));
    const std::size_t unionSize =
        baselineTop.size() + recoveredTop.size() - intersection.size();

    int newActive = 0;
    int newUsed = 0;
    int inheritedRepairs = 0;
    for (const auto& synapse : recoveredState.synapses) {
        if (synapse.index >= baselineState.synapses.size() && synapse.active) {
            ++newActive;
            if (synapse.usage > 0.1) ++newUsed;
            if (synapse.parentSynapse >= 0) ++inheritedRepairs;
        }
    }
    RecoveryResult result;
    result.baselineFunction = baselineFunction;
    result.immediateFunction = immediateFunction;
    result.recoveredFunction = recoveredFunction;
    result.lostFunction =
        std::abs(baselineFunction) - std::abs(immediateFunction);
    result.regainedFraction = std::abs(result.lostFunction) > 1e-9
        ? (
            std::abs(recoveredFunction)
            - std::abs(immediateFunction))
            / result.lostFunction
        : 0.0;
    result.pathwayJaccard = unionSize > 0
        ? static_cast<double>(intersection.size()) / static_cast<double>(unionSize)
        : 1.0;
    result.disabledNeurons = static_cast<int>(
        neuronDamage.disabledNeurons.size());
    result.disabledSynapses = static_cast<int>(
        targetedDamage.disabledSynapses.size());
    result.newActiveSynapses = newActive;
    result.newUsedSynapses = newUsed;
    result.inheritedRepairSynapses = inheritedRepairs;
    result.functionRestored =
        std::abs(baselineFunction) >= 0.03
        && result.lostFunction > 0.004
        && baselineFunction * recoveredFunction > 0.0
        && recoveredFunction
            != 0.0
        && result.regainedFraction >= 0.70;
    result.sameMappingRepair =
        result.functionRestored && inheritedRepairs > 0;
    result.alternativeSolution =
        result.functionRestored
        && inheritedRepairs == 0
        && newUsed > 0
        && result.pathwayJaccard < 0.8;
    return result;
}

void writeReport(
    const std::filesystem::path& output,
    const AssemblyResult& assembly,
    const SequenceResult& sequence,
    const MemoryResult& memory,
    const RecoveryResult& recovery) {
    std::ofstream report(output / "STAGE17_REPORT.md", std::ios::binary | std::ios::trunc);
    report << "# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung\n\n"
        << "Status: `measured_experimental`\n\n"
        << std::fixed << std::setprecision(6)
        << "## Repräsentationsbildung\n\n"
        << "| Metrik | Wert |\n|---|---:|\n"
        << "| Ähnlichkeit gleicher Reize über Zeit | " << assembly.withinSimilarity << " |\n"
        << "| Ähnlichkeit verschiedener Reize | " << assembly.betweenSimilarity << " |\n"
        << "| Reaktivierung durch ähnlichen Reiz | " << assembly.similarCueReactivation << " |\n"
        << "| Audio-only zu gelernter multimodaler Repräsentation | " << assembly.crossModalSimilarity << " |\n"
        << "| Repräsentationsähnlichkeit nach Schaden | " << assembly.postDamageSimilarity << " |\n"
        << "| Assemblies vorher / nachher | " << assembly.assembliesBefore
        << " / " << assembly.assembliesAfter << " |\n"
        << "| Snapshot bitgenau | " << (assembly.snapshotExact ? "ja" : "nein") << " |\n\n"
        << "## Tokenizerfreie Sequenzbildung\n\n"
        << "- Übergangsklassifikation auf unberührten Epochen: "
        << sequence.transitionAccuracy << "\n"
        << "- Übergangsklassen: " << sequence.transitionClasses << "\n"
        << "- Grenzreaktion / normale Übergangsreaktion: "
        << sequence.boundaryResponseRatio << "\n"
        << "- Stabilität AB früh zu spät: " << sequence.repetitionStability << "\n\n"
        << "## Reizfreies Recall-Gedächtnis\n\n"
        << "- Entwicklungsaccuracy der gewählten Parameter: "
        << memory.developmentAccuracy << "\n"
        << "- gewähltes Tau / Gain / Inkrement: "
        << memory.selectedTauMs << " / "
        << memory.selectedGain << " / "
        << memory.selectedIncrement << "\n"
        << "- Accuracy mit lokaler Eligibility: " << memory.traceAccuracy << "\n"
        << "- Accuracy ohne lokale Eligibility: " << memory.noTraceAccuracy << "\n"
        << "- Vorteil: " << memory.advantage << "\n"
        << "- mittlere absolute Eligibility beim Recall: "
        << memory.meanAbsoluteTraceAtRecall << "\n"
        << "- striktes Trace-essential-Kriterium erfüllt: "
        << (memory.traceEssential ? "ja" : "nein") << "\n\n"
        << "Das Readout erhielt nur Spikeänderungen und Membranzustände im "
           "neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, "
           "Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.\n\n"
        << "## Funktionsverlust und Wiederherstellung\n\n"
        << "- Funktion vor Schaden: " << recovery.baselineFunction << "\n"
        << "- unmittelbar nach Schaden: " << recovery.immediateFunction << "\n"
        << "- nach Erholungslernen: " << recovery.recoveredFunction << "\n"
        << "- gemessener Funktionsverlust: " << recovery.lostFunction << "\n"
        << "- wiedergewonnener Anteil: " << recovery.regainedFraction << "\n"
        << "- Jaccard der 100 meistgenutzten Pfade: " << recovery.pathwayJaccard << "\n"
        << "- deaktivierte Neuronen / Synapsen: " << recovery.disabledNeurons
        << " / " << recovery.disabledSynapses << "\n"
        << "- neue aktive / tatsächlich genutzte Synapsen: "
        << recovery.newActiveSynapses << " / " << recovery.newUsedSynapses << "\n"
        << "- Ersatzsynapsen mit Eltern-Provenienz: "
        << recovery.inheritedRepairSynapses << "\n"
        << "- Funktionswiederherstellung nach Kriterium: "
        << (recovery.functionRestored ? "ja" : "nein") << "\n"
        << "- gleicher Endpunkt über neuen Reparaturpfad: "
        << (recovery.sameMappingRepair ? "ja" : "nein") << "\n"
        << "- alternative Lösung nach Kriterium: "
        << (recovery.alternativeSolution ? "ja" : "nein") << "\n\n"
        << "Alle Werte sind experimentelle Messungen dieses synthetischen Systems. "
           "Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine "
           "Überlegenheits- oder Biologiebehauptung.\n";

    std::ofstream json(output / "stage17_metrics.json", std::ios::binary | std::ios::trunc);
    json << std::fixed << std::setprecision(9)
        << "{\n"
        << "  \"assembly_within_similarity\": " << assembly.withinSimilarity << ",\n"
        << "  \"assembly_between_similarity\": " << assembly.betweenSimilarity << ",\n"
        << "  \"similar_cue_reactivation\": " << assembly.similarCueReactivation << ",\n"
        << "  \"cross_modal_similarity\": " << assembly.crossModalSimilarity << ",\n"
        << "  \"post_damage_similarity\": " << assembly.postDamageSimilarity << ",\n"
        << "  \"snapshot_exact\": " << (assembly.snapshotExact ? "true" : "false") << ",\n"
        << "  \"transition_accuracy\": " << sequence.transitionAccuracy << ",\n"
        << "  \"boundary_response_ratio\": " << sequence.boundaryResponseRatio << ",\n"
        << "  \"repetition_stability\": " << sequence.repetitionStability << ",\n"
        << "  \"trace_accuracy\": " << memory.traceAccuracy << ",\n"
        << "  \"trace_development_accuracy\": "
        << memory.developmentAccuracy << ",\n"
        << "  \"selected_trace_tau_ms\": " << memory.selectedTauMs << ",\n"
        << "  \"selected_trace_gain\": " << memory.selectedGain << ",\n"
        << "  \"selected_trace_increment\": "
        << memory.selectedIncrement << ",\n"
        << "  \"no_trace_accuracy\": " << memory.noTraceAccuracy << ",\n"
        << "  \"mean_absolute_trace_at_recall\": "
        << memory.meanAbsoluteTraceAtRecall << ",\n"
        << "  \"trace_essential\": " << (memory.traceEssential ? "true" : "false") << ",\n"
        << "  \"function_before\": " << recovery.baselineFunction << ",\n"
        << "  \"function_immediate\": " << recovery.immediateFunction << ",\n"
        << "  \"function_recovered\": " << recovery.recoveredFunction << ",\n"
        << "  \"pathway_jaccard\": " << recovery.pathwayJaccard << ",\n"
        << "  \"inherited_repair_synapses\": "
        << recovery.inheritedRepairSynapses << ",\n"
        << "  \"function_restored\": "
        << (recovery.functionRestored ? "true" : "false") << ",\n"
        << "  \"same_mapping_repair\": "
        << (recovery.sameMappingRepair ? "true" : "false") << ",\n"
        << "  \"alternative_solution\": "
        << (recovery.alternativeSolution ? "true" : "false") << "\n"
        << "}\n";
}

bool confirmationExperiment(const std::filesystem::path& output) {
    constexpr int seedCount = 8;
    std::vector<AssemblyResult> assemblies;
    std::vector<SequenceResult> sequences;
    std::vector<RecoveryResult> recoveries;
    assemblies.reserve(seedCount);
    sequences.reserve(seedCount);
    recoveries.reserve(seedCount);
    int assemblyPasses = 0;
    int sequencePasses = 0;
    int recoveryPasses = 0;
    std::ofstream raw(
        output / "confirmation_seeds.csv",
        std::ios::binary | std::ios::trunc);
    raw << "seed,assemblies,within,between,reactivation,cross_modal,"
           "post_damage,assembly_pass,transition_accuracy,boundary_ratio,"
           "sequence_stability,sequence_pass,function_before,"
           "function_damaged,function_recovered,regained_fraction,"
           "inherited_repairs,recovery_pass\n"
        << std::fixed << std::setprecision(9);
    for (int index = 0; index < seedCount; ++index) {
        const std::uint64_t seed =
            25001U + static_cast<std::uint64_t>(index * 101);
        std::cout << "confirmation seed " << (index + 1)
            << "/" << seedCount << " assembly\n";
        const auto assembly = assemblyExperiment(output, seed);
        std::cout << "confirmation seed " << (index + 1)
            << "/" << seedCount << " sequence\n";
        const auto sequence = sequenceExperiment(seed + 20);
        std::cout << "confirmation seed " << (index + 1)
            << "/" << seedCount << " recovery\n";
        const auto recovery = recoveryExperiment(output, seed + 40);
        const bool assemblyPass =
            assembly.assembliesBefore >= 2
            && assembly.withinSimilarity
                > assembly.betweenSimilarity + 0.20
            && assembly.similarCueReactivation > 0.60
            && assembly.postDamageSimilarity > 0.50
            && assembly.snapshotExact;
        const bool sequencePass =
            sequence.transitionAccuracy > 0.50
            && sequence.boundaryResponseRatio > 1.30
            && sequence.repetitionStability > 0.70;
        const bool recoveryPass =
            recovery.functionRestored
            && recovery.sameMappingRepair;
        assemblyPasses += assemblyPass ? 1 : 0;
        sequencePasses += sequencePass ? 1 : 0;
        recoveryPasses += recoveryPass ? 1 : 0;
        raw << seed << "," << assembly.assembliesBefore << ","
            << assembly.withinSimilarity << ","
            << assembly.betweenSimilarity << ","
            << assembly.similarCueReactivation << ","
            << assembly.crossModalSimilarity << ","
            << assembly.postDamageSimilarity << ","
            << (assemblyPass ? 1 : 0) << ","
            << sequence.transitionAccuracy << ","
            << sequence.boundaryResponseRatio << ","
            << sequence.repetitionStability << ","
            << (sequencePass ? 1 : 0) << ","
            << recovery.baselineFunction << ","
            << recovery.immediateFunction << ","
            << recovery.recoveredFunction << ","
            << recovery.regainedFraction << ","
            << recovery.inheritedRepairSynapses << ","
            << (recoveryPass ? 1 : 0) << "\n";
        assemblies.push_back(assembly);
        sequences.push_back(sequence);
        recoveries.push_back(recovery);
    }
    std::cout << "confirmation trace-essential memory\n";
    const auto memory = memoryExperiment();
    const auto average = [](const auto& values, auto selector) {
        double sum = 0.0;
        for (const auto& value : values) sum += selector(value);
        return values.empty()
            ? 0.0
            : sum / static_cast<double>(values.size());
    };
    const bool assemblyConfirmed = assemblyPasses >= 6;
    const bool sequenceConfirmed = sequencePasses >= 6;
    const bool recoveryConfirmed = recoveryPasses >= 6;
    const bool overall =
        assemblyConfirmed
        && sequenceConfirmed
        && memory.traceEssential
        && recoveryConfirmed;
    std::ofstream report(
        output / "STAGE18_CONFIRMATION.md",
        std::ios::binary | std::ios::trunc);
    report << "# Forschungsstufe 18 – unabhängige Endzielbestätigung\n\n"
        << "Status: `" << (overall ? "confirmed" : "not_confirmed") << "`\n\n"
        << std::fixed << std::setprecision(6)
        << "Die Kriterien wurden vor dem Lauf eingefroren. Acht neue "
           "Netzwerkseeds wurden weder für Mechanismuswahl noch "
           "Parameteroptimierung verwendet.\n\n"
        << "Die Reparaturprüfung fordert einen Baseline-Effekt >=0,03, "
           "einen messbaren Verlust >0,004, gleiches Funktionsvorzeichen, "
           "mindestens 70 % Wiedergewinn und mindestens eine neue Synapse "
           "mit Eltern-Provenienz zum zerstörten Pfad.\n\n"
        << "| Hypothese | bestandene Seeds | Kriterium | Status |\n"
        << "|---|---:|---:|---|\n"
        << "| stabile konkurrierende Repräsentationen | "
        << assemblyPasses << "/8 | >=6 | "
        << (assemblyConfirmed ? "bestätigt" : "nicht bestätigt") << " |\n"
        << "| tokenizerfreie Übergänge und Grenzen | "
        << sequencePasses << "/8 | >=6 | "
        << (sequenceConfirmed ? "bestätigt" : "nicht bestätigt") << " |\n"
        << "| Trace-essential Recall-XOR | 12 Holdout-Netze | "
           "Accuracy>=0.70, Kontrolle<=0.60, Vorteil>=0.15 | "
        << (memory.traceEssential ? "bestätigt" : "nicht bestätigt") << " |\n"
        << "| Funktionsreparatur mit Pfadprovenienz | "
        << recoveryPasses << "/8 | >=6 | "
        << (recoveryConfirmed ? "bestätigt" : "nicht bestätigt") << " |\n\n"
        << "## Aggregierte Werte\n\n"
        << "- Assemblies pro Netz: "
        << average(assemblies, [](const auto& v) {
            return static_cast<double>(v.assembliesBefore);
        }) << "\n"
        << "- Reaktivierung ähnlicher Reize: "
        << average(assemblies, [](const auto& v) {
            return v.similarCueReactivation;
        }) << "\n"
        << "- Repräsentation nach Schaden: "
        << average(assemblies, [](const auto& v) {
            return v.postDamageSimilarity;
        }) << "\n"
        << "- Übergangsaccuracy: "
        << average(sequences, [](const auto& v) {
            return v.transitionAccuracy;
        }) << "\n"
        << "- Grenzreaktionsfaktor: "
        << average(sequences, [](const auto& v) {
            return v.boundaryResponseRatio;
        }) << "\n"
        << "- Trace-Accuracy / Kontrolle: "
        << memory.traceAccuracy << " / " << memory.noTraceAccuracy << "\n"
        << "- mittlerer wiedergewonnener Funktionsanteil: "
        << average(recoveries, [](const auto& v) {
            return v.regainedFraction;
        }) << "\n"
        << "- mittlere Ersatzsynapsen mit Eltern-Provenienz: "
        << average(recoveries, [](const auto& v) {
            return static_cast<double>(v.inheritedRepairSynapses);
        }) << "\n\n"
        << "Die Bestätigung gilt für die definierten synthetischen Aufgaben "
           "und ist keine Behauptung biologischer Gleichwertigkeit oder "
           "allgemeiner Intelligenz.\n";
    std::ofstream json(
        output / "stage18_confirmation.json",
        std::ios::binary | std::ios::trunc);
    json << "{\n"
        << "  \"status\": \"" << (overall ? "confirmed" : "not_confirmed") << "\",\n"
        << "  \"assembly_passes\": " << assemblyPasses << ",\n"
        << "  \"sequence_passes\": " << sequencePasses << ",\n"
        << "  \"trace_accuracy\": " << memory.traceAccuracy << ",\n"
        << "  \"trace_control_accuracy\": " << memory.noTraceAccuracy << ",\n"
        << "  \"trace_essential\": "
        << (memory.traceEssential ? "true" : "false") << ",\n"
        << "  \"recovery_passes\": " << recoveryPasses << ",\n"
        << "  \"seed_count\": " << seedCount << "\n"
        << "}\n";
    return overall;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path output =
            argc > 1 ? argv[1] : "stage17_results";
        std::filesystem::create_directories(output);
        if (argc > 2 && std::string(argv[2]) == "--confirm") {
            const bool confirmed = confirmationExperiment(output);
            std::cout << "confirmation="
                << (output / "STAGE18_CONFIRMATION.md").string() << '\n';
            return confirmed ? 0 : 3;
        }
        if (argc > 2 && std::string(argv[2]) == "--recovery-only") {
            std::cout << "functional recovery experiment\n";
            const std::uint64_t seed = argc > 3
                ? std::stoull(argv[3])
                : 17301U;
            const auto recovery = recoveryExperiment(output, seed);
            writeReport(
                output,
                AssemblyResult{},
                SequenceResult{},
                MemoryResult{},
                recovery);
            std::cout << "report="
                << (output / "STAGE17_REPORT.md").string() << '\n';
            return 0;
        }
        std::cout << "assembly experiment\n";
        const auto assembly = assemblyExperiment(output);
        std::cout << "token-free sequence experiment\n";
        const auto sequence = sequenceExperiment();
        std::cout << "trace-essential memory experiment\n";
        const auto memory = memoryExperiment();
        std::cout << "functional recovery experiment\n";
        const auto recovery = recoveryExperiment(output);
        writeReport(output, assembly, sequence, memory, recovery);
        std::cout << "report=" << (output / "STAGE17_REPORT.md").string() << '\n';
        return assembly.snapshotExact ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "stage17 failed: " << error.what() << '\n';
        return 1;
    }
}
