#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace agbnn {

enum class GateMode {
    Kernel,
    Constant,
    Disabled,
    Sign,
    Tanh,
    Random
};

const wchar_t* gateModeName(GateMode mode);

enum class GateTiming {
    ResetLocked,
    EmissionState
};

const wchar_t* gateTimingName(GateTiming timing);

enum class GatePerturbation {
    None,
    TimeShifted,
    StateShuffled
};

const wchar_t* gatePerturbationName(GatePerturbation perturbation);

enum class EmissionFeature {
    PreResetVoltage,
    EiBalance,
    FeatureProjection
};

const wchar_t* emissionFeatureName(EmissionFeature feature);

enum class SynapseModel {
    CurrentBased,
    ConductanceBased
};

const wchar_t* synapseModelName(SynapseModel model);

enum class LocalEligibilityTransform {
    Signed,
    Absolute,
    TimeShifted,
    SynapseShuffled,
    Inverted,
    ConstantMatched,
    RandomMatched
};

const wchar_t* localEligibilityTransformName(
    LocalEligibilityTransform transform);

enum class LocalEligibilityScope {
    All,
    ExcitatoryToExcitatory,
    InhibitoryToExcitatory
};

const wchar_t* localEligibilityScopeName(LocalEligibilityScope scope);

struct SpikeEvent {
    std::uint32_t sourceNeuron = 0;
    std::uint32_t emissionStep = 0;
    double amplitude = 1.0;
    double generatedGate = 1.0;
    double featureValue = 0.0;
    double eiBalance = 0.0;
    double membraneSlope = 0.0;
    double thresholdOvershoot = 0.0;
    double isiState = -1.0;
};

struct NetworkConfig {
    int neuronCount = 16;
    double excitatoryFraction = 0.8;
    double connectionProbability = 0.18;
    std::uint64_t seed = 38;
    double dtMs = 1.0;
    double tauMembraneMs = 20.0;
    double tauSynapseMs = 5.0;
    double restingMv = -65.0;
    double resetMv = -70.0;
    double thresholdMv = -50.0;
    double refractoryMs = 2.0;
    double excitatoryWeight = 13.0;
    double inhibitoryWeight = 17.0;
    double maximumWeight = 30.0;
    double adaptiveThresholdIncrementMv = 1.5;
    double adaptiveThresholdTauMs = 80.0;
    GateMode gateMode = GateMode::Kernel;
    GateTiming gateTiming = GateTiming::ResetLocked;
    GatePerturbation gatePerturbation = GatePerturbation::None;
    EmissionFeature emissionFeature = EmissionFeature::PreResetVoltage;
    double projectionEiWeight = 0.40;
    double projectionSlopeWeight = 0.25;
    double projectionOvershootWeight = 0.15;
    double projectionIsiWeight = 0.20;
    double membraneSlopeScaleMvPerMs = 1.0;
    double thresholdOvershootScaleMv = 1.0;
    double isiTauMs = 50.0;
    double minimumAxonDelayMs = 1.0;
    double maximumAxonDelayMs = 1.0;
    SynapseModel synapseModel = SynapseModel::CurrentBased;
    double ampaReversalMv = 0.0;
    double gabaReversalMv = -75.0;
    double excitatoryConductanceScale = 0.02;
    double inhibitoryConductanceScale = 0.02;
    bool dendriteEnabled = false;
    double tauDendriteMs = 30.0;
    double somaDendriteCoupling = 0.20;
    double externalToDendriteFraction = 0.0;
    bool classSpecificOperatorsEnabled = false;
    GateMode eeGateMode = GateMode::Kernel;
    GateMode eiGateMode = GateMode::Kernel;
    GateMode ieGateMode = GateMode::Kernel;
    GateMode iiGateMode = GateMode::Kernel;
    double gateInputScale = 1.0;
    double constantGate = 0.12831112128784755;
    double randomGateAmplitude = 0.35;
    std::vector<double> randomGateValues;
    bool plasticityEnabled = true;
    double stdpLearningRate = 0.01;
    double stdpTauMs = 20.0;
    double stdpPotentiation = 1.0;
    double stdpDepression = 1.05;
    bool localEligibilityEnabled = false;
    double localEligibilityTauMs = 100.0;
    double localEligibilityGain = 0.35;
    double localEligibilityMaximum = 4.0;
    double localEligibilityTimeShiftMs = 40.0;
    LocalEligibilityTransform localEligibilityTransform =
        LocalEligibilityTransform::Signed;
    LocalEligibilityScope localEligibilityScope =
        LocalEligibilityScope::All;
    double localEligibilityConstantFactor = 1.0;
    std::vector<double> localEligibilityRandomFactors;

    void validate() const;
};

