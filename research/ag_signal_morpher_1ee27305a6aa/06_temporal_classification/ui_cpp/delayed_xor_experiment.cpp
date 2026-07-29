#include "bio_core.hpp"

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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kModeCount = 6;
constexpr std::size_t kControlCount = 5;
constexpr int kRepetitionsPerCombination = 12;
constexpr int kFolds = 4;
constexpr int kTimeBins = 8;
constexpr std::size_t kPermutationCount = 1'000'000;
constexpr std::size_t kBootstrapCount = 200'000;
constexpr double kNoninferiorityMargin = -0.03;

const std::array<agbnn::GateMode, kModeCount> kModes{
    agbnn::GateMode::Kernel,
    agbnn::GateMode::Constant,
    agbnn::GateMode::Disabled,
    agbnn::GateMode::Sign,
    agbnn::GateMode::Tanh,
    agbnn::GateMode::Random};

const std::array<std::uint64_t, 24> kSeeds{
    1061, 1103, 1151, 1201, 1249, 1301, 1361, 1423,
    1481, 1543, 1601, 1663, 1721, 1783, 1847, 1901,
    1973, 2039, 2111, 2179, 2251, 2333, 2411, 2503};

struct DelayCondition {
    const char* name;
    int firstStart;
    int firstEnd;
    int secondStart;
    int secondEnd;
};

struct MemoryFeatureOptions {
    std::array<double, 3> eligibilityTausMs{50.0, 100.0, 200.0};
    bool eligibilityEnabled = true;
    bool interactionProductsEnabled = true;
    bool dendriteEnabled = true;
    bool localSynapticEligibilityEnabled = false;
    double localSynapticEligibilityTauMs = 100.0;
    double localSynapticEligibilityGain = 0.35;
    double localSynapticEligibilityMaximum = 4.0;
    double localSynapticEligibilityTimeShiftMs = 40.0;

    void validate() const {
        for (double tau : eligibilityTausMs) {
            if (!std::isfinite(tau) || tau <= 0.0) {
                throw std::invalid_argument(
                    "Eligibility-Zeitkonstanten müssen positiv sein");
            }
        }
        if (!std::isfinite(localSynapticEligibilityTauMs)
            || localSynapticEligibilityTauMs <= 0.0
            || !std::isfinite(localSynapticEligibilityGain)
            || localSynapticEligibilityGain < 0.0
            || localSynapticEligibilityGain > 1.0
            || !std::isfinite(localSynapticEligibilityMaximum)
            || localSynapticEligibilityMaximum <= 0.0
            || !std::isfinite(localSynapticEligibilityTimeShiftMs)
            || localSynapticEligibilityTimeShiftMs <= 0.0
            || localSynapticEligibilityTimeShiftMs > 1000.0) {
            throw std::invalid_argument(
                "Lokale Synapsen-Eligibility benötigt tau>0, "
                "Gain in [0,1], Maximum>0 und Shift in (0,1000]");
        }
    }
};

const std::array<DelayCondition, 2> kConditions{{
    {"medium_delay", 20, 40, 80, 100},
    {"long_delay", 10, 30, 110, 130},
}};

struct Evaluation {
    double accuracy = 0.0;
    double balancedAccuracy = 0.0;
    double spikeCost = 0.0;
    double firingRate = 0.0;
    double effectiveGate = 0.0;
    double effectiveGateVariance = 0.0;
    int totalSpikes = 0;
};

struct Dataset {
    std::vector<std::vector<double>> features;
    std::vector<int> labels;
    std::vector<int> foldIndices;
    double gateSum = 0.0;
    double effectiveGateWeightedSum = 0.0;
    double effectiveGateSquareWeightedSum = 0.0;
    double effectiveGateCount = 0.0;
    double firingRateSum = 0.0;
    double spikeSum = 0.0;
};

struct SeedAggregate {
    std::array<double, kModeCount> accuracy{};
    std::array<double, kModeCount> spikeCost{};
    std::array<double, kModeCount> firingRate{};
};

