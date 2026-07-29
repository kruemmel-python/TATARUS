#include "bio_core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kNeuronCount = 24;
constexpr int kSteps = 560;
constexpr int kCue1Start = 20;
constexpr int kCue1End = 40;
constexpr int kCue2Start = 60;
constexpr int kCue2End = 80;
constexpr int kRecallStart = 480;
constexpr int kRecallEnd = 500;
constexpr int kReadoutStart = 500;
constexpr int kReadoutEnd = 560;
constexpr int kRepetitions = 8;
constexpr int kFolds = 4;
constexpr double kCueCurrent = 27.0;
constexpr double kRecallCurrent = 24.0;
constexpr double kNoiseStd = 0.5;
constexpr double kLambda = 0.0005;
constexpr double kMu = 0.5;
constexpr std::size_t kMaximumParetoCandidates = 5;

const std::array<double, 5> kTaus{20.0, 50.0, 100.0, 200.0, 400.0};
const std::array<double, 5> kGains{0.0, 0.10, 0.20, 0.35, 0.50};
const std::array<double, 5> kMaximums{0.25, 0.5, 1.0, 2.0, 4.0};

const std::array<std::uint64_t, 4> kDevelopmentSeeds{
    4001, 4051, 4099, 4153};
const std::array<std::uint64_t, 12> kHoldoutSeeds{
    4211, 4271, 4337, 4409, 4481, 4561,
    4637, 4721, 4801, 4889, 4973, 5051};

struct TraceParameters {
    double tauMs = 100.0;
    double gain = 0.35;
    double maximum = 4.0;
};

struct Evaluation {
    double accuracy = 0.0;
    double balancedAccuracy = 0.0;
    double spikesPerCorrect = 0.0;
    double meanSpikes = 0.0;
    double meanEligibilityFactor = 1.0;
    double eligibilityFactorVariance = 0.0;
};

struct Dataset {
    std::vector<std::vector<double>> features;
    std::vector<int> labels;
    std::vector<int> folds;
    std::vector<double> eligibilityFactors;
    double totalSpikes = 0.0;
    double factorSum = 0.0;
    double factorSquareSum = 0.0;
    double factorCount = 0.0;
};

struct Candidate {
    TraceParameters parameters;
    double accuracy = 0.0;
    double spikeCost = 0.0;
    double preliminaryObjective = 0.0;
    double finalObjective = -std::numeric_limits<double>::infinity();
    double bestControlAccuracy = 0.0;
};

enum class Control {
    Signed,
    Disabled,
    GainZero,
    ConstantMatched,
    Absolute,
    TimeShifted,
    SynapseShuffled,
    Inverted,
    RandomMatched,
    EeOnly,
    IeOnly
};

const std::array<Control, 11> kControls{
    Control::Signed,
    Control::Disabled,
    Control::GainZero,
    Control::ConstantMatched,
    Control::Absolute,
    Control::TimeShifted,
    Control::SynapseShuffled,
    Control::Inverted,
    Control::RandomMatched,
    Control::EeOnly,
    Control::IeOnly};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

const char* controlName(Control control) {
    switch (control) {
        case Control::Signed: return "signed_trace";
        case Control::Disabled: return "without_trace";
        case Control::GainZero: return "gain_zero";
        case Control::ConstantMatched: return "event_matched_constant";
        case Control::Absolute: return "absolute_trace";
        case Control::TimeShifted: return "time_shifted_trace";
        case Control::SynapseShuffled: return "synapse_shuffled_trace";
        case Control::Inverted: return "inverted_trace";
        case Control::RandomMatched: return "distribution_matched_random";
        case Control::EeOnly: return "ee_only";
        case Control::IeOnly: return "ie_only";
    }
    return "unknown";
}

double mean(const std::vector<double>& values) {
    require(!values.empty(), "mean requires values");
    return std::accumulate(values.begin(), values.end(), 0.0)
        / static_cast<double>(values.size());
}

std::string parameterText(const TraceParameters& parameters) {
    std::ostringstream out;
    out << std::setprecision(12)
        << parameters.tauMs << ";"
        << parameters.gain << ";"
        << parameters.maximum;
    return out.str();
}

