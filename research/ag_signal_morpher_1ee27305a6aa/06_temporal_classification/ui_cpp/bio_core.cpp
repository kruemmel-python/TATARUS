#include "bio_core.hpp"

#include "ag_signal_morpher_1ee27305a6aa_kernel.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace agbnn {
namespace {

double clampGate(double value) {
    return std::clamp(value, 0.05, 0.95);
}

double generatedKernel(double value) {
    return ag_signal_morpher_1ee27305a6aa_kernel::kernel(value);
}

}  // namespace

const wchar_t* gateModeName(GateMode mode) {
    switch (mode) {
        case GateMode::Kernel: return L"Originalkernel";
        case GateMode::Constant: return L"Konstant";
        case GateMode::Disabled: return L"Deaktiviert";
        case GateMode::Sign: return L"Vorzeichen";
        case GateMode::Tanh: return L"Tanh";
        case GateMode::Random: return L"Zufall";
    }
    return L"Unbekannt";
}

const wchar_t* gateTimingName(GateTiming timing) {
    switch (timing) {
        case GateTiming::ResetLocked: return L"Reset-gebunden";
        case GateTiming::EmissionState: return L"Emissionszustand";
    }
    return L"Unbekannt";
}

const wchar_t* gatePerturbationName(GatePerturbation perturbation) {
    switch (perturbation) {
        case GatePerturbation::None: return L"Keine";
        case GatePerturbation::TimeShifted: return L"Zeitverschoben";
        case GatePerturbation::StateShuffled: return L"State-shuffled";
    }
    return L"Unbekannt";
}

const wchar_t* emissionFeatureName(EmissionFeature feature) {
    switch (feature) {
        case EmissionFeature::PreResetVoltage: return L"Pre-reset Spannung";
        case EmissionFeature::EiBalance: return L"E/I-Balance";
        case EmissionFeature::FeatureProjection: return L"4-Feature-Projektion";
    }
    return L"Unbekannt";
}

const wchar_t* synapseModelName(SynapseModel model) {
    switch (model) {
        case SynapseModel::CurrentBased: return L"Strombasiert";
        case SynapseModel::ConductanceBased: return L"AMPA/GABA-Leitwert";
    }
    return L"Unbekannt";
}

const wchar_t* localEligibilityTransformName(
    LocalEligibilityTransform transform) {
    switch (transform) {
        case LocalEligibilityTransform::Signed:
            return L"Signierte Spur";
        case LocalEligibilityTransform::Absolute:
            return L"Vorzeichenlose |e|-Spur";
        case LocalEligibilityTransform::TimeShifted:
            return L"Zeitverschobene Spur";
        case LocalEligibilityTransform::SynapseShuffled:
            return L"Synapsenvertauschte Spur";
        case LocalEligibilityTransform::Inverted:
            return L"Invertierte Spur";
        case LocalEligibilityTransform::ConstantMatched:
            return L"Event-gematchte Konstante";
        case LocalEligibilityTransform::RandomMatched:
            return L"Verteilungsgematchter Zufall";
    }
    return L"Unbekannt";
}

const wchar_t* localEligibilityScopeName(LocalEligibilityScope scope) {
    switch (scope) {
        case LocalEligibilityScope::All:
            return L"Alle Synapsen";
        case LocalEligibilityScope::ExcitatoryToExcitatory:
            return L"Nur E→E";
        case LocalEligibilityScope::InhibitoryToExcitatory:
            return L"Nur I→E";
    }
    return L"Unbekannt";
}