struct Comparison {
    agbnn::GateMode control = agbnn::GateMode::Disabled;
    double accuracyDifference = 0.0;
    double accuracyLowerBound = 0.0;
    double accuracyP = 1.0;
    double accuracyHolmP = 1.0;
    double costBenefit = 0.0;
    double costP = 1.0;
    double costHolmP = 1.0;
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

double mean(const std::vector<double>& values) {
    require(!values.empty(), "mean requires values");
    return std::accumulate(values.begin(), values.end(), 0.0)
        / static_cast<double>(values.size());
}

const char* modeName(agbnn::GateMode mode) {
    switch (mode) {
        case agbnn::GateMode::Kernel: return "kernel";
        case agbnn::GateMode::Constant: return "constant";
        case agbnn::GateMode::Disabled: return "disabled";
        case agbnn::GateMode::Sign: return "sign";
        case agbnn::GateMode::Tanh: return "tanh";
        case agbnn::GateMode::Random: return "random";
    }
    return "unknown";
}

agbnn::NetworkConfig networkConfig(std::uint64_t seed) {
    agbnn::NetworkConfig config;
    config.neuronCount = 24;
    config.connectionProbability = 0.15;
    config.seed = seed;
    config.gateTiming = agbnn::GateTiming::EmissionState;
    config.emissionFeature =
        agbnn::EmissionFeature::FeatureProjection;
    config.minimumAxonDelayMs = 1.0;
    config.maximumAxonDelayMs = 5.0;
    config.synapseModel =
        agbnn::SynapseModel::ConductanceBased;
    config.dendriteEnabled = true;
    config.externalToDendriteFraction = 0.0;
    config.classSpecificOperatorsEnabled = true;
    config.plasticityEnabled = true;
    config.validate();
    return config;
}

std::vector<std::vector<double>> makeDelayedXorStimulus(
    const DelayCondition& condition,
    int firstBit,
    int secondBit,
    int repetition,
    std::uint64_t seed) {
    constexpr int steps = 160;
    constexpr int neurons = 24;
    constexpr double baseline = 15.0;
    constexpr double pulse = 2.5;
    constexpr double noiseStd = 5.0;
    std::uint64_t mixedSeed = seed ^ 0xD31A9ED0ULL;
    mixedSeed ^= static_cast<std::uint64_t>(firstBit + 1)
        * 0x9E3779B97F4A7C15ULL;
    mixedSeed ^= static_cast<std::uint64_t>(secondBit + 3)
        * 0xBF58476D1CE4E5B9ULL;
    mixedSeed ^= static_cast<std::uint64_t>(repetition + 7)
        * 0x94D049BB133111EBULL;
    mixedSeed ^= static_cast<std::uint64_t>(condition.firstStart)
        * 0xA24BAED4963EE407ULL;
    std::mt19937_64 random(mixedSeed);
    std::normal_distribution<double> noise(0.0, noiseStd);
    std::vector<std::vector<double>> stimulus(
        steps,
        std::vector<double>(neurons, baseline));
    const int split = neurons / 2;
    for (int step = 0; step < steps; ++step) {
        for (int neuron = 0; neuron < neurons; ++neuron) {
            double current = baseline + noise(random);
            const int assembly = neuron < split ? 0 : 1;
            if (
                step >= condition.firstStart
                && step < condition.firstEnd
                && assembly == firstBit) {
                current += pulse;
            }
            if (
                step >= condition.secondStart
                && step < condition.secondEnd
                && assembly == secondBit) {
                current += pulse;
            }
            stimulus[static_cast<std::size_t>(step)]
                    [static_cast<std::size_t>(neuron)] = current;
        }
    }
    return stimulus;
}

std::vector<double> extractFeatures(
    const agbnn::SimulationResult& result,
    const agbnn::NetworkConfig& config) {
    const int neurons = config.neuronCount;
    const int split = neurons / 2;
    const std::array<int, 2> assemblySizes{
        split, neurons - split};
    std::vector<double> features(
        static_cast<std::size_t>(kTimeBins * 4),
        0.0);
    std::array<std::array<int, 2>, kTimeBins> voltageCounts{};
    for (std::size_t step = 0; step < result.spikes.size(); ++step) {
        const int bin = std::min(
            kTimeBins - 1,
            static_cast<int>(
                step * kTimeBins / result.spikes.size()));
        for (int neuron = 0; neuron < neurons; ++neuron) {
            const int assembly = neuron < split ? 0 : 1;
            const std::size_t spikeIndex =
                static_cast<std::size_t>(bin * 4 + assembly);
            const std::size_t voltageIndex =
                static_cast<std::size_t>(bin * 4 + 2 + assembly);
            features[spikeIndex] +=
                result.spikes[step][static_cast<std::size_t>(neuron)];
            features[voltageIndex] +=
                result.voltagesMv[step][static_cast<std::size_t>(neuron)];
            ++voltageCounts[static_cast<std::size_t>(bin)]
                [static_cast<std::size_t>(assembly)];
        }
    }
    const double binSeconds =
        config.dtMs * static_cast<double>(result.spikes.size())
        / kTimeBins / 1000.0;
    for (int bin = 0; bin < kTimeBins; ++bin) {
        for (int assembly = 0; assembly < 2; ++assembly) {
            features[static_cast<std::size_t>(bin * 4 + assembly)]
                /= binSeconds
                    * assemblySizes[static_cast<std::size_t>(assembly)];
            features[
                static_cast<std::size_t>(bin * 4 + 2 + assembly)]
                /= static_cast<double>(
                    voltageCounts[static_cast<std::size_t>(bin)]
                        [static_cast<std::size_t>(assembly)]);
        }
    }
    return features;
}

void standardize(
    std::vector<std::vector<double>>& train,
    std::vector<std::vector<double>>& test) {
    const std::size_t dimensions = train.front().size();
    std::vector<double> means(dimensions, 0.0);
    std::vector<double> deviations(dimensions, 1.0);
    for (std::size_t column = 0; column < dimensions; ++column) {
        for (const auto& row : train) {
            means[column] += row[column];
        }
        means[column] /= static_cast<double>(train.size());
        double variance = 0.0;
        for (const auto& row : train) {
            const double centered = row[column] - means[column];
            variance += centered * centered;
        }
        variance /= static_cast<double>(train.size());
        if (variance > 1e-12) {
            deviations[column] = std::sqrt(variance);
        }
    }
    for (auto* rows : {&train, &test}) {
        for (auto& row : *rows) {
            for (std::size_t column = 0; column < dimensions; ++column) {
                row[column] =
                    (row[column] - means[column]) / deviations[column];
            }
        }
    }
}

std::vector<double> trainReadout(
    const std::vector<std::vector<double>>& features,
    const std::vector<int>& labels) {
    constexpr int epochs = 300;
    constexpr double learningRate = 0.12;
    constexpr double l2 = 0.003;
    const std::size_t dimensions = features.front().size();
    std::vector<double> weights(dimensions + 1, 0.0);
    for (int epoch = 0; epoch < epochs; ++epoch) {
        std::vector<double> gradient(weights.size(), 0.0);
        for (std::size_t row = 0; row < features.size(); ++row) {
            double score = weights.back();
            for (std::size_t column = 0; column < dimensions; ++column) {
                score += weights[column] * features[row][column];
            }
            score = std::clamp(score, -30.0, 30.0);
            const double probability =
                1.0 / (1.0 + std::exp(-score));
            const double error = probability - labels[row];
            for (std::size_t column = 0; column < dimensions; ++column) {
                gradient[column] += error * features[row][column];
            }
            gradient.back() += error;
        }
        for (std::size_t column = 0; column < dimensions; ++column) {
            gradient[column] =
                gradient[column] / static_cast<double>(features.size())
                + l2 * weights[column];
        }
        gradient.back() /= static_cast<double>(features.size());
        for (std::size_t index = 0; index < weights.size(); ++index) {
            weights[index] -= learningRate * gradient[index];
        }
    }
    return weights;
}

Dataset simulateDataset(
    agbnn::NetworkConfig config,
    const DelayCondition& condition,
    agbnn::GateMode mode,
    bool collectEffectiveGates,
    std::vector<double>* effectiveGates) {
    config.gateMode = mode;
    if (config.classSpecificOperatorsEnabled) {
        config.eeGateMode = mode;
        config.eiGateMode = mode;
        config.ieGateMode = mode;
        config.iiGateMode = mode;
    }
    Dataset dataset;
    dataset.features.reserve(48);
    dataset.labels.reserve(48);
    dataset.foldIndices.reserve(48);
    for (int firstBit = 0; firstBit <= 1; ++firstBit) {
        for (int secondBit = 0; secondBit <= 1; ++secondBit) {
            for (
                int repetition = 0;
                repetition < kRepetitionsPerCombination;
                ++repetition) {
                agbnn::SpikingNetwork network(config);
                const auto result = network.run(makeDelayedXorStimulus(
                    condition,
                    firstBit,
                    secondBit,
                    repetition,
                    config.seed));
                require(result.metrics.finite, "non-finite XOR run");
                dataset.features.push_back(
                    extractFeatures(result, config));
                dataset.labels.push_back(firstBit ^ secondBit);
                dataset.foldIndices.push_back(repetition % kFolds);
                dataset.gateSum += result.metrics.meanGate;
                const double count =
                    result.metrics.effectiveGateCount;
                dataset.effectiveGateCount += count;
                dataset.effectiveGateWeightedSum +=
                    result.metrics.effectiveGateMean * count;
                dataset.effectiveGateSquareWeightedSum +=
                    (
                        result.metrics.effectiveGateVariance
                        + result.metrics.effectiveGateMean
                            * result.metrics.effectiveGateMean)
                    * count;
                dataset.firingRateSum +=
                    result.metrics.meanFiringRateHz;
                dataset.spikeSum += result.metrics.totalSpikes;
                if (collectEffectiveGates && effectiveGates) {
                    effectiveGates->insert(
                        effectiveGates->end(),
                        result.effectiveTransmissionGates.begin(),
                        result.effectiveTransmissionGates.end());
                }
            }
        }
    }
    return dataset;
}

Evaluation evaluateDataset(
    const Dataset& dataset,
    agbnn::GateMode mode) {
    Evaluation evaluation;
    evaluation.totalSpikes =
        static_cast<int>(std::lround(dataset.spikeSum));
    int totalCorrect = 0;
    int totalPredictions = 0;
    std::array<int, 2> allClassCorrect{0, 0};
    std::array<int, 2> allClassTotal{0, 0};
    for (int fold = 0; fold < kFolds; ++fold) {
        std::vector<std::vector<double>> trainFeatures;
        std::vector<std::vector<double>> testFeatures;
        std::vector<int> trainLabels;
        std::vector<int> testLabels;
        for (std::size_t row = 0; row < dataset.features.size(); ++row) {
            if (dataset.foldIndices[row] == fold) {
                testFeatures.push_back(dataset.features[row]);
                testLabels.push_back(dataset.labels[row]);
            } else {
                trainFeatures.push_back(dataset.features[row]);
                trainLabels.push_back(dataset.labels[row]);
            }
        }
        standardize(trainFeatures, testFeatures);
        const auto weights = trainReadout(trainFeatures, trainLabels);
        for (std::size_t row = 0; row < testFeatures.size(); ++row) {
            double score = weights.back();
            for (
                std::size_t column = 0;
                column < testFeatures[row].size();
                ++column) {
                score += weights[column] * testFeatures[row][column];
            }
            const int prediction = score >= 0.0 ? 1 : 0;
            const int truth = testLabels[row];
            ++totalPredictions;
            ++allClassTotal[static_cast<std::size_t>(truth)];
            if (prediction == truth) {
                ++totalCorrect;
                ++allClassCorrect[static_cast<std::size_t>(truth)];
            }
        }
    }
    evaluation.accuracy =
        totalCorrect / static_cast<double>(totalPredictions);
    evaluation.balancedAccuracy = 0.5 * (
        allClassCorrect[0] / static_cast<double>(allClassTotal[0])
        + allClassCorrect[1] / static_cast<double>(allClassTotal[1]));
    evaluation.spikeCost =
        totalCorrect > 0
            ? dataset.spikeSum / static_cast<double>(totalCorrect)
            : 0.0;
    evaluation.firingRate =
        dataset.firingRateSum
        / static_cast<double>(dataset.features.size());
    evaluation.effectiveGate =
        dataset.effectiveGateCount > 0.0
            ? dataset.effectiveGateWeightedSum
                / dataset.effectiveGateCount
            : 0.0;
    evaluation.effectiveGateVariance =
        dataset.effectiveGateCount > 0.0
            ? std::max(
                0.0,
                dataset.effectiveGateSquareWeightedSum
                    / dataset.effectiveGateCount
                    - evaluation.effectiveGate
                        * evaluation.effectiveGate)
            : 0.0;
    (void)mode;
    return evaluation;
}

std::array<Evaluation, kModeCount> compareAll(
    std::uint64_t seed,
    const DelayCondition& condition) {
    agbnn::NetworkConfig config = networkConfig(seed);
    std::array<Evaluation, kModeCount> evaluations{};
    evaluations[0] = evaluateDataset(
        simulateDataset(
            config,
            condition,
            agbnn::GateMode::Kernel,
            false,
            nullptr),
        agbnn::GateMode::Kernel);

    std::vector<double> effectiveGates;
    (void)simulateDataset(
        config,
        condition,
        agbnn::GateMode::Kernel,
        true,
        &effectiveGates);
    require(
        !effectiveGates.empty(),
        "XOR calibration has no effective gates");
    config.constantGate =
        std::accumulate(
            effectiveGates.begin(),
            effectiveGates.end(),
            0.0)
        / static_cast<double>(effectiveGates.size());
    config.randomGateValues = effectiveGates;
    for (std::size_t modeIndex = 1; modeIndex < kModeCount; ++modeIndex) {
        evaluations[modeIndex] = evaluateDataset(
            simulateDataset(
                config,
                condition,
                kModes[modeIndex],
                false,
                nullptr),
            kModes[modeIndex]);
    }
    return evaluations;
}

double signFlipPValue(
    const std::vector<double>& benefits,
    std::uint64_t seed) {
    const double observed = mean(benefits);
    std::mt19937_64 random(seed);
    std::uniform_int_distribution<int> sign(0, 1);
    std::size_t extreme = 0;
    for (
        std::size_t permutation = 0;
        permutation < kPermutationCount;
        ++permutation) {
        double sum = 0.0;
        for (double benefit : benefits) {
            sum += sign(random) == 0 ? -benefit : benefit;
        }
        if (
            sum / static_cast<double>(benefits.size())
            >= observed - 1e-15) {
            ++extreme;
        }
    }
    return static_cast<double>(extreme + 1)
        / static_cast<double>(kPermutationCount + 1);
}

double bootstrapLowerBound(
    const std::vector<double>& differences,
    std::uint64_t seed) {
    std::mt19937_64 random(seed);
    std::uniform_int_distribution<std::size_t> sample(
        0, differences.size() - 1);
    std::vector<double> means;
    means.reserve(kBootstrapCount);
    for (
        std::size_t replicate = 0;
        replicate < kBootstrapCount;
        ++replicate) {
        double sum = 0.0;
        for (std::size_t index = 0; index < differences.size(); ++index) {
            sum += differences[sample(random)];
        }
        means.push_back(
            sum / static_cast<double>(differences.size()));
    }
    const std::size_t index = static_cast<std::size_t>(
        std::floor(0.05 * static_cast<double>(kBootstrapCount - 1)));
    std::nth_element(
        means.begin(),
        means.begin() + static_cast<std::ptrdiff_t>(index),
        means.end());
    return means[index];
}

std::array<double, kControlCount> holmAdjust(
    const std::array<double, kControlCount>& pValues) {
    std::array<std::size_t, kControlCount> order{};
    std::iota(order.begin(), order.end(), 0);
    std::sort(
        order.begin(),
        order.end(),
        [&](std::size_t left, std::size_t right) {
            return pValues[left] < pValues[right];
        });
    std::array<double, kControlCount> adjusted{};
    double running = 0.0;
    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        const std::size_t index = order[rank];
        running = std::max(
            running,
            std::min(
                1.0,
                pValues[index]
                    * static_cast<double>(order.size() - rank)));
        adjusted[index] = running;
    }
    return adjusted;
}