agbnn::NetworkConfig baseNetworkConfig(
    std::uint64_t seed,
    const TraceParameters& parameters) {
    agbnn::NetworkConfig config;
    config.neuronCount = kNeuronCount;
    config.excitatoryFraction = 0.75;
    config.connectionProbability = 0.22;
    config.seed = seed;
    config.gateMode = agbnn::GateMode::Disabled;
    config.gateTiming = agbnn::GateTiming::EmissionState;
    config.emissionFeature = agbnn::EmissionFeature::FeatureProjection;
    config.minimumAxonDelayMs = 1.0;
    config.maximumAxonDelayMs = 5.0;
    config.synapseModel = agbnn::SynapseModel::ConductanceBased;
    config.excitatoryConductanceScale = 0.03;
    config.inhibitoryConductanceScale = 0.03;
    config.dendriteEnabled = false;
    config.plasticityEnabled = false;
    config.adaptiveThresholdIncrementMv = 0.0;
    config.localEligibilityEnabled = true;
    config.localEligibilityTauMs = parameters.tauMs;
    config.localEligibilityGain = parameters.gain;
    config.localEligibilityMaximum = parameters.maximum;
    config.localEligibilityTimeShiftMs = 40.0;
    config.localEligibilityTransform =
        agbnn::LocalEligibilityTransform::Signed;
    config.localEligibilityScope = agbnn::LocalEligibilityScope::All;
    config.validate();
    return config;
}

std::vector<std::vector<double>> makeStimulus(
    int firstBit,
    int secondBit,
    int repetition,
    std::uint64_t seed) {
    require(firstBit == 0 || firstBit == 1, "invalid first cue");
    require(secondBit == 0 || secondBit == 1, "invalid second cue");
    std::uint64_t mixedSeed =
        seed
        ^ (static_cast<std::uint64_t>(repetition) + 1)
            * 0x9E3779B97F4A7C15ULL
        ^ 0x7AACE15E771A1ULL;
    std::mt19937_64 random(mixedSeed);
    std::normal_distribution<double> noise(0.0, kNoiseStd);
    std::vector<std::vector<double>> stimulus(
        kSteps,
        std::vector<double>(kNeuronCount, 0.0));
    const int split = kNeuronCount / 2;
    for (int step = 0; step < kSteps; ++step) {
        const bool firstCue =
            step >= kCue1Start && step < kCue1End;
        const bool secondCue =
            step >= kCue2Start && step < kCue2End;
        const bool recall =
            step >= kRecallStart && step < kRecallEnd;
        if (!firstCue && !secondCue && !recall) {
            continue;
        }
        for (int neuron = 0; neuron < kNeuronCount; ++neuron) {
            const int assembly = neuron < split ? 0 : 1;
            double value = recall ? noise(random) : 0.0;
            if (firstCue && assembly == firstBit) {
                value += kCueCurrent;
            }
            if (secondCue && assembly == secondBit) {
                value += kCueCurrent;
            }
            if (recall) {
                value += kRecallCurrent;
            }
            stimulus[static_cast<std::size_t>(step)]
                    [static_cast<std::size_t>(neuron)] = value;
        }
    }
    return stimulus;
}

void verifyStimulusProtocol() {
    std::array<std::vector<std::vector<double>>, 4> patterns{
        makeStimulus(0, 0, 0, 4001),
        makeStimulus(0, 1, 0, 4001),
        makeStimulus(1, 0, 0, 4001),
        makeStimulus(1, 1, 0, 4001)};
    std::array<double, 4> energy{};
    for (std::size_t pattern = 0; pattern < patterns.size(); ++pattern) {
        for (int step = 0; step < kSteps; ++step) {
            for (int neuron = 0; neuron < kNeuronCount; ++neuron) {
                const double value = patterns[pattern]
                    [static_cast<std::size_t>(step)]
                    [static_cast<std::size_t>(neuron)];
                energy[pattern] += value * value;
                if (step >= kCue2End && step < kRecallStart) {
                    require(value == 0.0, "delay phase is not stimulus-free");
                }
                if (step >= kRecallStart) {
                    require(
                        value
                            == patterns[0][static_cast<std::size_t>(step)]
                                [static_cast<std::size_t>(neuron)],
                        "recall/readout input differs between classes");
                }
            }
        }
    }
    for (std::size_t pattern = 1; pattern < energy.size(); ++pattern) {
        require(
            std::abs(energy[pattern] - energy[0]) < 1e-8,
            "input energy differs between cue patterns");
    }
}

