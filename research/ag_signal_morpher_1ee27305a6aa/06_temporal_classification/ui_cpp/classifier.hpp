#pragma once

#include "bio_core.hpp"

#include <array>
#include <vector>

namespace agbnn {

struct ClassificationConfig {
    int samplesPerClass = 24;
    int folds = 4;
    int steps = 120;
    int timeBins = 4;
    double baselineCurrent = 12.5;
    double pulseCurrent = 2.0;
    double noiseStd = 5.0;
    std::uint64_t seed = 3801;
    double learningRate = 0.18;
    int trainingEpochs = 350;
    double l2 = 0.002;

    void validate() const;
};

struct GateEvaluation {
    GateMode gateMode = GateMode::Kernel;
    std::vector<double> foldAccuracies;
    std::vector<double> foldBalancedAccuracies;
    std::array<std::array<int, 2>, 2> confusion{{{0, 0}, {0, 0}}};
    int featureCount = 0;
    double meanGate = 0.0;
    double meanEffectiveGate = 0.0;
    double meanEffectiveGateVariance = 0.0;
    double meanEffectiveGateEntropyBits = 0.0;
    double meanFiringRateHz = 0.0;
    double meanSpikesPerSample = 0.0;
    double spikesPerCorrectDecision = 0.0;
    std::vector<double> foldAssemblySeparations;

    double meanAccuracy() const;
    double accuracyStddev() const;
    double meanBalancedAccuracy() const;
    double meanAssemblySeparation() const;
};

class TemporalClassifier {
public:
    TemporalClassifier(
        NetworkConfig networkConfig,
        ClassificationConfig classificationConfig);
    GateEvaluation evaluate(
        GateMode mode,
        bool overrideClassOperators = true) const;
    std::vector<GateEvaluation> compareAll() const;

private:
    NetworkConfig networkConfig_;
    ClassificationConfig classificationConfig_;

    std::vector<double> extractFeatures(
        const SimulationResult& result) const;
    std::vector<double> trainReadout(
        const std::vector<std::vector<double>>& features,
        const std::vector<int>& labels) const;
};

}  // namespace agbnn