std::string experimentDescriptor() {
    std::ostringstream out;
    out << "GO-SNN-DXOR-MS24-D2-v1;N=24;density=.15;steps=160;"
        "reps=12;folds=4;bins=8;lr=.12;epochs=300;l2=.003;";
    for (std::uint64_t seed : kSeeds) {
        out << seed << ",";
    }
    for (const auto& condition : kConditions) {
        out << condition.name << ":" << condition.firstStart << ":"
            << condition.firstEnd << ":" << condition.secondStart
            << ":" << condition.secondEnd << ";";
    }
    return out.str();
}

std::string experimentHash() {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : experimentDescriptor()) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << std::setw(16) << hash;
    return out.str();
}

using ObservationCube =
    std::array<std::array<std::array<Evaluation, kModeCount>, 2>, 24>;

void writeRawCsv(
    const std::filesystem::path& path,
    const ObservationCube& observations) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot create XOR raw CSV");
    output << "experiment_hash,seed,condition,mode,accuracy,"
        "balanced_accuracy,spikes_per_correct,firing_rate_hz,"
        "effective_gate,effective_gate_variance,total_spikes\n"
        << std::fixed << std::setprecision(12);
    for (std::size_t seed = 0; seed < kSeeds.size(); ++seed) {
        for (
            std::size_t condition = 0;
            condition < kConditions.size();
            ++condition) {
            for (std::size_t mode = 0; mode < kModeCount; ++mode) {
                const auto& item =
                    observations[seed][condition][mode];
                output << experimentHash() << "," << kSeeds[seed]
                    << "," << kConditions[condition].name
                    << "," << modeName(kModes[mode])
                    << "," << item.accuracy
                    << "," << item.balancedAccuracy
                    << "," << item.spikeCost
                    << "," << item.firingRate
                    << "," << item.effectiveGate
                    << "," << item.effectiveGateVariance
                    << "," << item.totalSpikes << "\n";
            }
        }
    }
    require(static_cast<bool>(output), "cannot write XOR raw CSV");
}

void writeSummary(
    const std::filesystem::path& path,
    const std::array<SeedAggregate, 24>& aggregates,
    const std::array<Comparison, kControlCount>& comparisons,
    bool scopedReplication,
    bool strongReplication,
    bool accuracySuperiorityAll) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot create XOR summary");
    output << std::boolalpha << std::fixed << std::setprecision(12)
        << "{\n"
        << "  \"experiment_id\": \"GO-SNN-DXOR-MS24-D2-v1\",\n"
        << "  \"experiment_hash\": \"" << experimentHash() << "\",\n"
        << "  \"task\": \"delayed_xor\",\n"
        << "  \"seed_count\": 24,\n"
        << "  \"delay_condition_count\": 2,\n"
        << "  \"raw_evaluation_count\": 288,\n"
        << "  \"noninferiority_margin\": "
        << kNoninferiorityMargin << ",\n"
        << "  \"claims\": {\n"
        << "    \"scoped_efficiency_replication\": "
        << scopedReplication << ",\n"
        << "    \"strong_all_control_efficiency_replication\": "
        << strongReplication << ",\n"
        << "    \"accuracy_superiority_all_controls\": "
        << accuracySuperiorityAll << "\n"
        << "  },\n"
        << "  \"mode_means\": [\n";
    for (std::size_t mode = 0; mode < kModeCount; ++mode) {
        std::vector<double> accuracies;
        std::vector<double> costs;
        std::vector<double> rates;
        for (const auto& seed : aggregates) {
            accuracies.push_back(seed.accuracy[mode]);
            costs.push_back(seed.spikeCost[mode]);
            rates.push_back(seed.firingRate[mode]);
        }
        output << "    {\"mode\": \"" << modeName(kModes[mode])
            << "\", \"accuracy\": " << mean(accuracies)
            << ", \"spikes_per_correct\": " << mean(costs)
            << ", \"firing_rate_hz\": " << mean(rates) << "}"
            << (mode + 1 == kModeCount ? "\n" : ",\n");
    }
    output << "  ],\n"
        << "  \"kernel_comparisons\": [\n";
    for (std::size_t index = 0; index < comparisons.size(); ++index) {
        const auto& item = comparisons[index];
        output << "    {\"control\": \"" << modeName(item.control)
            << "\", \"accuracy_difference\": "
            << item.accuracyDifference
            << ", \"accuracy_lower_95_one_sided\": "
            << item.accuracyLowerBound
            << ", \"accuracy_p_holm\": " << item.accuracyHolmP
            << ", \"spike_cost_benefit\": " << item.costBenefit
            << ", \"cost_p_holm\": " << item.costHolmP << "}"
            << (index + 1 == comparisons.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
        << "  \"decision\": \""
        << (
            strongReplication
                ? "STRONG_ALL_CONTROL_EFFICIENCY_REPLICATED"
                : (
                    scopedReplication
                        ? "SCOPED_EFFICIENCY_REPLICATED"
                        : "NO_EFFICIENCY_REPLICATION"))
        << "\"\n"
        << "}\n";
    require(static_cast<bool>(output), "cannot write XOR summary");
}

void writeReport(
    const std::filesystem::path& path,
    const std::array<SeedAggregate, 24>& aggregates,
    const std::array<Comparison, kControlCount>& comparisons,
    bool scopedReplication,
    bool strongReplication,
    bool accuracySuperiorityAll) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot create XOR report");
    output << "# Delayed-XOR-Replikation\n\n"
        << "Experiment-Hash: `" << experimentHash() << "`\n\n"
        << "24 neue Seeds, zwei Verzögerungsbedingungen und sechs "
        "Gatevarianten ergeben 288 vollständige Auswertungen.\n\n"
        << "## Modusmittel\n\n"
        << "| Modus | Accuracy | Spikes/korrekt | Rate Hz |\n"
        << "|---|---:|---:|---:|\n"
        << std::fixed << std::setprecision(6);
    for (std::size_t mode = 0; mode < kModeCount; ++mode) {
        std::vector<double> accuracies;
        std::vector<double> costs;
        std::vector<double> rates;
        for (const auto& seed : aggregates) {
            accuracies.push_back(seed.accuracy[mode]);
            costs.push_back(seed.spikeCost[mode]);
            rates.push_back(seed.firingRate[mode]);
        }
        output << "| " << modeName(kModes[mode])
            << " | " << mean(accuracies)
            << " | " << mean(costs)
            << " | " << mean(rates) << " |\n";
    }
    output << "\n## Kernel gegen Kontrollen\n\n"
        << "| Kontrolle | ΔAccuracy | untere 95-%-Grenze | "
        "p Holm Accuracy | Kostenvorteil | p Holm Kosten |\n"
        << "|---|---:|---:|---:|---:|---:|\n";
    for (const auto& item : comparisons) {
        output << "| " << modeName(item.control)
            << " | " << item.accuracyDifference
            << " | " << item.accuracyLowerBound
            << " | " << item.accuracyHolmP
            << " | " << item.costBenefit
            << " | " << item.costHolmP << " |\n";
    }
    output << "\n## Vorab festgelegte Entscheidungen\n\n"
        << "- Scoped-Effizienzreplikation: **"
        << (scopedReplication ? "BESTÄTIGT" : "NICHT BESTÄTIGT")
        << "**\n"
        << "- starke Replikation gegen alle Kontrollen einschließlich "
        "Vorzeichen: **"
        << (strongReplication ? "BESTÄTIGT" : "NICHT BESTÄTIGT")
        << "**\n"
        << "- Accuracy-Überlegenheit gegen alle Kontrollen: **"
        << (
            accuracySuperiorityAll
                ? "BESTÄTIGT"
                : "NICHT BESTÄTIGT")
        << "**\n\n"
        << "## Schlussfolgerung\n\n";
    if (strongReplication) {
        output << "Die Spikekosteneffizienz wurde auf Delayed XOR gegen "
            "alle Kontrollen einschließlich des Vorzeichengates repliziert."
            "\n";
    } else if (scopedReplication) {
        output << "Die frühere eng begrenzte Spikekosteneffizienz wurde "
            "auf Delayed XOR repliziert, aber nicht gegenüber allen "
            "Kontrollen einschließlich des Vorzeichengates.\n";
    } else {
        output << "`NO_EFFICIENCY_REPLICATION`: Die vorab festgelegten "
            "Kriterien wurden auf Delayed XOR nicht vollständig erfüllt.\n";
    }
    output << "\nDie Aufgabe ist synthetisch; das Resultat ist keine "
        "biologische Validierung.\n";
    require(static_cast<bool>(output), "cannot write XOR report");
}