std::vector<double> extractRecallOnlyFeatures(
    const agbnn::SimulationResult& result,
    const agbnn::NetworkConfig& config) {
    require(
        static_cast<int>(result.spikes.size()) >= kReadoutEnd,
        "simulation shorter than recall readout");
    constexpr int subwindows = 3;
    const int split = config.neuronCount / 2;
    std::vector<double> features(
        static_cast<std::size_t>(config.neuronCount * 3 + 6),
        0.0);
    std::vector<double> traces(
        static_cast<std::size_t>(config.neuronCount),
        0.0);
    const double traceDecay = std::exp(-config.dtMs / 20.0);
    for (int step = kReadoutStart; step < kReadoutEnd; ++step) {
        const int subwindow = std::min(
            subwindows - 1,
            (step - kReadoutStart) * subwindows
                / (kReadoutEnd - kReadoutStart));
        for (int neuron = 0; neuron < config.neuronCount; ++neuron) {
            const std::size_t index = static_cast<std::size_t>(neuron);
            const double spike = result.spikes[
                static_cast<std::size_t>(step)][index];
            features[index] += spike;
            features[static_cast<std::size_t>(
                config.neuronCount + neuron)] +=
                (
                    result.voltagesMv[
                        static_cast<std::size_t>(step)][index]
                    - config.restingMv)
                / (config.thresholdMv - config.restingMv);
            traces[index] = traces[index] * traceDecay + spike;
            const int assembly = neuron < split ? 0 : 1;
            features[static_cast<std::size_t>(
                config.neuronCount * 3
                + subwindow * 2 + assembly)] += spike;
        }
    }
    const double windowSteps = kReadoutEnd - kReadoutStart;
    for (int neuron = 0; neuron < config.neuronCount; ++neuron) {
        features[static_cast<std::size_t>(
            config.neuronCount + neuron)] /= windowSteps;
        features[static_cast<std::size_t>(
            config.neuronCount * 2 + neuron)] =
                traces[static_cast<std::size_t>(neuron)];
    }
    return features;
}

void standardize(
    std::vector<std::vector<double>>& train,
    std::vector<std::vector<double>>& test) {
    const std::size_t dimensions = train.front().size();
    std::vector<double> means(dimensions, 0.0);
    std::vector<double> scales(dimensions, 1.0);
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
            scales[column] = std::sqrt(variance);
        }
    }
    for (auto* rows : {&train, &test}) {
        for (auto& row : *rows) {
            for (std::size_t column = 0; column < dimensions; ++column) {
                row[column] =
                    (row[column] - means[column]) / scales[column];
            }
        }
    }
}

std::vector<double> trainReadout(
    const std::vector<std::vector<double>>& features,
    const std::vector<int>& labels) {
    constexpr int epochs = 400;
    constexpr double learningRate = 0.10;
    constexpr double l2 = 0.005;
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
            const double probability = 1.0 / (1.0 + std::exp(-score));
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
    const agbnn::NetworkConfig& config,
    bool collectFactors) {
    Dataset dataset;
    dataset.features.reserve(4 * kRepetitions);
    dataset.labels.reserve(4 * kRepetitions);
    dataset.folds.reserve(4 * kRepetitions);
    for (int firstBit = 0; firstBit <= 1; ++firstBit) {
        for (int secondBit = 0; secondBit <= 1; ++secondBit) {
            for (int repetition = 0;
                 repetition < kRepetitions;
                 ++repetition) {
                agbnn::SpikingNetwork network(config);
                const auto result = network.run(makeStimulus(
                    firstBit,
                    secondBit,
                    repetition,
                    config.seed));
                require(result.metrics.finite, "non-finite trace run");
                dataset.features.push_back(
                    extractRecallOnlyFeatures(result, config));
                dataset.labels.push_back(firstBit ^ secondBit);
                dataset.folds.push_back(repetition % kFolds);
                dataset.totalSpikes += result.metrics.totalSpikes;
                const double count = static_cast<double>(
                    result.eligibilityTransmissionFactors.size());
                dataset.factorCount += count;
                dataset.factorSum +=
                    result.metrics.meanEligibilityTransmissionFactor
                    * count;
                dataset.factorSquareSum +=
                    (
                        result.metrics
                            .eligibilityTransmissionFactorVariance
                        + result.metrics.meanEligibilityTransmissionFactor
                            * result.metrics
                                .meanEligibilityTransmissionFactor)
                    * count;
                if (collectFactors) {
                    dataset.eligibilityFactors.insert(
                        dataset.eligibilityFactors.end(),
                        result.eligibilityTransmissionFactors.begin(),
                        result.eligibilityTransmissionFactors.end());
                }
            }
        }
    }
    return dataset;
}

