#include "classifier.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace agbnn {
namespace {

double mean(const std::vector<double>& values) {
    return values.empty()
        ? 0.0
        : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
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
        means[column] /= train.size();
        double variance = 0.0;
        for (const auto& row : train) {
            const double centered = row[column] - means[column];
            variance += centered * centered;
        }
        variance /= train.size();
        if (variance > 1e-12) {
            deviations[column] = std::sqrt(variance);
        }
    }
    auto transform = [&](std::vector<std::vector<double>>& rows) {
        for (auto& row : rows) {
            for (std::size_t column = 0; column < dimensions; ++column) {
                row[column] =
                    (row[column] - means[column]) / deviations[column];
            }
        }
    };
    transform(train);
    transform(test);
}

}  // namespace

void ClassificationConfig::validate() const {
    if (samplesPerClass < 2 || samplesPerClass > 500) {
        throw std::invalid_argument("Samples/Klasse muss zwischen 2 und 500 liegen");
    }
    if (folds < 2 || folds > samplesPerClass) {
        throw std::invalid_argument("Folds muss zwischen 2 und Samples/Klasse liegen");
    }
    if (steps < timeBins || steps > 10000 || timeBins < 2 || timeBins > 32) {
        throw std::invalid_argument("Ungültige Schritte oder Zeitfenster");
    }
    if (!std::isfinite(baselineCurrent)
        || !std::isfinite(pulseCurrent)
        || !std::isfinite(noiseStd)
        || noiseStd < 0.0
        || !std::isfinite(learningRate)
        || learningRate <= 0.0
        || trainingEpochs <= 0
        || !std::isfinite(l2)
        || l2 < 0.0) {
        throw std::invalid_argument("Ungültige Lern- oder Rauschparameter");
    }
}

double GateEvaluation::meanAccuracy() const {
    return mean(foldAccuracies);
}

double GateEvaluation::accuracyStddev() const {
    const double average = meanAccuracy();
    double variance = 0.0;
    for (double value : foldAccuracies) {
        variance += (value - average) * (value - average);
    }
    return foldAccuracies.empty()
        ? 0.0
        : std::sqrt(variance / foldAccuracies.size());
}

double GateEvaluation::meanBalancedAccuracy() const {
    return mean(foldBalancedAccuracies);
}

double GateEvaluation::meanAssemblySeparation() const {
    return mean(foldAssemblySeparations);
}

TemporalClassifier::TemporalClassifier(
    NetworkConfig networkConfig,
    ClassificationConfig classificationConfig)
    : networkConfig_(std::move(networkConfig)),
      classificationConfig_(std::move(classificationConfig)) {
    networkConfig_.validate();
    classificationConfig_.validate();
}

std::vector<double> TemporalClassifier::extractFeatures(
    const SimulationResult& result) const {
    const int bins = classificationConfig_.timeBins;
    const int neurons = networkConfig_.neuronCount;
    const int split = std::max(1, neurons / 2);
    const std::array<int, 2> assemblySizes{
        split, std::max(1, neurons - split)};
    std::vector<double> features(static_cast<std::size_t>(bins * 2), 0.0);
    for (std::size_t step = 0; step < result.spikes.size(); ++step) {
        const int timeBin = std::min(
            bins - 1,
            static_cast<int>(step * bins / result.spikes.size()));
        for (int neuron = 0; neuron < neurons; ++neuron) {
            const int assembly = neuron < split ? 0 : 1;
            features[static_cast<std::size_t>(timeBin * 2 + assembly)] +=
                result.spikes[step][static_cast<std::size_t>(neuron)];
        }
    }
    const double binDurationSeconds =
        networkConfig_.dtMs * classificationConfig_.steps
        / bins / 1000.0;
    for (std::size_t index = 0; index < features.size(); ++index) {
        features[index] /= std::max(
            1e-12,
            binDurationSeconds
                * assemblySizes[index % 2]);
    }
    return features;
}

std::vector<double> TemporalClassifier::trainReadout(
    const std::vector<std::vector<double>>& features,
    const std::vector<int>& labels) const {
    const std::size_t dimensions = features.front().size();
    std::vector<double> weights(dimensions + 1, 0.0);
    for (int epoch = 0;
         epoch < classificationConfig_.trainingEpochs;
         ++epoch) {
        std::vector<double> gradient(weights.size(), 0.0);
        for (std::size_t rowIndex = 0;
             rowIndex < features.size();
             ++rowIndex) {
            double score = weights.back();
            for (std::size_t column = 0; column < dimensions; ++column) {
                score += weights[column] * features[rowIndex][column];
            }
            score = std::clamp(score, -30.0, 30.0);
            const double probability = 1.0 / (1.0 + std::exp(-score));
            const double error = probability - labels[rowIndex];
            for (std::size_t column = 0; column < dimensions; ++column) {
                gradient[column] += error * features[rowIndex][column];
            }
            gradient.back() += error;
        }
        for (std::size_t column = 0; column < dimensions; ++column) {
            gradient[column] =
                gradient[column] / features.size()
                + classificationConfig_.l2 * weights[column];
        }
        gradient.back() /= features.size();
        for (std::size_t index = 0; index < weights.size(); ++index) {
            weights[index] -=
                classificationConfig_.learningRate * gradient[index];
        }
    }
    return weights;
}