std::vector<double> extractMemoryFeatures(
    const agbnn::SimulationResult& result,
    const agbnn::NetworkConfig& config,
    const DelayCondition& condition,
    const MemoryFeatureOptions& options) {
    const std::array<double, 4> timeConstants{20.0, 50.0, 100.0, 200.0};
    const std::array<int, 3> checkpoints{
        condition.secondEnd - 1,
        std::min(
            static_cast<int>(result.spikes.size()) - 1,
            condition.secondEnd + 19),
        static_cast<int>(result.spikes.size()) - 1};
    const int split = config.neuronCount / 2;
    const std::array<int, 2> assemblySizes{
        split, config.neuronCount - split};
    std::array<std::array<double, 4>, 2> traces{};
    std::array<std::array<double, 8>, 3> differences{};
    std::vector<double> features;
    features.reserve(195);
    for (std::size_t step = 0; step < result.spikes.size(); ++step) {
        std::array<double, 2> spikeCounts{0.0, 0.0};
        for (int neuron = 0; neuron < config.neuronCount; ++neuron) {
            const int assembly = neuron < split ? 0 : 1;
            spikeCounts[static_cast<std::size_t>(assembly)] +=
                result.spikes[step][static_cast<std::size_t>(neuron)];
        }
        for (int assembly = 0; assembly < 2; ++assembly) {
            for (std::size_t tau = 0; tau < timeConstants.size(); ++tau) {
                traces[static_cast<std::size_t>(assembly)][tau] =
                    traces[static_cast<std::size_t>(assembly)][tau]
                        * std::exp(-config.dtMs / timeConstants[tau])
                    + spikeCounts[static_cast<std::size_t>(assembly)]
                        / assemblySizes[
                            static_cast<std::size_t>(assembly)];
            }
        }
        for (
            std::size_t checkpointIndex = 0;
            checkpointIndex < checkpoints.size();
            ++checkpointIndex) {
            if (static_cast<int>(step) != checkpoints[checkpointIndex]) {
                continue;
            }
            std::array<std::array<double, 8>, 2> state{};
            for (int assembly = 0; assembly < 2; ++assembly) {
                for (std::size_t tau = 0; tau < 4; ++tau) {
                    state[static_cast<std::size_t>(assembly)][tau] =
                        traces[static_cast<std::size_t>(assembly)][tau];
                }
                for (int windowIndex = 0; windowIndex < 2; ++windowIndex) {
                    const int window = windowIndex == 0 ? 10 : 30;
                    double recent = 0.0;
                    const int firstStep = std::max(
                        0,
                        static_cast<int>(step) - window + 1);
                    for (
                        int recentStep = firstStep;
                        recentStep <= static_cast<int>(step);
                        ++recentStep) {
                        const int firstNeuron =
                            assembly == 0 ? 0 : split;
                        const int lastNeuron =
                            assembly == 0
                                ? split
                                : config.neuronCount;
                        for (
                            int neuron = firstNeuron;
                            neuron < lastNeuron;
                            ++neuron) {
                            recent += result.spikes[
                                static_cast<std::size_t>(recentStep)]
                                [static_cast<std::size_t>(neuron)];
                        }
                    }
                    state[static_cast<std::size_t>(assembly)]
                        [static_cast<std::size_t>(4 + windowIndex)] =
                            recent
                            / assemblySizes[
                                static_cast<std::size_t>(assembly)];
                }
                double soma = 0.0;
                double dendrite = 0.0;
                const int firstNeuron = assembly == 0 ? 0 : split;
                const int lastNeuron =
                    assembly == 0 ? split : config.neuronCount;
                for (int neuron = firstNeuron; neuron < lastNeuron; ++neuron) {
                    soma += result.voltagesMv[step]
                        [static_cast<std::size_t>(neuron)];
                    if (options.dendriteEnabled) {
                        dendrite += result.dendriticVoltagesMv[step]
                            [static_cast<std::size_t>(neuron)];
                    }
                }
                state[static_cast<std::size_t>(assembly)][6] =
                    soma
                    / assemblySizes[static_cast<std::size_t>(assembly)];
                state[static_cast<std::size_t>(assembly)][7] =
                    dendrite
                    / assemblySizes[static_cast<std::size_t>(assembly)];
            }
            for (int assembly = 0; assembly < 2; ++assembly) {
                for (double value :
                     state[static_cast<std::size_t>(assembly)]) {
                    features.push_back(value);
                }
            }
            for (std::size_t channel = 0; channel < 8; ++channel) {
                differences[checkpointIndex][channel] =
                    state[1][channel] - state[0][channel];
            }
        }
    }
    require(features.size() == 48, "memory state feature count changed");
    if (options.interactionProductsEnabled) {
        for (const auto& checkpoint : differences) {
            for (std::size_t left = 0; left < 8; ++left) {
                for (std::size_t right = left + 1; right < 8; ++right) {
                    features.push_back(
                        checkpoint[left] * checkpoint[right]);
                }
            }
        }
        for (std::size_t channel = 0; channel < 8; ++channel) {
            for (std::size_t left = 0; left < 3; ++left) {
                for (std::size_t right = left + 1; right < 3; ++right) {
                    features.push_back(
                        differences[left][channel]
                        * differences[right][channel]);
                }
            }
        }
    }
    require(
        features.size()
            == (options.interactionProductsEnabled ? 156 : 48),
        "memory interaction count changed");

    std::array<std::array<double, 3>, 2> cueDifferences{};
    const std::array<std::array<int, 2>, 2> cueWindows{{
        {condition.firstStart, condition.firstEnd},
        {condition.secondStart, condition.secondEnd},
    }};
    for (std::size_t cue = 0; cue < cueWindows.size(); ++cue) {
        std::array<std::array<double, 3>, 2> cueState{};
        const int windowLength =
            cueWindows[cue][1] - cueWindows[cue][0];
        for (
            int step = cueWindows[cue][0];
            step < cueWindows[cue][1];
            ++step) {
            for (int neuron = 0; neuron < config.neuronCount; ++neuron) {
                const int assembly = neuron < split ? 0 : 1;
                cueState[static_cast<std::size_t>(assembly)][0] +=
                    result.spikes[static_cast<std::size_t>(step)]
                        [static_cast<std::size_t>(neuron)];
                cueState[static_cast<std::size_t>(assembly)][1] +=
                    result.voltagesMv[static_cast<std::size_t>(step)]
                        [static_cast<std::size_t>(neuron)];
                if (options.dendriteEnabled) {
                    cueState[static_cast<std::size_t>(assembly)][2] +=
                        result.dendriticVoltagesMv[
                            static_cast<std::size_t>(step)]
                            [static_cast<std::size_t>(neuron)];
                }
            }
        }
        for (int assembly = 0; assembly < 2; ++assembly) {
            cueState[static_cast<std::size_t>(assembly)][0] /=
                assemblySizes[static_cast<std::size_t>(assembly)];
            cueState[static_cast<std::size_t>(assembly)][1] /=
                windowLength
                * assemblySizes[static_cast<std::size_t>(assembly)];
            cueState[static_cast<std::size_t>(assembly)][2] /=
                windowLength
                * assemblySizes[static_cast<std::size_t>(assembly)];
        }
        for (std::size_t channel = 0; channel < 3; ++channel) {
            cueDifferences[cue][channel] =
                cueState[1][channel] - cueState[0][channel];
        }
    }
    const double memoryDurationMs =
        (condition.secondStart - condition.firstEnd) * config.dtMs;
    std::array<double, 9> eligibility{};
    if (options.eligibilityEnabled) {
        std::size_t eligibilityIndex = 0;
        for (double tau : options.eligibilityTausMs) {
            const double decay = std::exp(-memoryDurationMs / tau);
            for (double cueValue : cueDifferences[0]) {
                eligibility[eligibilityIndex++] = cueValue * decay;
                features.push_back(cueValue * decay);
            }
        }
        for (double secondCueValue : cueDifferences[1]) {
            features.push_back(secondCueValue);
        }
        if (options.interactionProductsEnabled) {
            for (double memoryValue : eligibility) {
                for (double secondCueValue : cueDifferences[1]) {
                    features.push_back(memoryValue * secondCueValue);
                }
            }
        }
    }
    const std::size_t expected =
        (options.interactionProductsEnabled ? 156U : 48U)
        + (options.eligibilityEnabled ? 12U : 0U)
        + (
            options.eligibilityEnabled
                && options.interactionProductsEnabled
            ? 27U
            : 0U);
    require(features.size() == expected, "eligibility feature count changed");
    return features;
}