Evaluation evaluateDataset(const Dataset& dataset) {
    Evaluation evaluation;
    int correct = 0;
    int predictions = 0;
    std::array<int, 2> classCorrect{0, 0};
    std::array<int, 2> classTotal{0, 0};
    for (int fold = 0; fold < kFolds; ++fold) {
        std::vector<std::vector<double>> trainFeatures;
        std::vector<std::vector<double>> testFeatures;
        std::vector<int> trainLabels;
        std::vector<int> testLabels;
        for (std::size_t row = 0; row < dataset.features.size(); ++row) {
            if (dataset.folds[row] == fold) {
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
            for (std::size_t column = 0;
                 column < testFeatures[row].size();
                 ++column) {
                score += weights[column] * testFeatures[row][column];
            }
            const int prediction = score >= 0.0 ? 1 : 0;
            const int truth = testLabels[row];
            ++predictions;
            ++classTotal[static_cast<std::size_t>(truth)];
            if (prediction == truth) {
                ++correct;
                ++classCorrect[static_cast<std::size_t>(truth)];
            }
        }
    }
    evaluation.accuracy =
        correct / static_cast<double>(predictions);
    evaluation.balancedAccuracy = 0.5 * (
        classCorrect[0] / static_cast<double>(classTotal[0])
        + classCorrect[1] / static_cast<double>(classTotal[1]));
    evaluation.meanSpikes =
        dataset.totalSpikes / dataset.features.size();
    evaluation.spikesPerCorrect =
        correct > 0
            ? dataset.totalSpikes / static_cast<double>(correct)
            : std::numeric_limits<double>::infinity();
    if (dataset.factorCount > 0.0) {
        evaluation.meanEligibilityFactor =
            dataset.factorSum / dataset.factorCount;
        evaluation.eligibilityFactorVariance = std::max(
            0.0,
            dataset.factorSquareSum / dataset.factorCount
                - evaluation.meanEligibilityFactor
                    * evaluation.meanEligibilityFactor);
    }
    return evaluation;
}

agbnn::NetworkConfig controlConfig(
    std::uint64_t seed,
    const TraceParameters& parameters,
    Control control,
    const std::vector<double>& matchedFactors) {
    agbnn::NetworkConfig config = baseNetworkConfig(seed, parameters);
    switch (control) {
        case Control::Signed:
            break;
        case Control::Disabled:
            config.localEligibilityEnabled = false;
            break;
        case Control::GainZero:
            config.localEligibilityGain = 0.0;
            break;
        case Control::ConstantMatched:
            require(!matchedFactors.empty(), "constant matching is empty");
            config.localEligibilityTransform =
                agbnn::LocalEligibilityTransform::ConstantMatched;
            config.localEligibilityConstantFactor =
                mean(matchedFactors);
            break;
        case Control::Absolute:
            config.localEligibilityTransform =
                agbnn::LocalEligibilityTransform::Absolute;
            break;
        case Control::TimeShifted:
            config.localEligibilityTransform =
                agbnn::LocalEligibilityTransform::TimeShifted;
            break;
        case Control::SynapseShuffled:
            config.localEligibilityTransform =
                agbnn::LocalEligibilityTransform::SynapseShuffled;
            break;
        case Control::Inverted:
            config.localEligibilityTransform =
                agbnn::LocalEligibilityTransform::Inverted;
            break;
        case Control::RandomMatched:
            require(!matchedFactors.empty(), "random matching is empty");
            config.localEligibilityTransform =
                agbnn::LocalEligibilityTransform::RandomMatched;
            config.localEligibilityRandomFactors = matchedFactors;
            break;
        case Control::EeOnly:
            config.localEligibilityScope =
                agbnn::LocalEligibilityScope::ExcitatoryToExcitatory;
            break;
        case Control::IeOnly:
            config.localEligibilityScope =
                agbnn::LocalEligibilityScope::InhibitoryToExcitatory;
            break;
    }
    config.validate();
    return config;
}

std::array<Evaluation, 11> evaluateControls(
    std::uint64_t seed,
    const TraceParameters& parameters) {
    std::array<Evaluation, 11> evaluations{};
    const auto signedConfig =
        controlConfig(seed, parameters, Control::Signed, {});
    const Dataset signedDataset = simulateDataset(signedConfig, true);
    require(
        !signedDataset.eligibilityFactors.empty(),
        "signed trace produced no transmitted factors");
    evaluations[0] = evaluateDataset(signedDataset);
    for (std::size_t index = 1; index < kControls.size(); ++index) {
        const auto config = controlConfig(
            seed,
            parameters,
            kControls[index],
            signedDataset.eligibilityFactors);
        evaluations[index] = evaluateDataset(
            simulateDataset(config, false));
    }
    return evaluations;
}

double exactSignFlipPValue(const std::vector<double>& differences) {
    require(!differences.empty(), "sign-flip requires differences");
    require(differences.size() <= 20, "too many exact sign flips");
    const double observed = std::abs(mean(differences));
    const std::uint64_t combinations =
        std::uint64_t{1} << differences.size();
    std::uint64_t extreme = 0;
    for (std::uint64_t mask = 0; mask < combinations; ++mask) {
        double sum = 0.0;
        for (std::size_t index = 0; index < differences.size(); ++index) {
            sum += ((mask >> index) & 1U)
                ? differences[index]
                : -differences[index];
        }
        if (
            std::abs(sum / differences.size())
            >= observed - 1e-15) {
            ++extreme;
        }
    }
    return extreme / static_cast<double>(combinations);
}

std::string experimentHash(const TraceParameters& parameters) {
    std::ostringstream descriptor;
    descriptor << std::setprecision(17)
        << "GO-SNN-TRACE-ESSENTIAL-v1;"
        << parameterText(parameters)
        << ";steps=" << kSteps
        << ";cue1=" << kCue1Start << "-" << kCue1End
        << ";cue2=" << kCue2Start << "-" << kCue2End
        << ";recall=" << kRecallStart << "-" << kRecallEnd
        << ";readout=" << kReadoutStart << "-" << kReadoutEnd
        << ";repetitions=" << kRepetitions << ";";
    for (auto seed : kHoldoutSeeds) {
        descriptor << seed << ",";
    }
    std::uint64_t hash = 14695981039346656037ULL;
    const std::string text = descriptor.str();
    for (unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << std::setw(16) << hash;
    return out.str();
}

std::vector<Candidate> paretoCandidates(
    const std::vector<Candidate>& candidates) {
    std::vector<Candidate> frontier;
    for (const auto& candidate : candidates) {
        bool dominated = false;
        for (const auto& other : candidates) {
            const bool noWorse =
                other.accuracy >= candidate.accuracy
                && other.spikeCost <= candidate.spikeCost;
            const bool strictlyBetter =
                other.accuracy > candidate.accuracy
                || other.spikeCost < candidate.spikeCost;
            if (noWorse && strictlyBetter) {
                dominated = true;
                break;
            }
        }
        if (!dominated) {
            frontier.push_back(candidate);
        }
    }
    std::sort(
        frontier.begin(),
        frontier.end(),
        [](const Candidate& left, const Candidate& right) {
            return left.preliminaryObjective
                > right.preliminaryObjective;
        });
    if (frontier.size() > kMaximumParetoCandidates) {
        frontier.resize(kMaximumParetoCandidates);
    }
    return frontier;
}

TraceParameters runDevelopment(
    const std::filesystem::path& outputDirectory) {
    verifyStimulusProtocol();
    std::filesystem::create_directories(outputDirectory);
    std::ofstream grid(
        outputDirectory / "development_grid.csv",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(grid), "cannot create development grid");
    grid << "tau_ms,gain,maximum,accuracy,spikes_per_correct,"
            "preliminary_objective\n"
        << std::fixed << std::setprecision(12);
    std::vector<Candidate> candidates;
    candidates.reserve(125);
    for (double tau : kTaus) {
        for (double gain : kGains) {
            for (double maximum : kMaximums) {
                const TraceParameters parameters{tau, gain, maximum};
                std::vector<double> accuracies;
                std::vector<double> costs;
                for (std::uint64_t seed : kDevelopmentSeeds) {
                    const auto evaluation = evaluateDataset(
                        simulateDataset(
                            baseNetworkConfig(seed, parameters),
                            false));
                    accuracies.push_back(evaluation.accuracy);
                    costs.push_back(evaluation.spikesPerCorrect);
                }
                Candidate candidate;
                candidate.parameters = parameters;
                candidate.accuracy = mean(accuracies);
                candidate.spikeCost = mean(costs);
                candidate.preliminaryObjective =
                    candidate.accuracy - kLambda * candidate.spikeCost;
                candidates.push_back(candidate);
                grid << tau << "," << gain << "," << maximum << ","
                    << candidate.accuracy << "," << candidate.spikeCost
                    << "," << candidate.preliminaryObjective << "\n";
            }
        }
        std::cout << "development tau=" << tau << " complete"
            << std::endl;
    }
    auto frontier = paretoCandidates(candidates);
    require(!frontier.empty(), "Pareto frontier is empty");
    std::ofstream controls(
        outputDirectory / "development_controls.csv",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(controls), "cannot create controls CSV");
    controls << "candidate,seed,control,accuracy,spikes_per_correct,"
                "mean_factor,factor_variance\n"
        << std::fixed << std::setprecision(12);
    for (auto& candidate : frontier) {
        std::array<std::vector<double>, 11> accuracies;
        std::array<std::vector<double>, 11> costs;
        for (std::uint64_t seed : kDevelopmentSeeds) {
            const auto evaluations =
                evaluateControls(seed, candidate.parameters);
            for (std::size_t control = 0;
                 control < kControls.size();
                 ++control) {
                accuracies[control].push_back(
                    evaluations[control].accuracy);
                costs[control].push_back(
                    evaluations[control].spikesPerCorrect);
                controls << "\"" << parameterText(candidate.parameters)
                    << "\"," << seed << ","
                    << controlName(kControls[control]) << ","
                    << evaluations[control].accuracy << ","
                    << evaluations[control].spikesPerCorrect << ","
                    << evaluations[control].meanEligibilityFactor << ","
                    << evaluations[control]
                        .eligibilityFactorVariance << "\n";
            }
        }
        candidate.accuracy = mean(accuracies[0]);
        candidate.spikeCost = mean(costs[0]);
        candidate.bestControlAccuracy = 0.0;
        for (std::size_t control = 1;
             control < kControls.size();
             ++control) {
            candidate.bestControlAccuracy = std::max(
                candidate.bestControlAccuracy,
                mean(accuracies[control]));
        }
        candidate.finalObjective =
            candidate.accuracy
            - kLambda * candidate.spikeCost
            + kMu
                * (candidate.accuracy
                    - candidate.bestControlAccuracy);
    }
    std::sort(
        frontier.begin(),
        frontier.end(),
        [](const Candidate& left, const Candidate& right) {
            return left.finalObjective > right.finalObjective;
        });
    std::ofstream pareto(
        outputDirectory / "pareto_candidates.csv",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(pareto), "cannot create Pareto CSV");
    pareto << "rank,tau_ms,gain,maximum,accuracy,spikes_per_correct,"
              "best_control_accuracy,final_objective\n"
        << std::fixed << std::setprecision(12);
    for (std::size_t rank = 0; rank < frontier.size(); ++rank) {
        const auto& candidate = frontier[rank];
        pareto << (rank + 1) << ","
            << candidate.parameters.tauMs << ","
            << candidate.parameters.gain << ","
            << candidate.parameters.maximum << ","
            << candidate.accuracy << ","
            << candidate.spikeCost << ","
            << candidate.bestControlAccuracy << ","
            << candidate.finalObjective << "\n";
    }
    const Candidate& selected = frontier.front();
    std::ofstream frozen(
        outputDirectory / "FROZEN_CANDIDATE.txt",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(frozen), "cannot freeze candidate");
    frozen << std::setprecision(17)
        << parameterText(selected.parameters) << "\n"
        << "development_accuracy=" << selected.accuracy << "\n"
        << "development_spikes_per_correct="
        << selected.spikeCost << "\n"
        << "best_control_accuracy="
        << selected.bestControlAccuracy << "\n"
        << "objective=" << selected.finalObjective << "\n";
    std::ofstream report(
        outputDirectory / "DEVELOPMENT_REPORT.md",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report), "cannot write development report");
    report << "# Trace-essential Memory – Entwicklung\n\n"
        << "Grobsuche: `5 × 5 × 5 = 125` Kombinationen auf "
        << kDevelopmentSeeds.size() << " Entwicklungsseeds.\n\n"
        << "Zielfunktion: `J = Accuracy - " << kLambda
        << " × Spikekosten + " << kMu
        << " × (Accuracy_Spur - Accuracy_beste_Kontrolle)`.\n\n"
        << "Eingefrorener Kandidat: `"
        << parameterText(selected.parameters) << "`\n\n"
        << std::fixed << std::setprecision(6)
        << "- Accuracy: " << selected.accuracy << "\n"
        << "- Spikes/korrekt: " << selected.spikeCost << "\n"
        << "- beste Kontrollaccuracy: "
        << selected.bestControlAccuracy << "\n"
        << "- J: " << selected.finalObjective << "\n\n"
        << "Die Holdout-Seeds wurden in dieser Phase nicht simuliert.\n";
    std::cout << "frozen_candidate="
        << parameterText(selected.parameters) << "\n";
    return selected.parameters;
}

int runConfirmation(
    const std::filesystem::path& outputDirectory,
    const TraceParameters& parameters) {
    verifyStimulusProtocol();
    std::filesystem::create_directories(outputDirectory);
    using EvaluationRow = std::array<Evaluation, 11>;
    std::array<EvaluationRow, kHoldoutSeeds.size()> rows{};
    std::ofstream raw(
        outputDirectory / "holdout_raw.csv",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(raw), "cannot create holdout CSV");
    raw << "seed,control,accuracy,balanced_accuracy,spikes_per_correct,"
           "mean_spikes,mean_factor,factor_variance\n"
        << std::fixed << std::setprecision(12);
    for (std::size_t seed = 0; seed < kHoldoutSeeds.size(); ++seed) {
        rows[seed] =
            evaluateControls(kHoldoutSeeds[seed], parameters);
        for (std::size_t control = 0;
             control < kControls.size();
             ++control) {
            const auto& evaluation = rows[seed][control];
            raw << kHoldoutSeeds[seed] << ","
                << controlName(kControls[control]) << ","
                << evaluation.accuracy << ","
                << evaluation.balancedAccuracy << ","
                << evaluation.spikesPerCorrect << ","
                << evaluation.meanSpikes << ","
                << evaluation.meanEligibilityFactor << ","
                << evaluation.eligibilityFactorVariance << "\n";
        }
        std::cout << "holdout seed " << (seed + 1) << "/"
            << kHoldoutSeeds.size() << " complete" << std::endl;
    }
    std::array<std::vector<double>, 11> accuracies;
    std::array<std::vector<double>, 11> costs;
    for (std::size_t seed = 0; seed < rows.size(); ++seed) {
        for (std::size_t control = 0;
             control < kControls.size();
             ++control) {
            accuracies[control].push_back(
                rows[seed][control].accuracy);
            costs[control].push_back(
                rows[seed][control].spikesPerCorrect);
        }
    }
    const double fullAccuracy = mean(accuracies[0]);
    bool superiorToEveryControl = true;
    std::array<double, 11> adjustedAccuracyP{};
    std::array<double, 11> adjustedCostP{};
    for (std::size_t control = 1;
         control < kControls.size();
         ++control) {
        std::vector<double> accuracyDifferences;
        std::vector<double> costDifferences;
        for (std::size_t seed = 0; seed < rows.size(); ++seed) {
            accuracyDifferences.push_back(
                accuracies[0][seed] - accuracies[control][seed]);
            costDifferences.push_back(
                costs[control][seed] - costs[0][seed]);
        }
        adjustedAccuracyP[control] = std::min(
            1.0,
            10.0 * exactSignFlipPValue(accuracyDifferences));
        adjustedCostP[control] = std::min(
            1.0,
            10.0 * exactSignFlipPValue(costDifferences));
        superiorToEveryControl =
            superiorToEveryControl
            && mean(accuracyDifferences) > 0.03
            && adjustedAccuracyP[control] < 0.05;
    }
    const bool nullControlsAtChance =
        mean(accuracies[1]) <= 0.55
        && mean(accuracies[2]) <= 0.55;
    const bool taskLearned = fullAccuracy >= 0.65;
    const bool traceEssential =
        taskLearned && nullControlsAtChance
        && superiorToEveryControl;
    std::vector<double> shiftedVsDisabledDifferences;
    for (std::size_t seed = 0; seed < rows.size(); ++seed) {
        shiftedVsDisabledDifferences.push_back(
            accuracies[5][seed] - accuracies[1][seed]);
    }
    const double shiftedVsDisabledDifference =
        mean(shiftedVsDisabledDifferences);
    const double shiftedVsDisabledP =
        exactSignFlipPValue(shiftedVsDisabledDifferences);
    const bool traceFamilyMemory =
        mean(accuracies[5]) >= 0.65
        && mean(accuracies[1]) <= 0.55
        && shiftedVsDisabledDifference > 0.03
        && shiftedVsDisabledP < 0.05;

    std::ofstream report(
        outputDirectory / "REPORT.md",
        std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report), "cannot create report");
    report << "# Forschungsstufe 15: Trace-essential Memory\n\n"
        << "Experimenthash: `" << experimentHash(parameters) << "`\n\n"
        << "Eingefrorene Parameter: `tau=" << parameters.tauMs
        << " ms; Gain=" << parameters.gain
        << "; Maximum=" << parameters.maximum << "`\n\n"
        << "Das Readout verwendet ausschließlich Spikecounts, "
           "Spiketraces und Membranspannungen aus `"
        << kReadoutStart << "..." << (kReadoutEnd - 1)
        << " ms`. Cue-gebundene Eligibility und Produktmerkmale "
           "sind nicht enthalten.\n\n"
        << "| Kontrolle | Accuracy | Spikes/korrekt | "
           "ΔAccuracy Spur−Kontrolle | p Accuracy (Holm konservativ) | "
           "p Kosten (Holm konservativ) |\n"
        << "|---|---:|---:|---:|---:|---:|\n"
        << std::fixed << std::setprecision(6);
    for (std::size_t control = 0;
         control < kControls.size();
         ++control) {
        report << "| " << controlName(kControls[control])
            << " | " << mean(accuracies[control])
            << " | " << mean(costs[control])
            << " | "
            << (fullAccuracy - mean(accuracies[control]))
            << " | "
            << (
                control == 0
                    ? 1.0
                    : adjustedAccuracyP[control])
            << " | "
            << (
                control == 0
                    ? 1.0
                    : adjustedCostP[control])
            << " |\n";
    }
    report << "\n## Vorab definierte Entscheidung\n\n"
        << "- Vollmodell-Accuracy ≥ 0,65: **"
        << (taskLearned ? "JA" : "NEIN") << "**\n"
        << "- ohne Spur und Gain=0 ≤ 0,55: **"
        << (nullControlsAtChance ? "JA" : "NEIN") << "**\n"
        << "- >0,03 und Holm-p<0,05 gegen jede Kontrolle: **"
        << (superiorToEveryControl ? "JA" : "NEIN") << "**\n\n"
        << "Ergebnis: **"
        << (
            traceEssential
                ? "TRACE_ESSENTIAL_MEMORY_CONFIRMED"
                : "TRACE_ESSENTIAL_MEMORY_NOT_CONFIRMED")
        << "**\n\n"
        << "## Explorative Mechanik innerhalb der Spur-Familie\n\n"
        << "Die vorab definierte 40-ms-Timingkontrolle ist selbst eine "
           "lokale Eligibility-Spur, aber mit älterem synaptischem "
           "Zustand. Sie erreicht Accuracy `"
        << mean(accuracies[5]) << "` gegenüber `"
        << mean(accuracies[1]) << "` ohne Spur. Gepaarte Differenz: `"
        << shiftedVsDisabledDifference
        << "`, unkorrigiertes exaktes p: `"
        << shiftedVsDisabledP << "`.\n\n"
        << "Explorativer Befund lokaler Spur-Familien-Memory: **"
        << (traceFamilyMemory ? "JA" : "NEIN") << "**. Dieser Befund "
           "ersetzt die negative vorab definierte Entscheidung nicht und "
           "benötigt neue Bestätigungsseeds.\n\n"
        << "Dies ist ein synthetischer Gedächtnistest und keine "
           "biologische Validierung.\n";
    std::cout << std::fixed << std::setprecision(6)
        << "holdout_accuracy=" << fullAccuracy << "\n"
        << "trace_essential=" << std::boolalpha
        << traceEssential << "\n"
        << "exploratory_trace_family_memory="
        << traceFamilyMemory << "\n"
        << "report=" << (outputDirectory / "REPORT.md").string()
        << "\n";
    return traceEssential ? 0 : 2;
}

