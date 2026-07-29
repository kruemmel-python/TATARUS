#pragma once

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace agns {

enum class PopulationRole : std::uint8_t {
    Sensory,
    Excitatory,
    Inhibitory,
    Context,
    Motor,
    Modulatory
};

enum class ReceptorType : std::uint8_t {
    Ampa,
    Nmda,
    GabaA,
    GabaB,
    Modulatory
};

struct NervousSystemConfig {
    int sensoryNeurons = 16;
    int excitatoryNeurons = 40;
    int inhibitoryNeurons = 12;
    int contextNeurons = 16;
    int motorNeurons = 8;
    int modulatoryNeurons = 4;
    std::uint64_t seed = 7001;
    double dtMs = 1.0;
    double connectionProbability = 0.07;
    double restingMv = -65.0;
    double resetMv = -70.0;
    double thresholdMv = -50.0;
    double tauSomaMs = 20.0;
    double tauDendriteMs = 35.0;
    double somaDendriteCoupling = 0.22;
    double baseCurrent = 12.5;
    double motorRateScaleHz = 20.0;
    double tauAmpaMs = 5.0;
    double tauNmdaMs = 80.0;
    double tauGabaAMs = 10.0;
    double tauGabaBMs = 120.0;
    double ampaReversalMv = 0.0;
    double nmdaReversalMv = 0.0;
    double gabaAReversalMv = -75.0;
    double gabaBReversalMv = -95.0;
    double refractoryMs = 2.0;
    double adaptationIncrementMv = 1.2;
    double adaptationTauMs = 100.0;
    double targetRateHz = 8.0;
    double homeostasisTauMs = 2000.0;
    double homeostasisGain = 0.003;
    double eligibilityTauMs = 400.0;
    double eligibilityTransmissionGain = 1.0;
    double eligibilityIncrement = 8.0;
    double learningRate = 0.0008;
    double consolidationRate = 0.00002;
    double dopamineTauMs = 250.0;
    double acetylcholineTauMs = 180.0;
    double resourceRecoveryTauMs = 180.0;
    double facilitationTauMs = 120.0;
    double releaseProbability = 0.18;
    double energyRecoveryPerMs = 0.0015;
    double spikeEnergyCost = 0.025;
    double transmissionEnergyCost = 0.0004;
    double structuralIntervalMs = 500.0;
    double pruneUsageThreshold = 0.0001;
    double pruneWeightThreshold = 0.004;
    int maximumNewSynapsesPerInterval = 8;
    int maximumAssemblies = 64;
    double assemblySimilarityThreshold = 0.68;
    bool generatedOperatorEnabled = true;
    bool eligibilityMemoryEnabled = true;
    bool shortTermPlasticityEnabled = true;
    bool longTermPlasticityEnabled = true;
    bool homeostasisEnabled = true;
    bool structuralPlasticityEnabled = true;
    bool energyRegulationEnabled = true;

    [[nodiscard]] int neuronCount() const;
    void validate() const;
};

struct SensorFrame {
    std::vector<double> visionEvents;
    std::vector<double> audioSamples;
    std::vector<double> touch;
    std::vector<std::uint8_t> textBytes;
    // Restricted top-down channel used by CognitiveBridge. It targets the
    // context population and never addresses individual neurons or synapses.
    std::vector<double> contextEvents;
    double temperature = 0.0;
    double internalEnergy = 1.0;
    double reward = 0.0;
    double novelty = 0.0;
};

struct MotorAction {
    double movement = 0.0;
    double attention = 0.0;
    double vocalization = 0.0;
    double confidence = 0.0;
};

struct NervousSystemMetrics {
    std::uint64_t step = 0;
    std::uint64_t totalSpikes = 0;
    std::uint64_t totalTransmissions = 0;
    int activeSynapses = 0;
    int assemblyCount = 0;
    int activeAssembly = -1;
    double meanRateHz = 0.0;
    double meanEnergy = 1.0;
    double dopamine = 0.0;
    double acetylcholine = 0.0;
    double meanEligibility = 0.0;
    double meanResource = 1.0;
    double structuralGrowth = 0.0;
    double structuralPruning = 0.0;
    bool finite = true;
};

struct NeuronStateView {
    PopulationRole role = PopulationRole::Excitatory;
    double somaMv = 0.0;
    double dendriteMv = 0.0;
    double filteredRateHz = 0.0;
    double fastRateHz = 0.0;
    double energy = 0.0;
    std::uint64_t spikeCount = 0;
    bool active = false;
};

struct SynapseStateView {
    std::size_t index = 0;
    std::int64_t parentSynapse = -1;
    std::uint32_t pre = 0;
    std::uint32_t post = 0;
    ReceptorType receptor = ReceptorType::Ampa;
    double weight = 0.0;
    double consolidatedWeight = 0.0;
    double eligibility = 0.0;
    double resource = 0.0;
    double usage = 0.0;
    bool active = false;
};

struct AssemblyStateView {
    std::uint64_t id = 0;
    std::vector<double> prototype;
    double activation = 0.0;
    std::uint64_t observations = 0;
    std::uint64_t lastActiveStep = 0;
};

struct RepresentationState {
    std::uint64_t step = 0;
    int activeAssembly = -1;
    std::vector<NeuronStateView> neurons;
    std::vector<SynapseStateView> synapses;
    std::vector<AssemblyStateView> assemblies;
};

struct DamageReport {
    std::vector<std::size_t> disabledNeurons;
    std::vector<std::size_t> disabledSynapses;
};

struct MechanismDescriptor {
    std::string id;
    std::string formula;
    std::string placement;
    std::string parameterRange;
    std::string evidence;
    std::string status;
};

class MechanismLibrary {
public:
    MechanismLibrary();
    [[nodiscard]] const std::vector<MechanismDescriptor>& entries() const;
    void writeJson(const std::filesystem::path& path) const;

private:
    std::vector<MechanismDescriptor> entries_;
};

class PersistentNervousSystem {
public:
    explicit PersistentNervousSystem(NervousSystemConfig config);
    ~PersistentNervousSystem();

    PersistentNervousSystem(const PersistentNervousSystem&) = delete;
    PersistentNervousSystem& operator=(const PersistentNervousSystem&) = delete;

    MotorAction step(const SensorFrame& frame);
    std::vector<MotorAction> run(
        const std::vector<SensorFrame>& frames);

    [[nodiscard]] const NervousSystemConfig& config() const;
    [[nodiscard]] const NervousSystemMetrics& metrics() const;
    [[nodiscard]] std::uint64_t stateHash() const;
    [[nodiscard]] bool dalePrincipleHolds() const;
    [[nodiscard]] RepresentationState inspect() const;

    void saveSnapshot(const std::filesystem::path& path) const;
    void loadSnapshot(const std::filesystem::path& path);
    void writeStateJson(const std::filesystem::path& path) const;
    void applyDamage(
        double neuronFraction,
        double synapseFraction,
        std::uint64_t seed);
    DamageReport applyDamageWithReport(
        double neuronFraction,
        double synapseFraction,
        std::uint64_t seed);
    DamageReport disableSynapses(
        const std::vector<std::size_t>& synapseIndices);
    void setStructuralPlasticityEnabled(bool enabled);

private:
    struct Neuron;
    struct Synapse;
    struct AxonEvent;
    struct Assembly;

    NervousSystemConfig config_;
    std::mt19937_64 random_;
    std::vector<Neuron> neurons_;
    std::vector<Synapse> synapses_;
    std::vector<std::vector<std::size_t>> outgoing_;
    std::vector<std::vector<AxonEvent>> axonQueue_;
    std::vector<Assembly> assemblies_;
    std::vector<std::uint8_t> spikes_;
    std::vector<double> preTrace_;
    std::vector<double> postTrace_;
    std::vector<double> assemblyAccumulator_;
    std::vector<double> assemblyBaseline_;
    NervousSystemMetrics metrics_;
    double dopamine_ = 0.0;
    double acetylcholine_ = 0.0;
    std::uint64_t lastStructuralStep_ = 0;
    std::uint64_t stimulusSignature_ = 0;
    int stimulusAgeSteps_ = 0;
    bool stimulusWasPresent_ = false;

    void createNeurons();
    void createSynapses();
    void rebuildOutgoing();
    void encodeSensors(
        const SensorFrame& frame,
        std::vector<double>& somaDrive,
        std::vector<double>& dendriteDrive) const;
    void deliverAxonEvents(const SensorFrame& frame);
    void updateNeurons(
        const std::vector<double>& somaDrive,
        const std::vector<double>& dendriteDrive);
    void scheduleSpikeEvents();
    void updatePlasticity(const SensorFrame& frame);
    void updateAssemblies(const SensorFrame& frame);
    void updateHomeostasisAndEnergy();
    void updateStructuralPlasticity();
    void updateMetrics();
    [[nodiscard]] MotorAction decodeAction() const;
    [[nodiscard]] PopulationRole roleForIndex(int index) const;
    [[nodiscard]] bool connectionExists(int pre, int post) const;
};

class ContinuousEnvironment {
public:
    explicit ContinuousEnvironment(std::uint64_t seed = 9001);

    SensorFrame sense() const;
    double apply(const MotorAction& action);
    void injectTextUtf8(const std::string& text);
    [[nodiscard]] bool reachedTarget() const;
    [[nodiscard]] double position() const;
    [[nodiscard]] double target() const;
    [[nodiscard]] double cumulativeReward() const;

private:
    std::uint64_t seed_ = 0;
    double position_ = -0.75;
    double target_ = 0.65;
    double temperature_ = 0.0;
    double cumulativeReward_ = 0.0;
    double lastReward_ = 0.0;
    std::vector<std::uint8_t> textStream_;
    std::size_t textIndex_ = 0;
};

struct ClosedLoopResult {
    int steps = 0;
    double cumulativeReward = 0.0;
    double finalDistance = 0.0;
    double meanEnergy = 0.0;
    int assemblies = 0;
    int activeSynapses = 0;
    std::uint64_t stateHash = 0;
};

ClosedLoopResult runClosedLoop(
    PersistentNervousSystem& nervousSystem,
    ContinuousEnvironment& environment,
    int steps);

}  // namespace agns