Dataset simulateMemoryDataset(
    agbnn::NetworkConfig config,
    const DelayCondition& condition,
    agbnn::GateMode mode,
    bool collectEffectiveGates,
    std::vector<double>* effectiveGates,
    const MemoryFeatureOptions& options = {}) {
    options.validate();
    config.dendriteEnabled = options.dendriteEnabled;
    config.localEligibilityEnabled =
        options.localSynapticEligibilityEnabled;
    config.localEligibilityTauMs =
        options.localSynapticEligibilityTauMs;
    config.localEligibilityGain =
        options.localSynapticEligibilityGain;
    config.localEligibilityMaximum =
        options.localSynapticEligibilityMaximum;
    config.localEligibilityTimeShiftMs =
        options.localSynapticEligibilityTimeShiftMs;
    config.gateMode = mode;
    config.eeGateMode = mode;
    config.eiGateMode = mode;
    config.ieGateMode = mode;
    config.iiGateMode = mode;
    Dataset dataset;
    dataset.features.reserve(48);
    dataset.labels.reserve(48);
    dataset.foldIndices.reserve(48);
    for (int firstBit = 0; firstBit <= 1; ++firstBit) {
        for (int secondBit = 0; secondBit <= 1; ++secondBit) {
            for (
                int repetition = 0;
                repetition < kRepetitionsPerCombination;
                ++repetition) {
                agbnn::SpikingNetwork network(config);
                const auto result = network.run(makeDelayedXorStimulus(
                    condition,
                    firstBit,
                    secondBit,
                    repetition,
                    config.seed));
                require(result.metrics.finite, "non-finite memory XOR run");
                dataset.features.push_back(
                    extractMemoryFeatures(
                        result, config, condition, options));
                dataset.labels.push_back(firstBit ^ secondBit);
                dataset.foldIndices.push_back(repetition % kFolds);
                dataset.gateSum += result.metrics.meanGate;
                const double count = result.metrics.effectiveGateCount;
                dataset.effectiveGateCount += count;
                dataset.effectiveGateWeightedSum +=
                    result.metrics.effectiveGateMean * count;
                dataset.effectiveGateSquareWeightedSum +=
                    (
                        result.metrics.effectiveGateVariance
                        + result.metrics.effectiveGateMean
                            * result.metrics.effectiveGateMean)
                    * count;
                dataset.firingRateSum +=
                    result.metrics.meanFiringRateHz;
                dataset.spikeSum += result.metrics.totalSpikes;
                if (collectEffectiveGates && effectiveGates) {
                    effectiveGates->insert(
                        effectiveGates->end(),
                        result.effectiveTransmissionGates.begin(),
                        result.effectiveTransmissionGates.end());
                }
            }
        }
    }
    return dataset;
}

std::string memoryExperimentHash() {
    std::string descriptor =
        "GO-SNN-DXOR-MEMORY-DEV-v2;features=195;"
        "taus=20,50,100,200;eligibility=50,100,200;"
        "recent=10,30;checkpoints=end2,+20,final;";
    for (std::uint64_t seed : kSeeds) {
        descriptor += std::to_string(seed) + ",";
    }
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : descriptor) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << std::setw(16) << hash;
    return out.str();
}

int runMemoryDevelopment(const std::filesystem::path& outputDirectory) {
    std::filesystem::create_directories(outputDirectory);
    std::array<std::array<Evaluation, 2>, 24> results{};
    std::cout << "Memory development hash="
        << memoryExperimentHash() << "\n";
    for (std::size_t seed = 0; seed < kSeeds.size(); ++seed) {
        for (
            std::size_t condition = 0;
            condition < kConditions.size();
            ++condition) {
            results[seed][condition] = evaluateDataset(
                simulateMemoryDataset(
                    networkConfig(kSeeds[seed]),
                    kConditions[condition],
                    agbnn::GateMode::Kernel,
                    false,
                    nullptr),
                agbnn::GateMode::Kernel);
        }
        std::cout << "development seed " << (seed + 1) << "/"
            << kSeeds.size() << " complete" << std::endl;
    }
    std::array<double, 2> conditionAccuracy{};
    double overallAccuracy = 0.0;
    std::ofstream csv(
        outputDirectory / "development_results.csv",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(csv), "cannot create development CSV");
    csv << "development_hash,seed,condition,accuracy,"
        "spikes_per_correct,firing_rate_hz\n"
        << std::fixed << std::setprecision(12);
    for (std::size_t seed = 0; seed < kSeeds.size(); ++seed) {
        for (
            std::size_t condition = 0;
            condition < kConditions.size();
            ++condition) {
            const auto& item = results[seed][condition];
            conditionAccuracy[condition] +=
                item.accuracy / static_cast<double>(kSeeds.size());
            overallAccuracy +=
                item.accuracy
                / static_cast<double>(
                    kSeeds.size() * kConditions.size());
            csv << memoryExperimentHash() << "," << kSeeds[seed]
                << "," << kConditions[condition].name
                << "," << item.accuracy
                << "," << item.spikeCost
                << "," << item.firingRate << "\n";
        }
    }
    const bool freeze =
        overallAccuracy >= 0.75
        && conditionAccuracy[0] >= 0.65
        && conditionAccuracy[1] >= 0.65;
    std::ofstream summary(
        outputDirectory / "development_summary.json",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(summary), "cannot create development summary");
    summary << std::boolalpha << std::fixed << std::setprecision(12)
        << "{\n"
        << "  \"development_hash\": \"" << memoryExperimentHash()
        << "\",\n"
        << "  \"feature_count\": 195,\n"
        << "  \"overall_accuracy\": " << overallAccuracy << ",\n"
        << "  \"medium_delay_accuracy\": "
        << conditionAccuracy[0] << ",\n"
        << "  \"long_delay_accuracy\": "
        << conditionAccuracy[1] << ",\n"
        << "  \"freeze_for_holdout\": " << freeze << "\n"
        << "}\n";
    std::ofstream report(
        outputDirectory / "DEVELOPMENT_REPORT.md",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report), "cannot create development report");
    report << "# Delayed-XOR-Gedächtnisreadout – Entwicklung\n\n"
        << "Hash: `" << memoryExperimentHash() << "`\n\n"
        << std::fixed << std::setprecision(6)
        << "- Gesamtaccuracy: " << overallAccuracy << "\n"
        << "- mittlere Verzögerung: " << conditionAccuracy[0] << "\n"
        << "- lange Verzögerung: " << conditionAccuracy[1] << "\n"
        << "- Features: 195\n"
        << "- Holdout freigeben: **"
        << (freeze ? "JA" : "NEIN") << "**\n\n"
        << (
            freeze
                ? "Die vorab festgelegten Entwicklungskriterien sind "
                  "erfüllt. Diese Konfiguration darf ohne Änderung auf "
                  "den gesperrten Seeds bestätigt werden.\n"
                : "Die Entwicklungskriterien sind nicht erfüllt. Die "
                  "gesperrten Bestätigungs-Seeds bleiben unangetastet.\n");
    std::cout << std::fixed << std::setprecision(6)
        << "development_accuracy=" << overallAccuracy << "\n"
        << "medium_delay_accuracy=" << conditionAccuracy[0] << "\n"
        << "long_delay_accuracy=" << conditionAccuracy[1] << "\n"
        << "freeze_for_holdout=" << std::boolalpha << freeze << "\n";
    return 0;
}