GateEvaluation TemporalClassifier::evaluate(
    GateMode mode,
    bool overrideClassOperators) const {
    const int totalSamples = classificationConfig_.samplesPerClass * 2;
    std::vector<std::vector<double>> features;
    std::vector<int> labels;
    std::vector<int> sampleIndices;
    features.reserve(static_cast<std::size_t>(totalSamples));
    labels.reserve(static_cast<std::size_t>(totalSamples));
    sampleIndices.reserve(static_cast<std::size_t>(totalSamples));
    double gateSum = 0.0;
    double effectiveGateWeightedSum = 0.0;
    double effectiveGateSquareWeightedSum = 0.0;
    double effectiveGateEntropyWeightedSum = 0.0;
    double effectiveGateCount = 0.0;
    double rateSum = 0.0;
    double spikeSum = 0.0;
    for (int label = 0; label <= 1; ++label) {
        for (int sample = 0;
             sample < classificationConfig_.samplesPerClass;
             ++sample) {
            NetworkConfig config = networkConfig_;
            config.gateMode = mode;
            if (
                config.classSpecificOperatorsEnabled
                && overrideClassOperators) {
                config.eeGateMode = mode;
                config.eiGateMode = mode;
                config.ieGateMode = mode;
                config.iiGateMode = mode;
            }
            SpikingNetwork network(config);
            const auto stimulus = makeTemporalStimulus(
                config.neuronCount,
                classificationConfig_.steps,
                classificationConfig_.timeBins,
                label,
                sample,
                classificationConfig_.seed,
                classificationConfig_.baselineCurrent,
                classificationConfig_.pulseCurrent,
                classificationConfig_.noiseStd);
            const SimulationResult result = network.run(stimulus);
            features.push_back(extractFeatures(result));
            labels.push_back(label);
            sampleIndices.push_back(sample);
            gateSum += result.metrics.meanGate;
            const double sampleEffectiveCount =
                result.metrics.effectiveGateCount;
            effectiveGateCount += sampleEffectiveCount;
            effectiveGateWeightedSum +=
                result.metrics.effectiveGateMean
                * sampleEffectiveCount;
            effectiveGateSquareWeightedSum +=
                (
                    result.metrics.effectiveGateVariance
                    + result.metrics.effectiveGateMean
                        * result.metrics.effectiveGateMean)
                * sampleEffectiveCount;
            effectiveGateEntropyWeightedSum +=
                result.metrics.effectiveGateEntropyBits
                * sampleEffectiveCount;
            rateSum += result.metrics.meanFiringRateHz;
            spikeSum += result.metrics.totalSpikes;
        }
    }

    GateEvaluation evaluation;
    evaluation.gateMode = mode;
    evaluation.featureCount =
        static_cast<int>(features.front().size());
    evaluation.meanGate = gateSum / totalSamples;
    evaluation.meanEffectiveGate =
        effectiveGateCount > 0.0
            ? effectiveGateWeightedSum / effectiveGateCount
            : 0.0;
    evaluation.meanEffectiveGateVariance =
        effectiveGateCount > 0.0
            ? std::max(
                0.0,
                effectiveGateSquareWeightedSum / effectiveGateCount
                    - evaluation.meanEffectiveGate
                        * evaluation.meanEffectiveGate)
            : 0.0;
    evaluation.meanEffectiveGateEntropyBits =
        effectiveGateCount > 0.0
            ? effectiveGateEntropyWeightedSum / effectiveGateCount
            : 0.0;
    evaluation.meanFiringRateHz = rateSum / totalSamples;
    evaluation.meanSpikesPerSample = spikeSum / totalSamples;
    int totalCorrect = 0;
    for (int fold = 0; fold < classificationConfig_.folds; ++fold) {
        std::vector<std::vector<double>> trainFeatures;
        std::vector<std::vector<double>> testFeatures;
        std::vector<int> trainLabels;
        std::vector<int> testLabels;
        for (std::size_t index = 0; index < features.size(); ++index) {
            if (sampleIndices[index] % classificationConfig_.folds == fold) {
                testFeatures.push_back(features[index]);
                testLabels.push_back(labels[index]);
            } else {
                trainFeatures.push_back(features[index]);
                trainLabels.push_back(labels[index]);
            }
        }
        standardize(trainFeatures, testFeatures);

        std::array<std::vector<double>, 2> centroids{
            std::vector<double>(testFeatures.front().size(), 0.0),
            std::vector<double>(testFeatures.front().size(), 0.0)};
        std::array<int, 2> centroidCounts{0, 0};
        for (std::size_t row = 0; row < testFeatures.size(); ++row) {
            const std::size_t label =
                static_cast<std::size_t>(testLabels[row]);
            ++centroidCounts[label];
            for (std::size_t column = 0;
                 column < testFeatures[row].size();
                 ++column) {
                centroids[label][column] += testFeatures[row][column];
            }
        }
        for (std::size_t label = 0; label < 2; ++label) {
            for (double& value : centroids[label]) {
                value /= std::max(1, centroidCounts[label]);
            }
        }
        double squaredSeparation = 0.0;
        for (std::size_t column = 0;
             column < centroids[0].size();
             ++column) {
            const double difference =
                centroids[0][column] - centroids[1][column];
            squaredSeparation += difference * difference;
        }
        evaluation.foldAssemblySeparations.push_back(
            std::sqrt(squaredSeparation));

        const std::vector<double> weights =
            trainReadout(trainFeatures, trainLabels);
        int correct = 0;
        std::array<int, 2> classCorrect{0, 0};
        std::array<int, 2> classTotal{0, 0};
        for (std::size_t index = 0; index < testFeatures.size(); ++index) {
            double score = weights.back();
            for (std::size_t column = 0;
                 column < testFeatures[index].size();
                 ++column) {
                score += weights[column] * testFeatures[index][column];
            }
            const int prediction = score >= 0.0 ? 1 : 0;
            const int truth = testLabels[index];
            evaluation.confusion[static_cast<std::size_t>(truth)]
                                [static_cast<std::size_t>(prediction)]++;
            classTotal[static_cast<std::size_t>(truth)]++;
            if (prediction == truth) {
                ++correct;
                classCorrect[static_cast<std::size_t>(truth)]++;
            }
        }
        evaluation.foldAccuracies.push_back(
            correct / static_cast<double>(testLabels.size()));
        totalCorrect += correct;
        evaluation.foldBalancedAccuracies.push_back(
            0.5 * (
                classCorrect[0] / static_cast<double>(classTotal[0])
                + classCorrect[1] / static_cast<double>(classTotal[1])));
    }
    evaluation.spikesPerCorrectDecision =
        totalCorrect > 0 ? spikeSum / totalCorrect : 0.0;
    return evaluation;
}