void NetworkConfig::validate() const {
    if (neuronCount < 2 || neuronCount > 256) {
        throw std::invalid_argument("Neuronen muss zwischen 2 und 256 liegen");
    }
    if (!(excitatoryFraction > 0.0 && excitatoryFraction <= 1.0)) {
        throw std::invalid_argument("Exzitatorischer Anteil muss in (0,1] liegen");
    }
    if (!(connectionProbability >= 0.0 && connectionProbability <= 1.0)) {
        throw std::invalid_argument("Verbindungsdichte muss in [0,1] liegen");
    }
    if (!(constantGate >= 0.0 && constantGate <= 1.0)) {
        throw std::invalid_argument("Konstantes Gate muss in [0,1] liegen");
    }
    if (!std::all_of(
            randomGateValues.begin(),
            randomGateValues.end(),
            [](double value) {
                return std::isfinite(value)
                    && value >= 0.0
                    && value <= 1.0;
            })) {
        throw std::invalid_argument("Empirische Zufallsgates müssen in [0,1] liegen");
    }
    if (dtMs <= 0.0 || tauMembraneMs <= 0.0 || tauSynapseMs <= 0.0) {
        throw std::invalid_argument("Zeitparameter müssen positiv sein");
    }
    if (!std::isfinite(restingMv)
        || !std::isfinite(resetMv)
        || !std::isfinite(thresholdMv)
        || thresholdMv <= restingMv
        || resetMv > restingMv
        || !std::isfinite(refractoryMs)
        || refractoryMs < 0.0) {
        throw std::invalid_argument("Ungültige Membranpotentiale/Refraktärzeit");
    }
    if (!std::isfinite(excitatoryWeight)
        || excitatoryWeight < 0.0
        || !std::isfinite(inhibitoryWeight)
        || inhibitoryWeight < 0.0
        || !std::isfinite(maximumWeight)
        || maximumWeight <= 0.0
        || !std::isfinite(adaptiveThresholdIncrementMv)
        || !std::isfinite(adaptiveThresholdTauMs)
        || adaptiveThresholdTauMs <= 0.0) {
        throw std::invalid_argument("Ungültige Gewichts-/Adaptationsparameter");
    }
    if (!std::isfinite(gateInputScale)
        || gateInputScale <= 0.0
        || !std::isfinite(randomGateAmplitude)
        || randomGateAmplitude < 0.0
        || randomGateAmplitude > 1.0) {
        throw std::invalid_argument("Ungültige interne Gateparameter");
    }
    if (!std::isfinite(stdpLearningRate)
        || stdpLearningRate < 0.0
        || !std::isfinite(stdpTauMs)
        || stdpTauMs <= 0.0
        || !std::isfinite(stdpPotentiation)
        || stdpPotentiation < 0.0
        || !std::isfinite(stdpDepression)
        || stdpDepression < 0.0) {
        throw std::invalid_argument("Ungültige STDP-Parameter");
    }
    if (!std::isfinite(localEligibilityTauMs)
        || localEligibilityTauMs <= 0.0
        || !std::isfinite(localEligibilityGain)
        || localEligibilityGain < 0.0
        || localEligibilityGain > 1.0
        || !std::isfinite(localEligibilityMaximum)
        || localEligibilityMaximum <= 0.0
        || !std::isfinite(localEligibilityTimeShiftMs)
        || localEligibilityTimeShiftMs < dtMs
        || localEligibilityTimeShiftMs > 1000.0) {
        throw std::invalid_argument(
            "Lokale Eligibility benötigt tau>0, Gain in [0,1] "
            "Maximum>0 und Zeitverschiebung zwischen dt und 1000 ms");
    }
    if (!std::isfinite(localEligibilityConstantFactor)
        || localEligibilityConstantFactor < 0.0
        || localEligibilityConstantFactor > 2.0
        || !std::all_of(
            localEligibilityRandomFactors.begin(),
            localEligibilityRandomFactors.end(),
            [](double value) {
                return std::isfinite(value)
                    && value >= 0.0
                    && value <= 2.0;
            })) {
        throw std::invalid_argument(
            "Eligibility-Kontrollfaktoren müssen in [0,2] liegen");
    }
    if (
        localEligibilityTransform
            == LocalEligibilityTransform::RandomMatched
        && localEligibilityRandomFactors.empty()) {
        throw std::invalid_argument(
            "Verteilungsgematchte Eligibility benötigt Faktoren");
    }
    if (!std::isfinite(projectionEiWeight)
        || !std::isfinite(projectionSlopeWeight)
        || !std::isfinite(projectionOvershootWeight)
        || !std::isfinite(projectionIsiWeight)) {
        throw std::invalid_argument("Projektionsgewichte müssen endlich sein");
    }
    if (!std::isfinite(membraneSlopeScaleMvPerMs)
        || membraneSlopeScaleMvPerMs <= 0.0
        || !std::isfinite(thresholdOvershootScaleMv)
        || thresholdOvershootScaleMv <= 0.0
        || !std::isfinite(isiTauMs)
        || isiTauMs <= 0.0) {
        throw std::invalid_argument("Feature-Skalen müssen positiv sein");
    }
    if (!std::isfinite(minimumAxonDelayMs)
        || !std::isfinite(maximumAxonDelayMs)
        || minimumAxonDelayMs < dtMs
        || maximumAxonDelayMs < minimumAxonDelayMs
        || maximumAxonDelayMs > 1000.0) {
        throw std::invalid_argument(
            "Axonverzögerung muss zwischen dt und 1000 ms liegen");
    }
    if (!std::isfinite(ampaReversalMv)
        || !std::isfinite(gabaReversalMv)
        || !std::isfinite(excitatoryConductanceScale)
        || excitatoryConductanceScale < 0.0
        || !std::isfinite(inhibitoryConductanceScale)
        || inhibitoryConductanceScale < 0.0) {
        throw std::invalid_argument("Ungültige Leitwertparameter");
    }
    if (!std::isfinite(tauDendriteMs)
        || tauDendriteMs <= 0.0
        || !std::isfinite(somaDendriteCoupling)
        || somaDendriteCoupling < 0.0
        || !std::isfinite(externalToDendriteFraction)
        || externalToDendriteFraction < 0.0
        || externalToDendriteFraction > 1.0) {
        throw std::invalid_argument("Ungültige Dendritenparameter");
    }
}

SpikingNetwork::SpikingNetwork(NetworkConfig config)
    : config_(std::move(config)), rng_(config_.seed) {
    config_.validate();
    excitatoryCount_ = std::clamp(
        static_cast<int>(std::lround(
            config_.neuronCount * config_.excitatoryFraction)),
        1,
        config_.neuronCount);
    createWeights();
    resetState();
}

void SpikingNetwork::createWeights() {
    const int count = config_.neuronCount;
    weights_.assign(static_cast<std::size_t>(count * count), 0.0);
    delaySteps_.assign(static_cast<std::size_t>(count * count), 1);
    const int minimumDelay = std::max(
        1,
        static_cast<int>(std::lround(
            config_.minimumAxonDelayMs / config_.dtMs)));
    const int maximumDelay = std::max(
        minimumDelay,
        static_cast<int>(std::lround(
            config_.maximumAxonDelayMs / config_.dtMs)));
    maximumDelaySteps_ = maximumDelay;
    std::uniform_int_distribution<int> delayDistribution(
        minimumDelay, maximumDelay);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> scale(0.75, 1.25);
    for (int post = 0; post < count; ++post) {
        for (int pre = 0; pre < count; ++pre) {
            if (pre == post || unit(rng_) > config_.connectionProbability) {
                continue;
            }
            const double magnitude =
                pre < excitatoryCount_
                    ? config_.excitatoryWeight * scale(rng_)
                    : -config_.inhibitoryWeight * scale(rng_);
            weights_[static_cast<std::size_t>(post * count + pre)] = magnitude;
            delaySteps_[static_cast<std::size_t>(post * count + pre)] =
                minimumDelay == maximumDelay
                    ? minimumDelay
                    : delayDistribution(rng_);
        }
    }
    shuffledEligibilitySource_.assign(weights_.size(), 0);
    std::vector<std::size_t> existingConnections;
    for (std::size_t index = 0; index < weights_.size(); ++index) {
        if (weights_[index] != 0.0) {
            existingConnections.push_back(index);
        }
    }
    for (std::size_t index = 0; index < weights_.size(); ++index) {
        shuffledEligibilitySource_[index] = index;
    }
    if (existingConnections.size() > 1) {
        for (std::size_t index = 0;
             index < existingConnections.size();
             ++index) {
            shuffledEligibilitySource_[existingConnections[index]] =
                existingConnections[
                    (index + 1) % existingConnections.size()];
        }
    }
}