const std::array<std::uint64_t, 16> kMemoryHoldoutSeeds{
    2609, 2671, 2741, 2803, 2879, 2953, 3023, 3109,
    3181, 3253, 3323, 3391, 3463, 3541, 3613, 3691};

std::array<Evaluation, kModeCount> compareAllMemory(
    std::uint64_t seed,
    const DelayCondition& condition) {
    agbnn::NetworkConfig config = networkConfig(seed);
    std::array<Evaluation, kModeCount> evaluations{};
    evaluations[0] = evaluateDataset(
        simulateMemoryDataset(
            config, condition, agbnn::GateMode::Kernel, false, nullptr),
        agbnn::GateMode::Kernel);
    std::vector<double> effectiveGates;
    (void)simulateMemoryDataset(
        config,
        condition,
        agbnn::GateMode::Kernel,
        true,
        &effectiveGates);
    require(!effectiveGates.empty(), "memory calibration has no gates");
    config.constantGate =
        std::accumulate(
            effectiveGates.begin(), effectiveGates.end(), 0.0)
        / static_cast<double>(effectiveGates.size());
    config.randomGateValues = effectiveGates;
    for (std::size_t mode = 1; mode < kModeCount; ++mode) {
        evaluations[mode] = evaluateDataset(
            simulateMemoryDataset(
                config, condition, kModes[mode], false, nullptr),
            kModes[mode]);
    }
    return evaluations;
}

std::string memoryConfirmationHash() {
    std::string descriptor = memoryExperimentHash() + ";holdout=";
    for (std::uint64_t seed : kMemoryHoldoutSeeds) {
        descriptor += std::to_string(seed) + ",";
    }
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : descriptor) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << std::setw(16) << hash;
    return out.str();
}

int runMemoryConfirmation(const std::filesystem::path& outputDirectory) {
    std::filesystem::create_directories(outputDirectory);
    using HoldoutCube = std::array<
        std::array<std::array<Evaluation, kModeCount>, 2>, 16>;
    HoldoutCube observations{};
    std::cout << "Frozen model=" << memoryExperimentHash()
        << " confirmation=" << memoryConfirmationHash() << "\n";
    for (std::size_t seed = 0; seed < kMemoryHoldoutSeeds.size(); ++seed) {
        for (
            std::size_t condition = 0;
            condition < kConditions.size();
            ++condition) {
            observations[seed][condition] = compareAllMemory(
                kMemoryHoldoutSeeds[seed], kConditions[condition]);
        }
        std::cout << "holdout seed " << (seed + 1) << "/"
            << kMemoryHoldoutSeeds.size() << " complete" << std::endl;
    }

    std::array<SeedAggregate, 16> aggregates{};
    std::array<double, 2> conditionAccuracy{};
    for (std::size_t seed = 0; seed < aggregates.size(); ++seed) {
        for (std::size_t mode = 0; mode < kModeCount; ++mode) {
            for (std::size_t condition = 0; condition < 2; ++condition) {
                const auto& item = observations[seed][condition][mode];
                aggregates[seed].accuracy[mode] += item.accuracy / 2.0;
                aggregates[seed].spikeCost[mode] += item.spikeCost / 2.0;
                aggregates[seed].firingRate[mode] += item.firingRate / 2.0;
                if (mode == 0) {
                    conditionAccuracy[condition] +=
                        item.accuracy / aggregates.size();
                }
            }
        }
    }
    std::vector<double> kernelAccuracies;
    for (const auto& seed : aggregates) {
        kernelAccuracies.push_back(seed.accuracy[0]);
    }
    const double kernelAccuracy = mean(kernelAccuracies);
    const double accuracyLower = bootstrapLowerBound(
        kernelAccuracies, 0xC0FFEE13ULL);
    const bool reliablyLearned =
        kernelAccuracy >= 0.75
        && accuracyLower >= 0.65
        && conditionAccuracy[0] >= 0.65
        && conditionAccuracy[1] >= 0.65;

    std::array<Comparison, kControlCount> comparisons{};
    std::array<double, kControlCount> costP{};
    for (std::size_t control = 0; control < kControlCount; ++control) {
        const std::size_t mode = control + 1;
        std::vector<double> accuracyDifferences;
        std::vector<double> costBenefits;
        for (const auto& seed : aggregates) {
            accuracyDifferences.push_back(
                seed.accuracy[0] - seed.accuracy[mode]);
            costBenefits.push_back(
                seed.spikeCost[mode] - seed.spikeCost[0]);
        }
        auto& item = comparisons[control];
        item.control = kModes[mode];
        item.accuracyDifference = mean(accuracyDifferences);
        item.accuracyLowerBound = bootstrapLowerBound(
            accuracyDifferences, 0xC0FFEE20ULL + control);
        item.costBenefit = mean(costBenefits);
        item.costP = signFlipPValue(
            costBenefits, 0xC0FFEE30ULL + control);
        costP[control] = item.costP;
    }
    const auto adjustedCost = holmAdjust(costP);
    for (std::size_t index = 0; index < comparisons.size(); ++index) {
        comparisons[index].costHolmP = adjustedCost[index];
    }
    const auto efficient = [&](std::size_t control) {
        const auto& item = comparisons[control];
        return reliablyLearned
            && item.accuracyLowerBound > kNoninferiorityMargin
            && item.costBenefit > 0.0
            && item.costHolmP < 0.05;
    };
    const bool scopedEfficiency =
        efficient(0) && efficient(1) && efficient(4);
    const bool allControlEfficiency =
        scopedEfficiency && efficient(2) && efficient(3);

    std::ofstream raw(
        outputDirectory / "holdout_raw_results.csv",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(raw), "cannot create memory holdout CSV");
    raw << "confirmation_hash,model_hash,seed,condition,mode,"
        "accuracy,spikes_per_correct,firing_rate_hz\n"
        << std::fixed << std::setprecision(12);
    for (std::size_t seed = 0; seed < observations.size(); ++seed) {
        for (std::size_t condition = 0; condition < 2; ++condition) {
            for (std::size_t mode = 0; mode < kModeCount; ++mode) {
                const auto& item = observations[seed][condition][mode];
                raw << memoryConfirmationHash() << ","
                    << memoryExperimentHash() << ","
                    << kMemoryHoldoutSeeds[seed] << ","
                    << kConditions[condition].name << ","
                    << modeName(kModes[mode]) << ","
                    << item.accuracy << "," << item.spikeCost << ","
                    << item.firingRate << "\n";
            }
        }
    }

    std::ofstream summary(
        outputDirectory / "holdout_summary.json",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(summary), "cannot create holdout summary");
    summary << std::boolalpha << std::fixed << std::setprecision(12)
        << "{\n"
        << "  \"model_hash\": \"" << memoryExperimentHash() << "\",\n"
        << "  \"confirmation_hash\": \"" << memoryConfirmationHash()
        << "\",\n"
        << "  \"kernel_accuracy\": " << kernelAccuracy << ",\n"
        << "  \"kernel_accuracy_lower_95_one_sided\": "
        << accuracyLower << ",\n"
        << "  \"medium_delay_accuracy\": "
        << conditionAccuracy[0] << ",\n"
        << "  \"long_delay_accuracy\": "
        << conditionAccuracy[1] << ",\n"
        << "  \"task_reliably_learned\": " << reliablyLearned << ",\n"
        << "  \"scoped_efficiency_replication\": "
        << scopedEfficiency << ",\n"
        << "  \"all_control_efficiency_replication\": "
        << allControlEfficiency << ",\n"
        << "  \"mode_means\": [\n";
    for (std::size_t mode = 0; mode < kModeCount; ++mode) {
        std::vector<double> accuracies;
        std::vector<double> costs;
        for (const auto& seed : aggregates) {
            accuracies.push_back(seed.accuracy[mode]);
            costs.push_back(seed.spikeCost[mode]);
        }
        summary << "    {\"mode\": \"" << modeName(kModes[mode])
            << "\", \"accuracy\": " << mean(accuracies)
            << ", \"spikes_per_correct\": " << mean(costs) << "}"
            << (mode + 1 == kModeCount ? "\n" : ",\n");
    }
    summary << "  ],\n  \"comparisons\": [\n";
    for (std::size_t index = 0; index < comparisons.size(); ++index) {
        const auto& item = comparisons[index];
        summary << "    {\"control\": \"" << modeName(item.control)
            << "\", \"accuracy_difference\": "
            << item.accuracyDifference
            << ", \"accuracy_lower_95_one_sided\": "
            << item.accuracyLowerBound
            << ", \"spike_cost_benefit\": " << item.costBenefit
            << ", \"cost_p_holm\": " << item.costHolmP << "}"
            << (index + 1 == comparisons.size() ? "\n" : ",\n");
    }
    summary << "  ],\n  \"decision\": \""
        << (
            reliablyLearned
                ? "DELAYED_XOR_RELIABLY_LEARNED"
                : "DELAYED_XOR_NOT_RELIABLY_LEARNED")
        << "\"\n}\n";

    std::ofstream report(
        outputDirectory / "HOLDOUT_REPORT.md",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report), "cannot create holdout report");
    report << "# Eingefrorene Delayed-XOR-Holdoutbestätigung\n\n"
        << "Modellhash: `" << memoryExperimentHash() << "`  \n"
        << "Bestätigungshash: `" << memoryConfirmationHash() << "`\n\n"
        << std::fixed << std::setprecision(6)
        << "- Kernel-Accuracy: " << kernelAccuracy << "\n"
        << "- untere einseitige 95-%-Grenze: " << accuracyLower << "\n"
        << "- mittlere Verzögerung: " << conditionAccuracy[0] << "\n"
        << "- lange Verzögerung: " << conditionAccuracy[1] << "\n"
        << "- zuverlässig gelernt: **"
        << (reliablyLearned ? "JA" : "NEIN") << "**\n"
        << "- scoped Effizienz repliziert: **"
        << (scopedEfficiency ? "JA" : "NEIN") << "**\n"
        << "- alle Kontrollen übertroffen: **"
        << (allControlEfficiency ? "JA" : "NEIN") << "**\n\n"
        << "| Kontrolle | ΔAccuracy | untere Grenze | "
        "Kostenvorteil | p Holm Kosten |\n"
        << "|---|---:|---:|---:|---:|\n";
    for (const auto& item : comparisons) {
        report << "| " << modeName(item.control)
            << " | " << item.accuracyDifference
            << " | " << item.accuracyLowerBound
            << " | " << item.costBenefit
            << " | " << item.costHolmP << " |\n";
    }
    report << "\nDie Bestätigungs-Seeds wurden erst nach Einfrieren "
        "des Modellhashes ausgewertet.\n";

    std::cout << std::fixed << std::setprecision(6)
        << "holdout_kernel_accuracy=" << kernelAccuracy << "\n"
        << "holdout_accuracy_lower=" << accuracyLower << "\n"
        << "task_reliably_learned=" << std::boolalpha
        << reliablyLearned << "\n"
        << "scoped_efficiency_replication=" << scopedEfficiency << "\n"
        << "all_control_efficiency_replication="
        << allControlEfficiency << "\n";
    return 0;
}

