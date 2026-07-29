#include "nervous_system.hpp"
#include "cognitive_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using agns::ContinuousEnvironment;
using agns::NervousSystemConfig;
using agns::PersistentNervousSystem;
using agns::SensorFrame;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

NervousSystemConfig compactConfig() {
    NervousSystemConfig cfg;
    cfg.sensoryNeurons = 12;
    cfg.excitatoryNeurons = 20;
    cfg.inhibitoryNeurons = 6;
    cfg.contextNeurons = 8;
    cfg.motorNeurons = 4;
    cfg.modulatoryNeurons = 2;
    cfg.connectionProbability = 0.16;
    cfg.structuralIntervalMs = 40.0;
    cfg.maximumAssemblies = 12;
    cfg.targetRateHz = 10.0;
    cfg.validate();
    return cfg;
}

SensorFrame deterministicFrame(std::uint64_t step) {
    SensorFrame frame;
    frame.visionEvents = {
        std::sin(static_cast<double>(step) * 0.071),
        std::cos(static_cast<double>(step) * 0.043),
        (step % 17U == 0U) ? 1.0 : 0.0
    };
    frame.audioSamples = {
        std::sin(static_cast<double>(step) * 0.19),
        std::sin(static_cast<double>(step) * 0.07)
    };
    frame.touch = {(step % 31U == 0U) ? 1.0 : 0.0};
    if (step % 53U < 8U) {
        frame.textBytes = {'A', 'B'};
    }
    frame.temperature = 0.25 * std::sin(static_cast<double>(step) * 0.011);
    frame.internalEnergy = 0.8;
    frame.reward = (step % 101U == 0U) ? 0.4 : 0.0;
    frame.novelty = (step % 67U == 0U) ? 0.7 : 0.0;
    return frame;
}

void deterministicTest(const NervousSystemConfig& cfg) {
    auto seeded = cfg;
    seeded.seed = 123456U;
    PersistentNervousSystem a(seeded);
    PersistentNervousSystem b(seeded);
    for (std::uint64_t step = 0; step < 800U; ++step) {
        const auto frame = deterministicFrame(step);
        const auto actionA = a.step(frame);
        const auto actionB = b.step(frame);
        require(actionA.movement == actionB.movement &&
                    actionA.attention == actionB.attention &&
                    actionA.vocalization == actionB.vocalization &&
                    actionA.confidence == actionB.confidence,
                "same seed produced different motor actions");
    }
    require(a.stateHash() == b.stateHash(), "same seed produced different state hash");
    require(a.dalePrincipleHolds() && b.dalePrincipleHolds(), "Dale law violated");
}

void snapshotContinuationTest(const NervousSystemConfig& cfg,
                              const std::filesystem::path& snapshotPath) {
    auto seeded = cfg;
    seeded.seed = 778899U;
    PersistentNervousSystem uninterrupted(seeded);
    for (std::uint64_t step = 0; step < 420U; ++step) {
        uninterrupted.step(deterministicFrame(step));
    }
    uninterrupted.saveSnapshot(snapshotPath.string());

    auto restoreConfig = cfg;
    restoreConfig.seed = 1U;
    PersistentNervousSystem restored(restoreConfig);
    restored.loadSnapshot(snapshotPath.string());
    require(uninterrupted.stateHash() == restored.stateHash(),
            "snapshot restore did not reproduce exact state");

    for (std::uint64_t step = 420U; step < 900U; ++step) {
        const auto frame = deterministicFrame(step);
        const auto actionA = uninterrupted.step(frame);
        const auto actionB = restored.step(frame);
        require(actionA.movement == actionB.movement &&
                    actionA.attention == actionB.attention &&
                    actionA.vocalization == actionB.vocalization &&
                    actionA.confidence == actionB.confidence,
                "snapshot continuation changed motor output");
    }
    require(uninterrupted.stateHash() == restored.stateHash(),
            "snapshot continuation diverged");
}

void multimodalPersistentTest(const NervousSystemConfig& cfg) {
    auto seeded = cfg;
    seeded.seed = 20260729U;
    PersistentNervousSystem system(seeded);
    const auto initialHash = system.stateHash();
    for (std::uint64_t step = 0; step < 1400U; ++step) {
        system.step(deterministicFrame(step));
    }
    const auto metrics = system.metrics();
    require(metrics.step == 1400U, "continuous step counter was reset");
    require(system.stateHash() != initialHash, "persistent state did not change");
    require(metrics.totalSpikes > 0U, "multimodal stream caused no spikes");
    require(metrics.totalTransmissions > 0U, "network caused no synaptic transmissions");
    require(std::isfinite(metrics.meanRateHz), "mean rate is not finite");
    require(std::isfinite(metrics.meanEnergy), "mean energy is not finite");
    require(metrics.meanEnergy >= 0.0 && metrics.meanEnergy <= 1.0,
            "energy left normalized range");
    require(system.dalePrincipleHolds(), "Dale law violated after plasticity");
}