struct SimulationMetrics {
    int steps = 0;
    int neuronCount = 0;
    int totalSpikes = 0;
    double meanFiringRateHz = 0.0;
    double normalizedVoltageEnergy = 0.0;
    double meanGate = 1.0;
    double gateVariance = 0.0;
    double populationSpikeCountFano = 0.0;
    double meanPairwiseSpikeCorrelation = 0.0;
    int pairwiseCorrelationPairs = 0;
    double binnedCoincidenceRate = 0.0;
    double activeFraction = 0.0;
    double globallyComputedGateMean = 1.0;
    double globallyComputedGateVariance = 0.0;
    int effectiveGateCount = 0;
    double effectiveGateMean = 0.0;
    double effectiveGateVariance = 0.0;
    double effectiveGateEntropyBits = 0.0;
    double effectiveGateMinimum = 0.0;
    double effectiveGateMaximum = 0.0;
    int spikeEventCount = 0;
    double eventFeatureMean = 0.0;
    double eventFeatureVariance = 0.0;
    double eventFeatureMinimum = 0.0;
    double eventFeatureMaximum = 0.0;
    double eventEiBalanceMean = 0.0;
    double eventEiBalanceVariance = 0.0;
    double eventMembraneSlopeMean = 0.0;
    double eventMembraneSlopeVariance = 0.0;
    double eventThresholdOvershootMean = 0.0;
    double eventThresholdOvershootVariance = 0.0;
    double eventIsiStateMean = 0.0;
    double eventIsiStateVariance = 0.0;
    double dendriticVoltageEnergy = 0.0;
    double meanAxonDelayMs = 1.0;
    int synapticTransmissionCount = 0;
    int localEligibilitySynapseCount = 0;
    double meanLocalEligibility = 0.0;
    double localEligibilityVariance = 0.0;
    double maximumAbsoluteLocalEligibility = 0.0;
    double meanEligibilityTransmissionFactor = 1.0;
    double eligibilityTransmissionFactorVariance = 0.0;
    bool finite = true;
};

struct SimulationResult {
    std::vector<std::vector<double>> voltagesMv;
    std::vector<std::vector<std::uint8_t>> spikes;
    std::vector<std::vector<double>> gates;
    std::vector<std::vector<double>> computedGates;
    std::vector<std::vector<double>> dendriticVoltagesMv;
    std::vector<SpikeEvent> events;
    std::vector<double> effectiveTransmissionGates;
    std::vector<double> eligibilityTransmissionFactors;
    std::vector<double> finalWeights;
    std::vector<double> finalLocalEligibility;
    SimulationMetrics metrics;
};

class SpikingNetwork {
public:
    explicit SpikingNetwork(NetworkConfig config);
    SimulationResult run(const std::vector<std::vector<double>>& stimulus);
    bool dalePrincipleHolds() const;

private:
    NetworkConfig config_;
    int excitatoryCount_ = 0;
    std::mt19937_64 rng_;
    std::vector<double> weights_;
    std::vector<int> delaySteps_;
    int maximumDelaySteps_ = 1;
    std::vector<double> voltage_;
    std::vector<double> dendriticVoltage_;
    std::vector<double> synapticState_;
    std::vector<double> excitatorySynapticState_;
    std::vector<double> inhibitorySynapticState_;
    std::vector<int> refractorySteps_;
    std::vector<std::uint8_t> previousSpikes_;
    std::vector<double> preTrace_;
    std::vector<double> postTrace_;
    std::vector<double> localEligibility_;
    std::vector<std::vector<double>> localEligibilityHistory_;
    std::vector<std::size_t> shuffledEligibilitySource_;
    std::vector<double> adaptiveThreshold_;
    std::vector<double> previousEmissionGates_;
    std::vector<double> delayedTransmissionGates_;
    std::vector<int> lastSpikeSteps_;
    std::vector<std::vector<std::uint8_t>> spikeHistory_;
    std::vector<std::vector<double>> emissionGateHistory_;
    std::vector<std::vector<double>> eventFeatureHistory_;
    std::vector<SpikeEvent> events_;
    std::uint32_t stepIndex_ = 0;

    void createWeights();
    void resetState();
    double gateForInput(double value);
    double gateForInput(double value, GateMode mode);
    GateMode gateModeForConnection(int pre, int post) const;
    std::vector<double> computeGates();
    void applyStdp(const std::vector<std::uint8_t>& spikes);
    void updateLocalEligibility(
        const std::vector<std::uint8_t>& spikes);
    bool localEligibilityApplies(int pre, int post) const;
    double localEligibilityFactor(
        std::size_t connection,
        int pre,
        int post);
};

std::vector<std::vector<double>> makeTemporalStimulus(
    int neuronCount,
    int steps,
    int timeBins,
    int label,
    int sampleIndex,
    std::uint64_t seed,
    double baselineCurrent,
    double pulseCurrent,
    double noiseStd);

}  // namespace agbnn