void SpikingNetwork::resetState() {
    const std::size_t count = static_cast<std::size_t>(config_.neuronCount);
    voltage_.assign(count, config_.restingMv);
    dendriticVoltage_.assign(count, config_.restingMv);
    synapticState_.assign(count, 0.0);
    excitatorySynapticState_.assign(count, 0.0);
    inhibitorySynapticState_.assign(count, 0.0);
    refractorySteps_.assign(count, 0);
    previousSpikes_.assign(count, 0);
    preTrace_.assign(count, 0.0);
    postTrace_.assign(count, 0.0);
    localEligibility_.assign(count * count, 0.0);
    localEligibilityHistory_.clear();
    adaptiveThreshold_.assign(count, 0.0);
    previousEmissionGates_.assign(count, 1.0);
    delayedTransmissionGates_.assign(count, 1.0);
    lastSpikeSteps_.assign(count, -1);
    spikeHistory_.clear();
    emissionGateHistory_.clear();
    eventFeatureHistory_.clear();
    events_.clear();
    stepIndex_ = 0;
}

double SpikingNetwork::gateForInput(double state) {
    return gateForInput(state, config_.gateMode);
}

double SpikingNetwork::gateForInput(double state, GateMode mode) {
    std::uniform_real_distribution<double> randomOffset(
        -config_.randomGateAmplitude,
        config_.randomGateAmplitude);
    switch (mode) {
        case GateMode::Kernel:
            return clampGate(
                0.5 * (1.0 + std::tanh(generatedKernel(
                    state * config_.gateInputScale))));
        case GateMode::Constant:
            return config_.constantGate;
        case GateMode::Disabled:
            return 1.0;
        case GateMode::Sign:
            return state >= 0.0 ? 0.9 : 0.1;
        case GateMode::Tanh:
            return clampGate(
                0.5 * (1.0 + std::tanh(4.0 * state)));
        case GateMode::Random:
            if (!config_.randomGateValues.empty()) {
                std::uniform_int_distribution<std::size_t> choice(
                    0, config_.randomGateValues.size() - 1);
                return config_.randomGateValues[choice(rng_)];
            }
            return clampGate(
                config_.constantGate + randomOffset(rng_));
    }
    return 1.0;
}

GateMode SpikingNetwork::gateModeForConnection(int pre, int post) const {
    if (!config_.classSpecificOperatorsEnabled) {
        return config_.gateMode;
    }
    const bool preExcitatory = pre < excitatoryCount_;
    const bool postExcitatory = post < excitatoryCount_;
    if (preExcitatory && postExcitatory) {
        return config_.eeGateMode;
    }
    if (preExcitatory && !postExcitatory) {
        return config_.eiGateMode;
    }
    if (!preExcitatory && postExcitatory) {
        return config_.ieGateMode;
    }
    return config_.iiGateMode;
}

std::vector<double> SpikingNetwork::computeGates() {
    const int count = config_.neuronCount;
    std::vector<double> gates(static_cast<std::size_t>(count), 1.0);
    const double voltageScale = config_.thresholdMv - config_.restingMv;
    for (int neuron = 0; neuron < count; ++neuron) {
        const double state =
            (voltage_[static_cast<std::size_t>(neuron)] - config_.restingMv)
            / voltageScale;
        gates[static_cast<std::size_t>(neuron)] = gateForInput(state);
    }
    return gates;
}

void SpikingNetwork::applyStdp(const std::vector<std::uint8_t>& spikes) {
    const int count = config_.neuronCount;
    const double decay = std::exp(-config_.dtMs / config_.stdpTauMs);
    for (int neuron = 0; neuron < count; ++neuron) {
        preTrace_[static_cast<std::size_t>(neuron)] *= decay;
        postTrace_[static_cast<std::size_t>(neuron)] *= decay;
    }
    if (config_.plasticityEnabled) {
        for (int post = 0; post < count; ++post) {
            for (int pre = 0; pre < count; ++pre) {
                double& weight =
                    weights_[static_cast<std::size_t>(post * count + pre)];
                if (weight == 0.0) {
                    continue;
                }
                const double delta = config_.stdpLearningRate * (
                    config_.stdpPotentiation
                        * spikes[static_cast<std::size_t>(post)]
                        * preTrace_[static_cast<std::size_t>(pre)]
                    - config_.stdpDepression
                        * spikes[static_cast<std::size_t>(pre)]
                        * postTrace_[static_cast<std::size_t>(post)]);
                const double sign = pre < excitatoryCount_ ? 1.0 : -1.0;
                const double magnitude = std::clamp(
                    std::abs(weight) + delta,
                    0.0,
                    config_.maximumWeight);
                weight = sign * magnitude;
            }
        }
    }
    for (int neuron = 0; neuron < count; ++neuron) {
        preTrace_[static_cast<std::size_t>(neuron)] +=
            spikes[static_cast<std::size_t>(neuron)];
        postTrace_[static_cast<std::size_t>(neuron)] +=
            spikes[static_cast<std::size_t>(neuron)];
    }
}

