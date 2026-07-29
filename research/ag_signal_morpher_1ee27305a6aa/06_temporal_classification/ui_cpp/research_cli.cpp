#include "bio_core.hpp"
#include "classifier.hpp"

#include <iomanip>
#include <iostream>

int main() {
    agbnn::NetworkConfig network;
    network.neuronCount = 16;
    network.seed = 38;
    network.constantGate = 0.12831112128784755;
    network.gateTiming = agbnn::GateTiming::EmissionState;
    network.emissionFeature = agbnn::EmissionFeature::FeatureProjection;
    network.minimumAxonDelayMs = 1.0;
    network.maximumAxonDelayMs = 5.0;
    network.synapseModel = agbnn::SynapseModel::ConductanceBased;
    network.dendriteEnabled = true;
    network.externalToDendriteFraction = 0.0;
    network.classSpecificOperatorsEnabled = true;
    network.plasticityEnabled = true;

    agbnn::ClassificationConfig task;
    task.samplesPerClass = 24;
    task.folds = 4;
    task.steps = 120;
    task.seed = 38;
    task.pulseCurrent = 2.0;
    task.noiseStd = 5.0;
    task.baselineCurrent = 15.0;

    const agbnn::TemporalClassifier classifier(network, task);
    const auto results = classifier.compareAll();
    std::cout << std::fixed << std::setprecision(6);
    for (const auto& result : results) {
        std::wcout
            << agbnn::gateModeName(result.gateMode)
            << L": accuracy=" << result.meanAccuracy()
            << L", stddev=" << result.accuracyStddev()
            << L", rate_hz=" << result.meanFiringRateHz
            << L", mean_gate=" << result.meanGate
            << L", effective_gate=" << result.meanEffectiveGate
            << L", effective_variance="
            << result.meanEffectiveGateVariance
            << L", entropy_bits="
            << result.meanEffectiveGateEntropyBits
            << L", assembly_separation="
            << result.meanAssemblySeparation()
            << L", spikes_per_correct="
            << result.spikesPerCorrectDecision
            << L'\n';
    }
    return 0;
}