int runMemoryAblations(
    const std::filesystem::path& outputDirectory,
    const MemoryFeatureOptions& fullOptions) {
    fullOptions.validate();
    struct Variant {
        std::string name;
        agbnn::GateMode mode;
        MemoryFeatureOptions options;
    };
    MemoryFeatureOptions noDendrite = fullOptions;
    noDendrite.dendriteEnabled = false;
    MemoryFeatureOptions noEligibility = fullOptions;
    noEligibility.eligibilityEnabled = false;
    MemoryFeatureOptions noProducts = fullOptions;
    noProducts.interactionProductsEnabled = false;
    MemoryFeatureOptions toggledLocalEligibility = fullOptions;
    toggledLocalEligibility.localSynapticEligibilityEnabled =
        !fullOptions.localSynapticEligibilityEnabled;
    const std::array<Variant, 6> variants{{
        {"full_kernel", agbnn::GateMode::Kernel, fullOptions},
        {"without_dendrite", agbnn::GateMode::Kernel, noDendrite},
        {"without_eligibility", agbnn::GateMode::Kernel, noEligibility},
        {"without_products", agbnn::GateMode::Kernel, noProducts},
        {
            fullOptions.localSynapticEligibilityEnabled
                ? "without_local_synaptic_eligibility"
                : "with_local_synaptic_eligibility",
            agbnn::GateMode::Kernel,
            toggledLocalEligibility},
        {"sign_gate", agbnn::GateMode::Sign, fullOptions},
    }};
    using AblationCube =
        std::array<std::array<std::array<Evaluation, 2>, 16>, 6>;
    AblationCube results{};
    std::filesystem::create_directories(outputDirectory);
    for (std::size_t variant = 0; variant < variants.size(); ++variant) {
        for (
            std::size_t seed = 0;
            seed < kMemoryHoldoutSeeds.size();
            ++seed) {
            for (std::size_t condition = 0; condition < 2; ++condition) {
                results[variant][seed][condition] = evaluateDataset(
                    simulateMemoryDataset(
                        networkConfig(kMemoryHoldoutSeeds[seed]),
                        kConditions[condition],
                        variants[variant].mode,
                        false,
                        nullptr,
                        variants[variant].options),
                    variants[variant].mode);
            }
        }
        std::cout << "ablation " << variants[variant].name
            << " complete" << std::endl;
    }
    std::array<std::vector<double>, 6> seedAccuracies;
    std::array<std::vector<double>, 6> seedCosts;
    for (std::size_t variant = 0; variant < variants.size(); ++variant) {
        for (
            std::size_t seed = 0;
            seed < kMemoryHoldoutSeeds.size();
            ++seed) {
            seedAccuracies[variant].push_back(
                0.5 * (
                    results[variant][seed][0].accuracy
                    + results[variant][seed][1].accuracy));
            seedCosts[variant].push_back(
                0.5 * (
                    results[variant][seed][0].spikeCost
                    + results[variant][seed][1].spikeCost));
        }
    }
    std::ofstream raw(
        outputDirectory / "ablation_raw_results.csv",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(raw), "cannot create ablation CSV");
    raw << "seed,condition,variant,accuracy,spikes_per_correct\n"
        << std::fixed << std::setprecision(12);
    for (std::size_t variant = 0; variant < variants.size(); ++variant) {
        for (
            std::size_t seed = 0;
            seed < kMemoryHoldoutSeeds.size();
            ++seed) {
            for (std::size_t condition = 0; condition < 2; ++condition) {
                const auto& item = results[variant][seed][condition];
                raw << kMemoryHoldoutSeeds[seed] << ","
                    << kConditions[condition].name << ","
                    << variants[variant].name << ","
                    << item.accuracy << "," << item.spikeCost << "\n";
            }
        }
    }
    std::ofstream report(
        outputDirectory / "ABLATION_REPORT.md",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report), "cannot create ablation report");
    report << "# Delayed-XOR-Einzelablationen\n\n"
        << "Eligibility-Zeiten: `" << fullOptions.eligibilityTausMs[0]
        << ";" << fullOptions.eligibilityTausMs[1] << ";"
        << fullOptions.eligibilityTausMs[2] << " ms`\n\n"
        << "Lokale Synapsen-Eligibility: `"
        << (
            fullOptions.localSynapticEligibilityEnabled
                ? "aktiv"
                : "inaktiv")
        << "`, Parameter `tau="
        << fullOptions.localSynapticEligibilityTauMs
        << " ms, gain=" << fullOptions.localSynapticEligibilityGain
        << ", maximum="
        << fullOptions.localSynapticEligibilityMaximum
        << ", shift="
        << fullOptions.localSynapticEligibilityTimeShiftMs
        << " ms`\n\n"
        << "| Variante | Accuracy | Spikes/korrekt | ΔAccuracy zum Vollmodell "
        "| ΔKosten zum Vollmodell | p Kosten (Holm konservativ) |\n"
        << "|---|---:|---:|---:|---:|---:|\n"
        << std::fixed << std::setprecision(6);
    for (std::size_t variant = 0; variant < variants.size(); ++variant) {
        double accuracyDifference = 0.0;
        double costDifference = 0.0;
        double adjustedP = 1.0;
        if (variant > 0) {
            std::vector<double> accuracyDifferences;
            std::vector<double> costDifferences;
            for (
                std::size_t seed = 0;
                seed < kMemoryHoldoutSeeds.size();
                ++seed) {
                accuracyDifferences.push_back(
                    seedAccuracies[0][seed]
                    - seedAccuracies[variant][seed]);
                costDifferences.push_back(
                    seedCosts[variant][seed] - seedCosts[0][seed]);
            }
            accuracyDifference = mean(accuracyDifferences);
            costDifference = mean(costDifferences);
            adjustedP = std::min(
                1.0,
                5.0 * signFlipPValue(
                    costDifferences,
                    0xAB1A7100ULL + variant));
        }
        report << "| " << variants[variant].name
            << " | " << mean(seedAccuracies[variant])
            << " | " << mean(seedCosts[variant])
            << " | " << accuracyDifference
            << " | " << costDifference
            << " | " << adjustedP << " |\n";
    }
    report << "\nPositive ΔAccuracy/ΔKosten bedeuten einen Vorteil des "
        "Vollmodells. Jede Zeile ändert genau einen Mechanismus.\n";
    std::cout << "Ablation report: "
        << (outputDirectory / "ABLATION_REPORT.md").string() << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory =
            argc > 1
                ? std::filesystem::path(argv[1])
                : std::filesystem::current_path();
        if (argc > 2 && std::string(argv[2]) == "--memory-develop") {
            return runMemoryDevelopment(outputDirectory);
        }
        if (argc > 2 && std::string(argv[2]) == "--memory-confirm") {
            return runMemoryConfirmation(outputDirectory);
        }
        if (argc > 2 && std::string(argv[2]) == "--memory-ablate") {
            MemoryFeatureOptions options;
            if (argc > 3) {
                std::stringstream values(argv[3]);
                std::string token;
                for (std::size_t index = 0; index < 3; ++index) {
                    if (!std::getline(values, token, ';')) {
                        throw std::invalid_argument(
                            "Eligibility benötigt drei Werte");
                    }
                    options.eligibilityTausMs[index] = std::stod(token);
                }
                if (std::getline(values, token, ';')) {
                    throw std::invalid_argument(
                        "Eligibility benötigt genau drei Werte");
                }
            }
            if (argc > 4) {
                options.eligibilityEnabled = std::stoi(argv[4]) != 0;
            }
            if (argc > 5) {
                options.interactionProductsEnabled =
                    std::stoi(argv[5]) != 0;
            }
            if (argc > 6) {
                options.dendriteEnabled = std::stoi(argv[6]) != 0;
            }
            if (argc > 7) {
                options.localSynapticEligibilityEnabled =
                    std::stoi(argv[7]) != 0;
            }
            if (argc > 8) {
                std::stringstream values(argv[8]);
                std::string token;
                std::array<double*, 4> targets{{
                    &options.localSynapticEligibilityTauMs,
                    &options.localSynapticEligibilityGain,
                    &options.localSynapticEligibilityMaximum,
                    &options.localSynapticEligibilityTimeShiftMs,
                }};
                for (double* target : targets) {
                    if (!std::getline(values, token, ';')) {
                        throw std::invalid_argument(
                            "Lokale Eligibility benötigt "
                            "tau;gain;maximum;shift");
                    }
                    *target = std::stod(token);
                }
                if (std::getline(values, token, ';')) {
                    throw std::invalid_argument(
                        "Lokale Eligibility benötigt genau vier Werte");
                }
            }
            return runMemoryAblations(outputDirectory, options);
        }
        std::filesystem::create_directories(outputDirectory);
        ObservationCube observations{};
        std::cout << "Experiment GO-SNN-DXOR-MS24-D2-v1 hash="
            << experimentHash() << "\n";
        for (std::size_t seed = 0; seed < kSeeds.size(); ++seed) {
            for (
                std::size_t condition = 0;
                condition < kConditions.size();
                ++condition) {
                observations[seed][condition] =
                    compareAll(kSeeds[seed], kConditions[condition]);
            }
            std::cout << "seed " << (seed + 1) << "/" << kSeeds.size()
                << " (" << kSeeds[seed] << ") complete" << std::endl;
        }

        std::array<SeedAggregate, 24> aggregates{};
        for (std::size_t seed = 0; seed < kSeeds.size(); ++seed) {
            for (std::size_t mode = 0; mode < kModeCount; ++mode) {
                for (
                    std::size_t condition = 0;
                    condition < kConditions.size();
                    ++condition) {
                    aggregates[seed].accuracy[mode] +=
                        observations[seed][condition][mode].accuracy
                        / static_cast<double>(kConditions.size());
                    aggregates[seed].spikeCost[mode] +=
                        observations[seed][condition][mode].spikeCost
                        / static_cast<double>(kConditions.size());
                    aggregates[seed].firingRate[mode] +=
                        observations[seed][condition][mode].firingRate
                        / static_cast<double>(kConditions.size());
                }
            }
        }

        std::array<Comparison, kControlCount> comparisons{};
        std::array<double, kControlCount> accuracyP{};
        std::array<double, kControlCount> costP{};
        for (
            std::size_t control = 0;
            control < kControlCount;
            ++control) {
            const std::size_t mode = control + 1;
            std::vector<double> accuracyDifferences;
            std::vector<double> costBenefits;
            for (const auto& seed : aggregates) {
                accuracyDifferences.push_back(
                    seed.accuracy[0] - seed.accuracy[mode]);
                costBenefits.push_back(
                    seed.spikeCost[mode] - seed.spikeCost[0]);
            }
            Comparison& item = comparisons[control];
            item.control = kModes[mode];
            item.accuracyDifference = mean(accuracyDifferences);
            item.accuracyLowerBound = bootstrapLowerBound(
                accuracyDifferences,
                0xD31A0000ULL + control);
            item.accuracyP = signFlipPValue(
                accuracyDifferences,
                0xD31A1000ULL + control);
            item.costBenefit = mean(costBenefits);
            item.costP = signFlipPValue(
                costBenefits,
                0xD31A2000ULL + control);
            accuracyP[control] = item.accuracyP;
            costP[control] = item.costP;
        }
        const auto adjustedAccuracy = holmAdjust(accuracyP);
        const auto adjustedCost = holmAdjust(costP);
        for (std::size_t index = 0; index < comparisons.size(); ++index) {
            comparisons[index].accuracyHolmP =
                adjustedAccuracy[index];
            comparisons[index].costHolmP = adjustedCost[index];
        }

        const auto efficient = [&](std::size_t control) {
            const auto& item = comparisons[control];
            return item.accuracyLowerBound
                    > kNoninferiorityMargin
                && item.costBenefit > 0.0
                && item.costHolmP < 0.05;
        };
        const auto accuracySuperior = [&](std::size_t control) {
            const auto& item = comparisons[control];
            return item.accuracyDifference > 0.0
                && item.accuracyHolmP < 0.05;
        };
        const bool scopedReplication =
            efficient(0) && efficient(1) && efficient(4);
        const bool strongReplication =
            efficient(0)
            && efficient(1)
            && efficient(2)
            && efficient(3)
            && efficient(4);
        const bool accuracySuperiorityAll =
            accuracySuperior(0)
            && accuracySuperior(1)
            && accuracySuperior(2)
            && accuracySuperior(3)
            && accuracySuperior(4);

        writeRawCsv(outputDirectory / "raw_results.csv", observations);
        writeSummary(
            outputDirectory / "summary.json",
            aggregates,
            comparisons,
            scopedReplication,
            strongReplication,
            accuracySuperiorityAll);
        writeReport(
            outputDirectory / "RESULT_REPORT.md",
            aggregates,
            comparisons,
            scopedReplication,
            strongReplication,
            accuracySuperiorityAll);
        std::cout << "Results written to "
            << outputDirectory.string() << "\n"
            << "scoped_efficiency_replication=" << std::boolalpha
            << scopedReplication << "\n"
            << "strong_all_control_replication="
            << strongReplication << "\n"
            << "accuracy_superiority_all_controls="
            << accuracySuperiorityAll << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Delayed XOR experiment failed: "
            << error.what() << "\n";
        return 1;
    }
}