void SpikingNetwork::updateLocalEligibility(
    const std::vector<std::uint8_t>& spikes) {
    if (!config_.localEligibilityEnabled) {
        return;
    }
    const int count = config_.neuronCount;
    const double decay =
        std::exp(-config_.dtMs / config_.localEligibilityTauMs);
    for (int post = 0; post < count; ++post) {
        for (int pre = 0; pre < count; ++pre) {
            const std::size_t connection =
                static_cast<std::size_t>(post * count + pre);
            if (weights_[connection] == 0.0) {
                localEligibility_[connection] = 0.0;
                continue;
            }
            if (!localEligibilityApplies(pre, post)) {
                localEligibility_[connection] = 0.0;
                continue;
            }
            const double causal =
                spikes[static_cast<std::size_t>(post)]
                * preTrace_[static_cast<std::size_t>(pre)];
            const double antiCausal =
                spikes[static_cast<std::size_t>(pre)]
                * postTrace_[static_cast<std::size_t>(post)];
            localEligibility_[connection] = std::clamp(
                localEligibility_[connection] * decay
                    + causal - antiCausal,
                -config_.localEligibilityMaximum,
                config_.localEligibilityMaximum);
        }
    }
    localEligibilityHistory_.push_back(localEligibility_);
    const std::size_t shiftSteps = static_cast<std::size_t>(
        std::max(
            1,
            static_cast<int>(std::lround(
                config_.localEligibilityTimeShiftMs / config_.dtMs))));
    const std::size_t historyLimit = shiftSteps + 2;
    if (localEligibilityHistory_.size() > historyLimit) {
        localEligibilityHistory_.erase(
            localEligibilityHistory_.begin());
    }
}

bool SpikingNetwork::localEligibilityApplies(int pre, int post) const {
    const bool preExcitatory = pre < excitatoryCount_;
    const bool postExcitatory = post < excitatoryCount_;
    switch (config_.localEligibilityScope) {
        case LocalEligibilityScope::All:
            return true;
        case LocalEligibilityScope::ExcitatoryToExcitatory:
            return preExcitatory && postExcitatory;
        case LocalEligibilityScope::InhibitoryToExcitatory:
            return !preExcitatory && postExcitatory;
    }
    return true;
}

double SpikingNetwork::localEligibilityFactor(
    std::size_t connection,
    int pre,
    int post) {
    if (!config_.localEligibilityEnabled
        || !localEligibilityApplies(pre, post)) {
        return 1.0;
    }
    if (
        config_.localEligibilityTransform
        == LocalEligibilityTransform::ConstantMatched) {
        return config_.localEligibilityConstantFactor;
    }
    if (
        config_.localEligibilityTransform
        == LocalEligibilityTransform::RandomMatched
        && !config_.localEligibilityRandomFactors.empty()) {
        std::uniform_int_distribution<std::size_t> choice(
            0, config_.localEligibilityRandomFactors.size() - 1);
        return config_.localEligibilityRandomFactors[choice(rng_)];
    }
    double trace = localEligibility_[connection];
    switch (config_.localEligibilityTransform) {
        case LocalEligibilityTransform::Signed:
            break;
        case LocalEligibilityTransform::Absolute:
            trace = std::abs(trace);
            break;
        case LocalEligibilityTransform::TimeShifted:
            {
                const std::size_t shiftSteps =
                    static_cast<std::size_t>(std::max(
                        1,
                        static_cast<int>(std::lround(
                            config_.localEligibilityTimeShiftMs
                            / config_.dtMs))));
                trace =
                    localEligibilityHistory_.size() > shiftSteps
                        ? localEligibilityHistory_[
                            localEligibilityHistory_.size()
                                - 1 - shiftSteps][connection]
                        : 0.0;
            }
            break;
        case LocalEligibilityTransform::SynapseShuffled:
            trace = localEligibility_[
                shuffledEligibilitySource_[connection]];
            break;
        case LocalEligibilityTransform::Inverted:
            trace = -trace;
            break;
        case LocalEligibilityTransform::ConstantMatched:
        case LocalEligibilityTransform::RandomMatched:
            break;
    }
    return std::clamp(
        1.0 + config_.localEligibilityGain * std::tanh(trace),
        1.0 - config_.localEligibilityGain,
        1.0 + config_.localEligibilityGain);
}