std::vector<GateEvaluation> TemporalClassifier::compareAll() const {
    std::vector<GateEvaluation> evaluations;
    evaluations.reserve(6);
    evaluations.push_back(evaluate(GateMode::Kernel));

    NetworkConfig calibratedConfig = networkConfig_;
    calibratedConfig.gateMode = GateMode::Kernel;
    if (calibratedConfig.classSpecificOperatorsEnabled) {
        calibratedConfig.eeGateMode = GateMode::Kernel;
        calibratedConfig.eiGateMode = GateMode::Kernel;
        calibratedConfig.ieGateMode = GateMode::Kernel;
        calibratedConfig.iiGateMode = GateMode::Kernel;
    }
    std::vector<double> empiricalEffectiveGates;
    for (int label = 0; label <= 1; ++label) {
        for (int sample = 0;
             sample < classificationConfig_.samplesPerClass;
             ++sample) {
            SpikingNetwork network(calibratedConfig);
            const auto stimulus = makeTemporalStimulus(
                calibratedConfig.neuronCount,
                classificationConfig_.steps,
                classificationConfig_.timeBins,
                label,
                sample,
                classificationConfig_.seed,
                classificationConfig_.baselineCurrent,
                classificationConfig_.pulseCurrent,
                classificationConfig_.noiseStd);
            const SimulationResult result = network.run(stimulus);
            empiricalEffectiveGates.insert(
                empiricalEffectiveGates.end(),
                result.effectiveTransmissionGates.begin(),
                result.effectiveTransmissionGates.end());
        }
    }
    if (!empiricalEffectiveGates.empty()) {
        calibratedConfig.constantGate =
            std::accumulate(
                empiricalEffectiveGates.begin(),
                empiricalEffectiveGates.end(),
                0.0)
            / empiricalEffectiveGates.size();
        calibratedConfig.randomGateValues = empiricalEffectiveGates;
    }

    const TemporalClassifier calibratedClassifier(
        calibratedConfig, classificationConfig_);
    for (GateMode mode : {
             GateMode::Constant,
             GateMode::Disabled,
             GateMode::Sign,
             GateMode::Tanh,
             GateMode::Random}) {
        evaluations.push_back(calibratedClassifier.evaluate(mode));
    }
    return evaluations;
}

}  // namespace agbnn
