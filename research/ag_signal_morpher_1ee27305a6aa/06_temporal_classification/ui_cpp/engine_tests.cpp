#include "bio_core.hpp"
#include "classifier.hpp"
#include "ag_signal_morpher_1ee27305a6aa_kernel.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using ag_signal_morpher_1ee27305a6aa_kernel::kernel;
    assert(std::abs(kernel(-1.0) - (-0.9579888296551812)) < 1e-14);
    assert(std::abs(kernel(0.0) - 0.13991628339787235) < 1e-14);
    assert(std::abs(kernel(1.0) - 1.0420111703448187) < 1e-14);

    agbnn::NetworkConfig networkConfig;
    networkConfig.neuronCount = 12;
    networkConfig.seed = 73;
    const auto stimulus = agbnn::makeTemporalStimulus(
        networkConfig.neuronCount, 80, 4, 0, 0, 3801, 12.5, 2.0, 5.0);
    agbnn::SpikingNetwork firstNetwork(networkConfig);
    agbnn::SpikingNetwork secondNetwork(networkConfig);
    const auto first = firstNetwork.run(stimulus);
    const auto second = secondNetwork.run(stimulus);
    assert(first.spikes == second.spikes);
    assert(first.voltagesMv == second.voltagesMv);
    assert(first.metrics.finite);
    assert(firstNetwork.dalePrincipleHolds());
    assert(std::abs(
        first.metrics.effectiveGateMean
        - 0.12831112128784755) < 1e-14);
    assert(first.metrics.effectiveGateVariance == 0.0);

    agbnn::NetworkConfig eventConstantConfig = networkConfig;
    eventConstantConfig.gateMode = agbnn::GateMode::Constant;
    eventConstantConfig.constantGate = first.metrics.effectiveGateMean;
    agbnn::SpikingNetwork eventConstantNetwork(eventConstantConfig);
    const auto eventConstant = eventConstantNetwork.run(stimulus);
    assert(first.spikes == eventConstant.spikes);
    assert(first.voltagesMv == eventConstant.voltagesMv);

    agbnn::NetworkConfig emissionConfig = networkConfig;
    emissionConfig.gateTiming = agbnn::GateTiming::EmissionState;
    agbnn::SpikingNetwork emissionNetwork(emissionConfig);
    const auto emission = emissionNetwork.run(stimulus);
    assert(emission.metrics.effectiveGateMean
        > first.metrics.effectiveGateMean);

    agbnn::NetworkConfig eiConfig = networkConfig;
    eiConfig.gateTiming = agbnn::GateTiming::EmissionState;
    eiConfig.emissionFeature = agbnn::EmissionFeature::EiBalance;
    const auto eiStimulus = agbnn::makeTemporalStimulus(
        eiConfig.neuronCount, 180, 4, 0, 0, 3801, 13.5, 10.0, 0.6);
    agbnn::SpikingNetwork eiNetwork(eiConfig);
    const auto ei = eiNetwork.run(eiStimulus);
    assert(ei.metrics.spikeEventCount == ei.metrics.totalSpikes);
    assert(ei.events.size()
        == static_cast<std::size_t>(ei.metrics.totalSpikes));
    assert(ei.metrics.effectiveGateVariance > 1e-6);
    assert(ei.metrics.effectiveGateEntropyBits > 0.0);
    assert(ei.metrics.eventFeatureMinimum < 0.0);
    assert(ei.metrics.eventFeatureMaximum > 0.0);

    agbnn::NetworkConfig projectionConfig = eiConfig;
    projectionConfig.emissionFeature =
        agbnn::EmissionFeature::FeatureProjection;
    agbnn::SpikingNetwork projectionNetwork(projectionConfig);
    const auto projection = projectionNetwork.run(eiStimulus);
    assert(!projection.events.empty());
    for (const auto& event : projection.events) {
        const double reconstructed =
            projectionConfig.projectionEiWeight * event.eiBalance
            + projectionConfig.projectionSlopeWeight * event.membraneSlope
            + projectionConfig.projectionOvershootWeight
                * event.thresholdOvershoot
            + projectionConfig.projectionIsiWeight * event.isiState;
        assert(std::abs(reconstructed - event.featureValue) < 1e-14);
        assert(event.eiBalance >= -1.0 && event.eiBalance <= 1.0);
        assert(event.membraneSlope >= -1.0 && event.membraneSlope <= 1.0);
        assert(event.thresholdOvershoot >= 0.0
            && event.thresholdOvershoot <= 1.0);
        assert(event.isiState >= -1.0 && event.isiState <= 1.0);
    }
    assert(projection.events.front().isiState == -1.0);
    assert(projection.metrics.effectiveGateVariance > 1e-6);
    assert(projection.metrics.eventMembraneSlopeVariance > 0.0);
    assert(projection.metrics.eventThresholdOvershootVariance > 0.0);
    assert(projection.metrics.eventIsiStateVariance > 0.0);

    agbnn::NetworkConfig zeroGainEligibilityConfig = projectionConfig;
    zeroGainEligibilityConfig.localEligibilityEnabled = true;
    zeroGainEligibilityConfig.localEligibilityTauMs = 75.0;
    zeroGainEligibilityConfig.localEligibilityGain = 0.0;
    zeroGainEligibilityConfig.localEligibilityMaximum = 3.0;
    agbnn::SpikingNetwork zeroGainEligibilityNetwork(
        zeroGainEligibilityConfig);
    const auto zeroGainEligibility =
        zeroGainEligibilityNetwork.run(eiStimulus);
    assert(zeroGainEligibility.spikes == projection.spikes);
    assert(zeroGainEligibility.voltagesMv == projection.voltagesMv);
    assert(zeroGainEligibility.finalWeights == projection.finalWeights);
    assert(std::all_of(
        zeroGainEligibility.eligibilityTransmissionFactors.begin(),
        zeroGainEligibility.eligibilityTransmissionFactors.end(),
        [](double value) { return value == 1.0; }));
    assert(
        zeroGainEligibility.metrics.maximumAbsoluteLocalEligibility > 0.0);

    agbnn::NetworkConfig localEligibilityConfig =
        zeroGainEligibilityConfig;
    localEligibilityConfig.localEligibilityGain = 0.35;
    agbnn::SpikingNetwork localEligibilityFirstNetwork(
        localEligibilityConfig);
    agbnn::SpikingNetwork localEligibilitySecondNetwork(
        localEligibilityConfig);
    const auto localEligibilityFirst =
        localEligibilityFirstNetwork.run(eiStimulus);
    const auto localEligibilitySecond =
        localEligibilitySecondNetwork.run(eiStimulus);
    assert(localEligibilityFirst.spikes == localEligibilitySecond.spikes);
    assert(
        localEligibilityFirst.voltagesMv
        == localEligibilitySecond.voltagesMv);
    assert(
        localEligibilityFirst.finalLocalEligibility
        == localEligibilitySecond.finalLocalEligibility);
    assert(
        localEligibilityFirst.metrics.localEligibilitySynapseCount > 0);
    assert(
        localEligibilityFirst.metrics.localEligibilityVariance > 0.0);
    assert(
        localEligibilityFirst.metrics
            .maximumAbsoluteLocalEligibility
        <= localEligibilityConfig.localEligibilityMaximum);
    assert(
        localEligibilityFirst.metrics
            .eligibilityTransmissionFactorVariance
        > 0.0);
    for (
        std::size_t connection = 0;
        connection < localEligibilityFirst.finalWeights.size();
        ++connection) {
        if (localEligibilityFirst.finalWeights[connection] == 0.0) {
            assert(
                localEligibilityFirst
                    .finalLocalEligibility[connection]
                == 0.0);
        }
    }

    agbnn::NetworkConfig constantEligibilityConfig =
        localEligibilityConfig;
    constantEligibilityConfig.localEligibilityTransform =
        agbnn::LocalEligibilityTransform::ConstantMatched;
    constantEligibilityConfig.localEligibilityConstantFactor = 1.17;
    const auto constantEligibility =
        agbnn::SpikingNetwork(constantEligibilityConfig).run(eiStimulus);
    assert(!constantEligibility.eligibilityTransmissionFactors.empty());
    assert(std::all_of(
        constantEligibility.eligibilityTransmissionFactors.begin(),
        constantEligibility.eligibilityTransmissionFactors.end(),
        [](double value) { return value == 1.17; }));

    agbnn::NetworkConfig randomEligibilityConfig =
        localEligibilityConfig;
    randomEligibilityConfig.localEligibilityTransform =
        agbnn::LocalEligibilityTransform::RandomMatched;
    randomEligibilityConfig.localEligibilityRandomFactors = {0.8, 1.2};
    const auto randomEligibilityFirst =
        agbnn::SpikingNetwork(randomEligibilityConfig).run(eiStimulus);
    const auto randomEligibilitySecond =
        agbnn::SpikingNetwork(randomEligibilityConfig).run(eiStimulus);
    assert(
        randomEligibilityFirst.eligibilityTransmissionFactors
        == randomEligibilitySecond.eligibilityTransmissionFactors);
    assert(std::all_of(
        randomEligibilityFirst.eligibilityTransmissionFactors.begin(),
        randomEligibilityFirst.eligibilityTransmissionFactors.end(),
        [](double value) {
            return value == 0.8 || value == 1.2;
        }));

    agbnn::NetworkConfig eeEligibilityConfig = localEligibilityConfig;
    eeEligibilityConfig.localEligibilityScope =
        agbnn::LocalEligibilityScope::ExcitatoryToExcitatory;
    const auto eeEligibility =
        agbnn::SpikingNetwork(eeEligibilityConfig).run(eiStimulus);
    const int excitatoryCount = std::clamp(
        static_cast<int>(std::lround(
            eeEligibilityConfig.neuronCount
            * eeEligibilityConfig.excitatoryFraction)),
        1,
        eeEligibilityConfig.neuronCount);
    for (int post = 0; post < eeEligibilityConfig.neuronCount; ++post) {
        for (int pre = 0; pre < eeEligibilityConfig.neuronCount; ++pre) {
            if (post < excitatoryCount && pre < excitatoryCount) {
                continue;
            }
            const std::size_t connection = static_cast<std::size_t>(
                post * eeEligibilityConfig.neuronCount + pre);
            assert(
                eeEligibility.finalLocalEligibility[connection] == 0.0);
        }
    }

    for (const auto transform : {
             agbnn::LocalEligibilityTransform::Absolute,
             agbnn::LocalEligibilityTransform::TimeShifted,
             agbnn::LocalEligibilityTransform::SynapseShuffled,
             agbnn::LocalEligibilityTransform::Inverted}) {
        agbnn::NetworkConfig transformConfig = localEligibilityConfig;
        transformConfig.localEligibilityTransform = transform;
        const auto transformedFirst =
            agbnn::SpikingNetwork(transformConfig).run(eiStimulus);
        const auto transformedSecond =
            agbnn::SpikingNetwork(transformConfig).run(eiStimulus);
        assert(transformedFirst.spikes == transformedSecond.spikes);
        assert(
            transformedFirst.eligibilityTransmissionFactors
            == transformedSecond.eligibilityTransmissionFactors);
        assert(transformedFirst.metrics.finite);
    }

    agbnn::NetworkConfig advancedConfig = projectionConfig;
    advancedConfig.minimumAxonDelayMs = 1.0;
    advancedConfig.maximumAxonDelayMs = 5.0;
    advancedConfig.synapseModel =
        agbnn::SynapseModel::ConductanceBased;
    advancedConfig.dendriteEnabled = true;
    advancedConfig.externalToDendriteFraction = 0.0;
    advancedConfig.classSpecificOperatorsEnabled = true;
    advancedConfig.eeGateMode = agbnn::GateMode::Kernel;
    advancedConfig.eiGateMode = agbnn::GateMode::Sign;
    advancedConfig.ieGateMode = agbnn::GateMode::Tanh;
    advancedConfig.iiGateMode = agbnn::GateMode::Constant;
    agbnn::SpikingNetwork advancedFirstNetwork(advancedConfig);
    agbnn::SpikingNetwork advancedSecondNetwork(advancedConfig);
    const auto advancedFirst = advancedFirstNetwork.run(eiStimulus);
    const auto advancedSecond = advancedSecondNetwork.run(eiStimulus);
    assert(advancedFirst.spikes == advancedSecond.spikes);
    assert(advancedFirst.voltagesMv == advancedSecond.voltagesMv);
    assert(advancedFirst.dendriticVoltagesMv
        == advancedSecond.dendriticVoltagesMv);
    assert(advancedFirst.metrics.finite);
    assert(advancedFirst.metrics.meanAxonDelayMs >= 1.0);
    assert(advancedFirst.metrics.meanAxonDelayMs <= 5.0);
    assert(advancedFirst.metrics.dendriticVoltageEnergy > 0.0);
    assert(advancedFirst.metrics.synapticTransmissionCount > 0);
    assert(!advancedFirst.effectiveTransmissionGates.empty());

    agbnn::ClassificationConfig advancedTask;
    advancedTask.samplesPerClass = 4;
    advancedTask.folds = 2;
    advancedTask.steps = 80;
    advancedTask.baselineCurrent = 16.0;
    advancedTask.trainingEpochs = 40;
    const auto advancedComparison =
        agbnn::TemporalClassifier(advancedConfig, advancedTask)
            .compareAll();
    assert(advancedComparison.size() == 6);
    for (const auto& evaluation : advancedComparison) {
        assert(std::isfinite(evaluation.meanAccuracy()));
        assert(std::isfinite(evaluation.meanEffectiveGate));
        assert(std::isfinite(evaluation.spikesPerCorrectDecision));
    }
    const auto longAdvancedStimulus = agbnn::makeTemporalStimulus(
        advancedConfig.neuronCount,
        600,
        4,
        1,
        3,
        991,
        15.0,
        2.0,
        5.0);
    agbnn::SpikingNetwork longAdvancedNetwork(advancedConfig);
    const auto longAdvanced =
        longAdvancedNetwork.run(longAdvancedStimulus);
    assert(longAdvanced.metrics.finite);
    assert(longAdvanced.metrics.totalSpikes > 0);
    assert(longAdvancedNetwork.dalePrincipleHolds());

    agbnn::ClassificationConfig taskConfig;
    taskConfig.samplesPerClass = 6;
    taskConfig.folds = 2;
    taskConfig.steps = 80;
    taskConfig.trainingEpochs = 80;
    agbnn::TemporalClassifier classifier(networkConfig, taskConfig);
    const auto result = classifier.evaluate(agbnn::GateMode::Kernel);
    assert(result.foldAccuracies.size() == 2);
    assert(result.featureCount == 8);
    assert(result.meanAccuracy() >= 0.0 && result.meanAccuracy() <= 1.0);
    assert(result.meanEffectiveGateEntropyBits >= 0.0);
    assert(result.meanSpikesPerSample > 0.0);
    assert(result.spikesPerCorrectDecision > 0.0);
    assert(result.foldAssemblySeparations.size() == 2);
    assert(result.confusion[0][0] + result.confusion[0][1] == 6);
    assert(result.confusion[1][0] + result.confusion[1][1] == 6);
    std::cout << "C++ engine tests passed\n";
    return 0;
}