SimulationResult SpikingNetwork::run(
    const std::vector<std::vector<double>>& stimulus) {
    resetState();
    SimulationResult result;
    const int count = config_.neuronCount;
    result.voltagesMv.reserve(stimulus.size());
    result.spikes.reserve(stimulus.size());
    result.gates.reserve(stimulus.size());
    result.computedGates.reserve(stimulus.size());
    result.dendriticVoltagesMv.reserve(stimulus.size());
    const double synapseDecay =
        std::exp(-config_.dtMs / config_.tauSynapseMs);
    const double adaptationDecay =
        std::exp(-config_.dtMs / config_.adaptiveThresholdTauMs);
    const int refractoryDuration = std::max(
        1, static_cast<int>(std::ceil(config_.refractoryMs / config_.dtMs)));
    const double membraneFactor = config_.dtMs / config_.tauMembraneMs;

    for (const auto& drive : stimulus) {
        if (drive.size() != static_cast<std::size_t>(count)) {
            throw std::invalid_argument("Stimulusbreite stimmt nicht mit Neuronenzahl überein");
        }
        if (!std::all_of(drive.begin(), drive.end(), [](double value) {
                return std::isfinite(value);
            })) {
            throw std::invalid_argument("Stimulus enthält nichtendliche Werte");
        }
        const std::vector<double> computedGates = computeGates();
        std::vector<double> baseGates =
            config_.gateTiming == GateTiming::EmissionState
                ? previousEmissionGates_
                : computedGates;
        std::vector<double> gates = baseGates;
        if (config_.gatePerturbation == GatePerturbation::TimeShifted) {
            gates = delayedTransmissionGates_;
            delayedTransmissionGates_ = baseGates;
        } else if (
            config_.gatePerturbation == GatePerturbation::StateShuffled
            && gates.size() > 1) {
            std::rotate(gates.begin(), gates.begin() + 1, gates.end());
        }
        for (int post = 0; post < count; ++post) {
            double incomingExc = 0.0;
            double incomingInh = 0.0;
            for (int pre = 0; pre < count; ++pre) {
                const std::size_t connection =
                    static_cast<std::size_t>(post * count + pre);
                const double weight = weights_[connection];
                if (weight == 0.0) {
                    continue;
                }
                const int delay = delaySteps_[connection];
                if (spikeHistory_.size()
                    < static_cast<std::size_t>(delay)) {
                    continue;
                }
                const auto& delayedSpikes =
                    spikeHistory_[spikeHistory_.size() - delay];
                if (!delayedSpikes[static_cast<std::size_t>(pre)]) {
                    continue;
                }

                const int gateSource =
                    config_.gatePerturbation
                            == GatePerturbation::StateShuffled
                        ? (pre + 1) % count
                        : pre;
                double transmissionGate = 1.0;
                if (config_.gateTiming == GateTiming::EmissionState) {
                    const int gateDelay =
                        delay
                        + (
                            config_.gatePerturbation
                                    == GatePerturbation::TimeShifted
                                ? 1
                                : 0);
                    if (emissionGateHistory_.size()
                        >= static_cast<std::size_t>(gateDelay)) {
                        const std::size_t historyIndex =
                            emissionGateHistory_.size() - gateDelay;
                        if (config_.classSpecificOperatorsEnabled) {
                            const double feature =
                                eventFeatureHistory_[historyIndex]
                                    [static_cast<std::size_t>(gateSource)];
                            transmissionGate = gateForInput(
                                feature,
                                gateModeForConnection(pre, post));
                        } else {
                            transmissionGate =
                                emissionGateHistory_[historyIndex]
                                    [static_cast<std::size_t>(gateSource)];
                        }
                    }
                } else {
                    if (config_.classSpecificOperatorsEnabled) {
                        const double voltageScale =
                            config_.thresholdMv - config_.restingMv;
                        const double state =
                            (voltage_[static_cast<std::size_t>(gateSource)]
                                - config_.restingMv)
                            / voltageScale;
                        transmissionGate = gateForInput(
                            state,
                            gateModeForConnection(pre, post));
                    } else {
                        transmissionGate =
                            gates[static_cast<std::size_t>(pre)];
                    }
                }
                result.effectiveTransmissionGates.push_back(
                    transmissionGate);
                const double eligibilityFactor =
                    localEligibilityFactor(connection, pre, post);
                result.eligibilityTransmissionFactors.push_back(
                    eligibilityFactor);
                const double effectiveTransmission =
                    transmissionGate * eligibilityFactor;
                const double magnitude = std::abs(weight);
                if (weight > 0.0) {
                    incomingExc +=
                        magnitude
                        * (
                            config_.synapseModel
                                    == SynapseModel::ConductanceBased
                                ? config_.excitatoryConductanceScale
                                : 1.0)
                        * effectiveTransmission;
                } else {
                    incomingInh +=
                        magnitude
                        * (
                            config_.synapseModel
                                    == SynapseModel::ConductanceBased
                                ? config_.inhibitoryConductanceScale
                                : 1.0)
                        * effectiveTransmission;
                }
            }
            const std::size_t index = static_cast<std::size_t>(post);
            excitatorySynapticState_[index] =
                excitatorySynapticState_[index] * synapseDecay + incomingExc;
            inhibitorySynapticState_[index] =
                inhibitorySynapticState_[index] * synapseDecay + incomingInh;
            const double targetVoltage =
                config_.dendriteEnabled
                    ? dendriticVoltage_[index]
                    : voltage_[index];
            synapticState_[index] =
                config_.synapseModel == SynapseModel::ConductanceBased
                    ? excitatorySynapticState_[index]
                            * (config_.ampaReversalMv - targetVoltage)
                        + inhibitorySynapticState_[index]
                            * (config_.gabaReversalMv - targetVoltage)
                    : excitatorySynapticState_[index]
                        - inhibitorySynapticState_[index];
        }

        std::vector<std::uint8_t> spikes(
            static_cast<std::size_t>(count), 0);
        std::vector<double> nextEmissionGates = computedGates;
        std::vector<double> currentEventFeatures(
            static_cast<std::size_t>(count), 0.0);
        for (int neuron = 0; neuron < count; ++neuron) {
            const std::size_t index = static_cast<std::size_t>(neuron);
            adaptiveThreshold_[index] *= adaptationDecay;
            const double previousDendriticVoltage =
                dendriticVoltage_[index];
            const double previousSomaticVoltage = voltage_[index];
            if (config_.dendriteEnabled) {
                const double dendriticDrive =
                    drive[index] * config_.externalToDendriteFraction;
                const double dendriticFactor =
                    config_.dtMs / config_.tauDendriteMs;
                dendriticVoltage_[index] += dendriticFactor * (
                    config_.restingMv - dendriticVoltage_[index]
                    + dendriticDrive
                    + synapticState_[index]
                    + config_.somaDendriteCoupling
                        * (previousSomaticVoltage
                            - previousDendriticVoltage));
            } else {
                dendriticVoltage_[index] = voltage_[index];
            }
            if (refractorySteps_[index] > 0) {
                --refractorySteps_[index];
                voltage_[index] = config_.resetMv;
                continue;
            }
            const double previousVoltage = voltage_[index];
            const double somaticDrive =
                config_.dendriteEnabled
                    ? drive[index]
                        * (1.0 - config_.externalToDendriteFraction)
                        + config_.somaDendriteCoupling
                            * (dendriticVoltage_[index]
                                - previousSomaticVoltage)
                    : drive[index] + synapticState_[index];
            const double dv = membraneFactor * (
                config_.restingMv - voltage_[index]
                + somaticDrive);
            voltage_[index] += dv;
            const double threshold =
                config_.thresholdMv + adaptiveThreshold_[index];
            if (voltage_[index] >= threshold) {
                spikes[index] = 1;
                const double excitation = excitatorySynapticState_[index];
                const double inhibition = inhibitorySynapticState_[index];
                const double eiBalance =
                    (excitation - inhibition)
                    / (std::abs(excitation) + std::abs(inhibition) + 1e-9);
                const double membraneSlope = std::tanh(
                    ((voltage_[index] - previousVoltage) / config_.dtMs)
                    / config_.membraneSlopeScaleMvPerMs);
                const double thresholdOvershoot = std::tanh(
                    (voltage_[index] - threshold)
                    / config_.thresholdOvershootScaleMv);
                const double isiState =
                    lastSpikeSteps_[index] < 0
                        ? -1.0
                        : 2.0 * std::exp(
                            -(
                                (static_cast<double>(stepIndex_)
                                    - lastSpikeSteps_[index])
                                * config_.dtMs)
                            / config_.isiTauMs)
                            - 1.0;
                double eventFeature = 0.0;
                if (config_.emissionFeature == EmissionFeature::EiBalance) {
                    eventFeature = eiBalance;
                } else if (
                    config_.emissionFeature
                    == EmissionFeature::FeatureProjection) {
                    eventFeature =
                        config_.projectionEiWeight * eiBalance
                        + config_.projectionSlopeWeight * membraneSlope
                        + config_.projectionOvershootWeight
                            * thresholdOvershoot
                        + config_.projectionIsiWeight * isiState;
                } else {
                    eventFeature =
                        (voltage_[index] - config_.restingMv)
                        / (config_.thresholdMv - config_.restingMv);
                }
                nextEmissionGates[index] = gateForInput(eventFeature);
                currentEventFeatures[index] = eventFeature;
                events_.push_back(SpikeEvent{
                    static_cast<std::uint32_t>(neuron),
                    stepIndex_,
                    1.0,
                    nextEmissionGates[index],
                    eventFeature,
                    eiBalance,
                    membraneSlope,
                    thresholdOvershoot,
                    isiState});
                lastSpikeSteps_[index] = static_cast<int>(stepIndex_);
                voltage_[index] = config_.resetMv;
                refractorySteps_[index] = refractoryDuration;
                adaptiveThreshold_[index] +=
                    config_.adaptiveThresholdIncrementMv;
            }
        }
        updateLocalEligibility(spikes);
        applyStdp(spikes);
        previousSpikes_ = spikes;
        previousEmissionGates_ = nextEmissionGates;
        spikeHistory_.push_back(spikes);
        emissionGateHistory_.push_back(nextEmissionGates);
        eventFeatureHistory_.push_back(currentEventFeatures);
        const std::size_t historyLimit =
            static_cast<std::size_t>(maximumDelaySteps_ + 2);
        if (spikeHistory_.size() > historyLimit) {
            spikeHistory_.erase(spikeHistory_.begin());
            emissionGateHistory_.erase(emissionGateHistory_.begin());
            eventFeatureHistory_.erase(eventFeatureHistory_.begin());
        }
        result.voltagesMv.push_back(voltage_);
        result.spikes.push_back(std::move(spikes));
        result.gates.push_back(gates);
        result.computedGates.push_back(computedGates);
        result.dendriticVoltagesMv.push_back(dendriticVoltage_);
        ++stepIndex_;
    }
    result.finalWeights = weights_;
    result.finalLocalEligibility = localEligibility_;
    result.events = events_;

    SimulationMetrics metrics;
    metrics.steps = static_cast<int>(result.spikes.size());
    metrics.neuronCount = count;
    std::vector<int> populationCounts;
    populationCounts.reserve(result.spikes.size());
    std::vector<bool> active(static_cast<std::size_t>(count), false);
    double voltageEnergy = 0.0;
    double gateSum = 0.0;
    double gateSquareSum = 0.0;
    double computedGateSum = 0.0;
    double computedGateSquareSum = 0.0;
    const std::vector<double>& effectiveGates =
        result.effectiveTransmissionGates;
    const std::vector<double>& eligibilityFactors =
        result.eligibilityTransmissionFactors;
    double dendriticVoltageEnergy = 0.0;
    std::size_t valueCount = 0;
    const double voltageScale = config_.thresholdMv - config_.restingMv;
    for (std::size_t step = 0; step < result.spikes.size(); ++step) {
        int population = 0;
        for (int neuron = 0; neuron < count; ++neuron) {
            const std::size_t index = static_cast<std::size_t>(neuron);
            population += result.spikes[step][index];
            active[index] = active[index] || result.spikes[step][index] != 0;
            const double normalized =
                (result.voltagesMv[step][index] - config_.restingMv)
                / voltageScale;
            voltageEnergy += normalized * normalized;
            const double normalizedDendritic =
                (result.dendriticVoltagesMv[step][index]
                    - config_.restingMv)
                / voltageScale;
            dendriticVoltageEnergy +=
                normalizedDendritic * normalizedDendritic;
            const double gate = result.gates[step][index];
            gateSum += gate;
            gateSquareSum += gate * gate;
            const double computedGate = result.computedGates[step][index];
            computedGateSum += computedGate;
            computedGateSquareSum += computedGate * computedGate;
            metrics.finite = metrics.finite
                && std::isfinite(result.voltagesMv[step][index])
                && std::isfinite(
                    result.dendriticVoltagesMv[step][index])
                && std::isfinite(gate);
            ++valueCount;
        }
        populationCounts.push_back(population);
        metrics.totalSpikes += population;
    }
    const double durationSeconds =
        std::max(config_.dtMs * metrics.steps / 1000.0, 1e-12);
    metrics.meanFiringRateHz =
        metrics.totalSpikes / static_cast<double>(count) / durationSeconds;
    metrics.normalizedVoltageEnergy =
        valueCount ? voltageEnergy / valueCount : 0.0;
    metrics.dendriticVoltageEnergy =
        valueCount ? dendriticVoltageEnergy / valueCount : 0.0;
    double delaySum = 0.0;
    int delayCount = 0;
    for (std::size_t index = 0; index < weights_.size(); ++index) {
        if (weights_[index] != 0.0) {
            delaySum += delaySteps_[index] * config_.dtMs;
            ++delayCount;
        }
    }
    metrics.meanAxonDelayMs =
        delayCount > 0 ? delaySum / delayCount : 0.0;
    metrics.synapticTransmissionCount =
        static_cast<int>(effectiveGates.size());
    if (!eligibilityFactors.empty()) {
        metrics.meanEligibilityTransmissionFactor =
            std::accumulate(
                eligibilityFactors.begin(),
                eligibilityFactors.end(),
                0.0)
            / eligibilityFactors.size();
        for (double value : eligibilityFactors) {
            const double centered =
                value - metrics.meanEligibilityTransmissionFactor;
            metrics.eligibilityTransmissionFactorVariance +=
                centered * centered;
            metrics.finite =
                metrics.finite && std::isfinite(value);
        }
        metrics.eligibilityTransmissionFactorVariance /=
            eligibilityFactors.size();
    }
    double eligibilitySquareSum = 0.0;
    for (std::size_t index = 0; index < weights_.size(); ++index) {
        if (weights_[index] == 0.0) {
            continue;
        }
        const double value = localEligibility_[index];
        metrics.meanLocalEligibility += value;
        eligibilitySquareSum += value * value;
        metrics.maximumAbsoluteLocalEligibility = std::max(
            metrics.maximumAbsoluteLocalEligibility,
            std::abs(value));
        metrics.finite = metrics.finite && std::isfinite(value);
        ++metrics.localEligibilitySynapseCount;
    }
    if (metrics.localEligibilitySynapseCount > 0) {
        metrics.meanLocalEligibility /=
            metrics.localEligibilitySynapseCount;
        metrics.localEligibilityVariance = std::max(
            0.0,
            eligibilitySquareSum
                    / metrics.localEligibilitySynapseCount
                - metrics.meanLocalEligibility
                    * metrics.meanLocalEligibility);
    }
    metrics.meanGate = valueCount ? gateSum / valueCount : 1.0;
    metrics.gateVariance = valueCount
        ? std::max(0.0, gateSquareSum / valueCount
            - metrics.meanGate * metrics.meanGate)
        : 0.0;
    const double meanPopulation = populationCounts.empty()
        ? 0.0
        : std::accumulate(
            populationCounts.begin(), populationCounts.end(), 0.0)
            / populationCounts.size();
    double populationVariance = 0.0;
    for (int value : populationCounts) {
        populationVariance +=
            (value - meanPopulation) * (value - meanPopulation);
    }
    if (!populationCounts.empty()) {
        populationVariance /= populationCounts.size();
    }
    metrics.populationSpikeCountFano =
        populationVariance / (meanPopulation + 1e-9);
    int coincidentSpikes = 0;
    for (int value : populationCounts) {
        if (value > 1) {
            coincidentSpikes += value;
        }
    }
    metrics.binnedCoincidenceRate = metrics.totalSpikes > 0
        ? coincidentSpikes / static_cast<double>(metrics.totalSpikes)
        : 0.0;
    double correlationSum = 0.0;
    for (int left = 0; left < count; ++left) {
        double leftMean = 0.0;
        for (const auto& row : result.spikes) {
            leftMean += row[static_cast<std::size_t>(left)];
        }
        leftMean /= std::max<std::size_t>(1, result.spikes.size());
        double leftVariance = 0.0;
        for (const auto& row : result.spikes) {
            const double centered =
                row[static_cast<std::size_t>(left)] - leftMean;
            leftVariance += centered * centered;
        }
        if (leftVariance <= 0.0) {
            continue;
        }
        for (int right = left + 1; right < count; ++right) {
            double rightMean = 0.0;
            for (const auto& row : result.spikes) {
                rightMean += row[static_cast<std::size_t>(right)];
            }
            rightMean /= std::max<std::size_t>(1, result.spikes.size());
            double rightVariance = 0.0;
            double covariance = 0.0;
            for (const auto& row : result.spikes) {
                const double leftCentered =
                    row[static_cast<std::size_t>(left)] - leftMean;
                const double rightCentered =
                    row[static_cast<std::size_t>(right)] - rightMean;
                rightVariance += rightCentered * rightCentered;
                covariance += leftCentered * rightCentered;
            }
            if (rightVariance <= 0.0) {
                continue;
            }
            correlationSum +=
                covariance / std::sqrt(leftVariance * rightVariance);
            ++metrics.pairwiseCorrelationPairs;
        }
    }
    metrics.meanPairwiseSpikeCorrelation =
        metrics.pairwiseCorrelationPairs > 0
            ? correlationSum / metrics.pairwiseCorrelationPairs
            : 0.0;
    metrics.globallyComputedGateMean =
        valueCount ? computedGateSum / valueCount : 1.0;
    metrics.globallyComputedGateVariance = valueCount
        ? std::max(
            0.0,
            computedGateSquareSum / valueCount
                - metrics.globallyComputedGateMean
                    * metrics.globallyComputedGateMean)
        : 0.0;
    metrics.effectiveGateCount =
        static_cast<int>(effectiveGates.size());
    if (!effectiveGates.empty()) {
        metrics.effectiveGateMean =
            std::accumulate(effectiveGates.begin(), effectiveGates.end(), 0.0)
            / effectiveGates.size();
        double variance = 0.0;
        for (double value : effectiveGates) {
            variance +=
                (value - metrics.effectiveGateMean)
                * (value - metrics.effectiveGateMean);
        }
        metrics.effectiveGateVariance =
            variance / effectiveGates.size();
        const auto bounds = std::minmax_element(
            effectiveGates.begin(), effectiveGates.end());
        metrics.effectiveGateMinimum = *bounds.first;
        metrics.effectiveGateMaximum = *bounds.second;
        if (metrics.effectiveGateVariance > 1e-15
            && metrics.effectiveGateMaximum
                - metrics.effectiveGateMinimum > 1e-12) {
            const int bins = std::clamp(
                static_cast<int>(std::lround(
                    std::sqrt(static_cast<double>(effectiveGates.size())))),
                2,
                16);
            std::vector<int> counts(static_cast<std::size_t>(bins), 0);
            const double width =
                (metrics.effectiveGateMaximum
                    - metrics.effectiveGateMinimum) / bins;
            for (double value : effectiveGates) {
                const int bin = std::min(
                    bins - 1,
                    static_cast<int>(
                        (value - metrics.effectiveGateMinimum) / width));
                counts[static_cast<std::size_t>(bin)]++;
            }
            for (int countInBin : counts) {
                if (countInBin == 0) {
                    continue;
                }
                const double probability =
                    countInBin / static_cast<double>(effectiveGates.size());
                metrics.effectiveGateEntropyBits -=
                    probability * std::log2(probability);
            }
        }
    }
    metrics.spikeEventCount = static_cast<int>(result.events.size());
    if (!result.events.empty()) {
        for (const auto& event : result.events) {
            metrics.eventFeatureMean += event.featureValue;
        }
        metrics.eventFeatureMean /= result.events.size();
        metrics.eventFeatureMinimum = result.events.front().featureValue;
        metrics.eventFeatureMaximum = result.events.front().featureValue;
        for (const auto& event : result.events) {
            const double centered =
                event.featureValue - metrics.eventFeatureMean;
            metrics.eventFeatureVariance += centered * centered;
            metrics.eventFeatureMinimum =
                std::min(metrics.eventFeatureMinimum, event.featureValue);
            metrics.eventFeatureMaximum =
                std::max(metrics.eventFeatureMaximum, event.featureValue);
        }
        metrics.eventFeatureVariance /= result.events.size();

        auto componentStatistics = [&result](
            double SpikeEvent::*member,
            double& componentMean,
            double& componentVariance) {
            for (const auto& event : result.events) {
                componentMean += event.*member;
            }
            componentMean /= result.events.size();
            for (const auto& event : result.events) {
                const double centered = event.*member - componentMean;
                componentVariance += centered * centered;
            }
            componentVariance /= result.events.size();
        };
        componentStatistics(
            &SpikeEvent::eiBalance,
            metrics.eventEiBalanceMean,
            metrics.eventEiBalanceVariance);
        componentStatistics(
            &SpikeEvent::membraneSlope,
            metrics.eventMembraneSlopeMean,
            metrics.eventMembraneSlopeVariance);
        componentStatistics(
            &SpikeEvent::thresholdOvershoot,
            metrics.eventThresholdOvershootMean,
            metrics.eventThresholdOvershootVariance);
        componentStatistics(
            &SpikeEvent::isiState,
            metrics.eventIsiStateMean,
            metrics.eventIsiStateVariance);
    }
    metrics.activeFraction = std::count(active.begin(), active.end(), true)
        / static_cast<double>(count);
    result.metrics = metrics;
    return result;
}