TraceParameters parseParameters(const std::string& text) {
    TraceParameters parameters;
    std::stringstream input(text);
    std::string token;
    std::array<double*, 3> targets{
        &parameters.tauMs,
        &parameters.gain,
        &parameters.maximum};
    for (double* target : targets) {
        if (!std::getline(input, token, ';')) {
            throw std::invalid_argument(
                "parameters require tau;gain;maximum");
        }
        *target = std::stod(token);
    }
    if (std::getline(input, token, ';')) {
        throw std::invalid_argument(
            "parameters require exactly three values");
    }
    baseNetworkConfig(4001, parameters).validate();
    return parameters;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory =
            argc > 1
                ? std::filesystem::path(argv[1])
                : std::filesystem::current_path();
        const std::string mode = argc > 2 ? argv[2] : "--full";
        if (mode == "--develop") {
            (void)runDevelopment(outputDirectory);
            return 0;
        }
        if (mode == "--confirm") {
            if (argc < 4) {
                throw std::invalid_argument(
                    "--confirm requires tau;gain;maximum");
            }
            return runConfirmation(
                outputDirectory,
                parseParameters(argv[3]));
        }
        if (mode == "--full") {
            const auto developmentDirectory =
                outputDirectory / "development";
            const auto holdoutDirectory =
                outputDirectory / "holdout";
            const TraceParameters frozen =
                runDevelopment(developmentDirectory);
            return runConfirmation(holdoutDirectory, frozen);
        }
        throw std::invalid_argument(
            "mode must be --develop, --confirm or --full");
    } catch (const std::exception& error) {
        std::cerr << "trace-essential experiment failed: "
            << error.what() << "\n";
        return 1;
    }
}