void damageAndRecoveryTest(const NervousSystemConfig& cfg) {
    auto seeded = cfg;
    seeded.seed = 404U;
    PersistentNervousSystem system(seeded);
    ContinuousEnvironment environment(909U);
    environment.injectTextUtf8("continuous raw byte stream");
    agns::runClosedLoop(system, environment, 500U);
    const auto before = system.metrics();
    const auto damage = system.applyDamageWithReport(0.12, 0.20, 8181U);
    require(!damage.disabledNeurons.empty(), "damage report contains no neurons");
    require(!damage.disabledSynapses.empty(), "damage report contains no synapses");
    agns::runClosedLoop(system, environment, 1000U);
    const auto after = system.metrics();
    require(after.step == before.step + 1000U, "damaged system stopped advancing");
    require(std::isfinite(after.meanRateHz) && std::isfinite(after.meanEnergy),
            "damaged system became non-finite");
    require(after.totalSpikes >= before.totalSpikes, "cumulative spike state regressed");
    require(after.activeSynapses > 0U, "all synapses disappeared after partial damage");
    require(system.dalePrincipleHolds(), "Dale law violated during damage recovery");
}

void representationInstrumentationTest(const NervousSystemConfig& cfg) {
    auto controlled = cfg;
    controlled.seed = 5151U;
    controlled.eligibilityMemoryEnabled = false;
    PersistentNervousSystem system(controlled);
    for (std::uint64_t step = 0; step < 500U; ++step) {
        system.step(deterministicFrame(step));
    }
    const auto state = system.inspect();
    require(state.step == 500U, "representation state has wrong step");
    require(state.neurons.size() == static_cast<std::size_t>(cfg.neuronCount()),
            "representation state has wrong neuron count");
    require(!state.synapses.empty(), "representation state contains no synapses");
    require(std::all_of(
        state.synapses.begin(),
        state.synapses.end(),
        [](const auto& synapse) {
            return synapse.eligibility == 0.0;
        }),
        "disabled eligibility memory retained a trace");
}

void mechanismLibraryTest(const std::filesystem::path& libraryPath) {
    const agns::MechanismLibrary library;
    require(library.entries().size() >= 6U, "mechanism library is incomplete");
    library.writeJson(libraryPath.string());
    require(std::filesystem::exists(libraryPath), "mechanism library was not written");
    require(std::filesystem::file_size(libraryPath) > 100U,
            "mechanism library artifact is unexpectedly small");
}

void cognitiveBridgeTest(
    const NervousSystemConfig& cfg,
    const std::filesystem::path& outputDirectory) {
    auto controlled = cfg;
    controlled.seed = 91919U;
    controlled.eligibilityTauMs = 800.0;
    controlled.eligibilityTransmissionGain = 10.0;
    controlled.eligibilityIncrement = 20.0;
    PersistentNervousSystem system(controlled);
    agns::CognitiveBridge bridge(system);
    agns::CognitiveCommand command;
    command.attention = agns::AttentionTarget::Vision;
    command.recallCue = 1U;
    command.recallStrength = 0.5;
    SensorFrame frame;
    frame.internalEnergy = 0.9;
    frame.visionEvents = {1.0, 0.0, 0.0, 0.0};
    for (int step = 0; step < 80; ++step) {
        bridge.step(frame, command);
    }
    const auto state = bridge.readState();
    require(
        state.recalledStates.size()
            == 3U * agns::CognitiveBridge::recallGroupCount,
        "cognitive bridge did not expose pooled recall channels");
    require(
        std::all_of(
            state.recalledStates.begin(),
            state.recalledStates.end(),
            [](const auto& recall) {
                return std::isfinite(recall.strength);
            }),
        "cognitive bridge produced a non-finite recall");
    const auto nervousPath = outputDirectory / "bridge_nervous.agns";
    const auto bridgePath = outputDirectory / "bridge_state.bin";
    system.saveSnapshot(nervousPath);
    bridge.saveState(bridgePath);
    const auto expected = bridge.step(frame, command);
    const auto expectedHash = system.stateHash();
    system.loadSnapshot(nervousPath);
    bridge.loadState(bridgePath);
    const auto actual = bridge.step(frame, command);
    require(
        expectedHash == system.stateHash()
            && expected.action.movement == actual.action.movement
            && expected.state.functionalFingerprint
                == actual.state.functionalFingerprint,
        "composite cognitive snapshot did not continue exactly");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto outputDirectory =
            std::filesystem::path(argc > 1 ? argv[1] : "nervous_system_test_output");
        std::filesystem::create_directories(outputDirectory);

        const auto cfg = compactConfig();
        deterministicTest(cfg);
        snapshotContinuationTest(cfg, outputDirectory / "continuation.agns");
        multimodalPersistentTest(cfg);
        damageAndRecoveryTest(cfg);
        representationInstrumentationTest(cfg);
        cognitiveBridgeTest(cfg, outputDirectory);
        mechanismLibraryTest(outputDirectory / "mechanism_library.json");

        std::ofstream report(outputDirectory / "test_report.txt");
        report << "PASS deterministic_same_seed\n"
               << "PASS exact_snapshot_continuation\n"
               << "PASS continuous_multimodal_state\n"
               << "PASS dale_plasticity\n"
               << "PASS damage_recovery\n"
               << "PASS representation_instrumentation\n"
               << "PASS restricted_cognitive_bridge\n"
               << "PASS mechanism_library\n";
        std::cout << "All persistent nervous-system tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Persistent nervous-system test failure: " << error.what() << '\n';
        return 1;
    }
}