bool SpikingNetwork::dalePrincipleHolds() const {
    const int count = config_.neuronCount;
    for (int post = 0; post < count; ++post) {
        for (int pre = 0; pre < count; ++pre) {
            const double weight =
                weights_[static_cast<std::size_t>(post * count + pre)];
            if (pre < excitatoryCount_ && weight < 0.0) {
                return false;
            }
            if (pre >= excitatoryCount_ && weight > 0.0) {
                return false;
            }
        }
    }
    return true;
}

std::vector<std::vector<double>> makeTemporalStimulus(
    int neuronCount,
    int steps,
    int timeBins,
    int label,
    int sampleIndex,
    std::uint64_t seed,
    double baselineCurrent,
    double pulseCurrent,
    double noiseStd) {
    if (neuronCount < 2 || steps < timeBins || timeBins < 2) {
        throw std::invalid_argument("Ungültige Stimulusdimension");
    }
    if (label != 0 && label != 1) {
        throw std::invalid_argument("Musterklasse muss 0 oder 1 sein");
    }
    const std::uint64_t derivedSeed =
        seed
        ^ static_cast<std::uint64_t>(label + 1) * 0x9E3779B1ULL
        ^ static_cast<std::uint64_t>(sampleIndex + 1) * 0x85EBCA77ULL;
    std::mt19937_64 rng(derivedSeed);
    std::normal_distribution<double> noise(0.0, noiseStd);
    const int split = neuronCount / 2;
    std::vector<std::vector<double>> stimulus(
        static_cast<std::size_t>(steps),
        std::vector<double>(static_cast<std::size_t>(neuronCount), 0.0));
    for (int step = 0; step < steps; ++step) {
        const int timeBin = std::min(
            timeBins - 1, step * timeBins / steps);
        const int assembly = (timeBin + label) % 2;
        for (int neuron = 0; neuron < neuronCount; ++neuron) {
            const bool first = neuron < split;
            const bool active = first == (assembly == 0);
            stimulus[static_cast<std::size_t>(step)]
                    [static_cast<std::size_t>(neuron)] =
                baselineCurrent + (active ? pulseCurrent : 0.0) + noise(rng);
        }
    }
    return stimulus;
}

}  // namespace agbnn
