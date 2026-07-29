#include "bio_core.hpp"
#include "classifier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

double mean(const std::vector<double>& values) {
    return values.empty()
        ? 0.0
        : std::accumulate(values.begin(), values.end(), 0.0)
            / static_cast<double>(values.size());
}

double voltageRmse(
    const agbnn::SimulationResult& left,
    const agbnn::SimulationResult& right) {
    require(
        left.voltagesMv.size() == right.voltagesMv.size(),
        "voltage traces have different lengths");
    double squared = 0.0;
    std::size_t count = 0;
    for (std::size_t step = 0; step < left.voltagesMv.size(); ++step) {
        require(
            left.voltagesMv[step].size()
                == right.voltagesMv[step].size(),
            "voltage traces have different neuron counts");
        for (
            std::size_t neuron = 0;
            neuron < left.voltagesMv[step].size();
            ++neuron) {
            const double difference =
                left.voltagesMv[step][neuron]
                - right.voltagesMv[step][neuron];
            squared += difference * difference;
            ++count;
        }
    }
    return count == 0
        ? 0.0
        : std::sqrt(squared / static_cast<double>(count));
}

double pairedSignFlipPValue(const std::vector<double>& differences) {
    require(!differences.empty(), "sign-flip test needs differences");
    require(
        differences.size() <= 20,
        "exact sign-flip test is limited to 20 pairs");
    const double observed = std::abs(mean(differences));
    const std::size_t permutationCount =
        static_cast<std::size_t>(1) << differences.size();
    std::size_t extreme = 0;
    for (
        std::size_t permutation = 0;
        permutation < permutationCount;
        ++permutation) {
        double sum = 0.0;
        for (
            std::size_t index = 0;
            index < differences.size();
            ++index) {
            sum += ((permutation >> index) & 1U) != 0
                ? differences[index]
                : -differences[index];
        }
        if (
            std::abs(sum / static_cast<double>(differences.size()))
            >= observed - 1e-12) {
            ++extreme;
        }
    }
    return static_cast<double>(extreme)
        / static_cast<double>(permutationCount);
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

agbnn::NetworkConfig extendedNetwork(std::uint64_t seed) {
    agbnn::NetworkConfig network;
    network.neuronCount = 16;
    network.seed = seed;
    network.gateTiming = agbnn::GateTiming::EmissionState;
    network.emissionFeature =
        agbnn::EmissionFeature::FeatureProjection;
    network.minimumAxonDelayMs = 1.0;
    network.maximumAxonDelayMs = 5.0;
    network.synapseModel =
        agbnn::SynapseModel::ConductanceBased;
    network.dendriteEnabled = true;
    network.externalToDendriteFraction = 0.0;
    network.classSpecificOperatorsEnabled = true;
    network.plasticityEnabled = true;
    return network;
}

agbnn::ClassificationConfig acceptanceTask(std::uint64_t seed) {
    agbnn::ClassificationConfig task;
    task.samplesPerClass = 4;
    task.folds = 2;
    task.steps = 80;
    task.timeBins = 4;
    task.baselineCurrent = 15.0;
    task.pulseCurrent = 2.0;
    task.noiseStd = 5.0;
    task.seed = seed;
    task.trainingEpochs = 40;
    return task;
}

struct AcceptanceResults {
    double historicalGate = 0.0;
    int extendedNullSpikes = 0;
    int extendedNullTransmissions = 0;
    std::array<double, 6> homogeneousAccuracy{};
    std::array<double, 4> singleRoleRmse{};
    std::array<double, 5> kernelAccuracy{};
    std::array<double, 5> disabledAccuracy{};
    double multiseedP = 1.0;
};

AcceptanceResults runAcceptance() {
    AcceptanceResults results;

    // 1. Historical reset-locked regression.
    agbnn::NetworkConfig historical;
    historical.neuronCount = 12;
    historical.seed = 73;
    historical.minimumAxonDelayMs = 1.0;
    historical.maximumAxonDelayMs = 1.0;
    historical.synapseModel = agbnn::SynapseModel::CurrentBased;
    historical.dendriteEnabled = false;
    historical.classSpecificOperatorsEnabled = false;
    const auto historicalStimulus = agbnn::makeTemporalStimulus(
        historical.neuronCount,
        80,
        4,
        0,
        0,
        3801,
        12.5,
        2.0,
        5.0);
    agbnn::SpikingNetwork historicalKernelNetwork(historical);
    const auto historicalKernel =
        historicalKernelNetwork.run(historicalStimulus);
    require(historicalKernel.metrics.finite, "historical run is not finite");
    require(
        historicalKernelNetwork.dalePrincipleHolds(),
        "historical run violates Dale principle");
    require(
        std::abs(
            historicalKernel.metrics.effectiveGateMean
            - 0.12831112128784755) < 1e-14,
        "historical effective gate changed");
    require(
        historicalKernel.metrics.effectiveGateVariance == 0.0,
        "historical effective gate is no longer constant");
    agbnn::NetworkConfig historicalConstant = historical;
    historicalConstant.gateMode = agbnn::GateMode::Constant;
    historicalConstant.constantGate =
        historicalKernel.metrics.effectiveGateMean;
    agbnn::SpikingNetwork historicalConstantNetwork(
        historicalConstant);
    const auto constantResult =
        historicalConstantNetwork.run(historicalStimulus);
    require(
        historicalKernel.spikes == constantResult.spikes,
        "historical kernel and matched constant spikes differ");
    require(
        historicalKernel.voltagesMv == constantResult.voltagesMv,
        "historical kernel and matched constant voltages differ");
    results.historicalGate =
        historicalKernel.metrics.effectiveGateMean;

    // 2. Extended biophysical null: advanced mechanics, no class gate.
    agbnn::NetworkConfig extendedNull = extendedNetwork(38);
    extendedNull.eeGateMode = agbnn::GateMode::Disabled;
    extendedNull.eiGateMode = agbnn::GateMode::Disabled;
    extendedNull.ieGateMode = agbnn::GateMode::Disabled;
    extendedNull.iiGateMode = agbnn::GateMode::Disabled;
    const auto extendedStimulus = agbnn::makeTemporalStimulus(
        extendedNull.neuronCount,
        240,
        4,
        1,
        3,
        991,
        15.0,
        2.0,
        5.0);
    agbnn::SpikingNetwork extendedNullNetwork(extendedNull);
    const auto extendedNullResult =
        extendedNullNetwork.run(extendedStimulus);
    require(
        extendedNullResult.metrics.finite,
        "extended biophysical null is not finite");
    require(
        extendedNullResult.metrics.totalSpikes > 0,
        "extended biophysical null has no spikes");
    require(
        extendedNullResult.metrics.synapticTransmissionCount > 0,
        "extended biophysical null has no synaptic transmissions");
    require(
        extendedNullResult.metrics.dendriticVoltageEnergy > 0.0,
        "extended biophysical null has no dendritic dynamics");
    require(
        extendedNullNetwork.dalePrincipleHolds(),
        "extended biophysical null violates Dale principle");
    require(
        std::all_of(
            extendedNullResult.effectiveTransmissionGates.begin(),
            extendedNullResult.effectiveTransmissionGates.end(),
            [](double value) { return value == 1.0; }),
        "disabled class operators are not neutral");
    results.extendedNullSpikes =
        extendedNullResult.metrics.totalSpikes;
    results.extendedNullTransmissions =
        extendedNullResult.metrics.synapticTransmissionCount;

    // 3. Homogeneous operator ecology with calibrated controls.
    agbnn::NetworkConfig homogeneous = extendedNetwork(38);
    const auto homogeneousEvaluations =
        agbnn::TemporalClassifier(
            homogeneous,
            acceptanceTask(38))
            .compareAll();
    require(
        homogeneousEvaluations.size() == 6,
        "homogeneous ecology did not return six controls");
    for (
        std::size_t index = 0;
        index < homogeneousEvaluations.size();
        ++index) {
        const auto& evaluation = homogeneousEvaluations[index];
        require(
            std::isfinite(evaluation.meanAccuracy())
                && evaluation.meanAccuracy() >= 0.0
                && evaluation.meanAccuracy() <= 1.0,
            "homogeneous ecology has invalid accuracy");
        require(
            std::isfinite(evaluation.meanEffectiveGate)
                && std::isfinite(evaluation.spikesPerCorrectDecision),
            "homogeneous ecology has invalid metrics");
        results.homogeneousAccuracy[index] =
            evaluation.meanAccuracy();
    }

    // 4. Only one connection class uses the generated kernel at a time.
    agbnn::NetworkConfig allDisabled = extendedNetwork(53);
    allDisabled.plasticityEnabled = false;
    allDisabled.eeGateMode = agbnn::GateMode::Disabled;
    allDisabled.eiGateMode = agbnn::GateMode::Disabled;
    allDisabled.ieGateMode = agbnn::GateMode::Disabled;
    allDisabled.iiGateMode = agbnn::GateMode::Disabled;
    const auto roleStimulus = agbnn::makeTemporalStimulus(
        allDisabled.neuronCount,
        240,
        4,
        0,
        2,
        8128,
        15.0,
        3.0,
        3.0);
    agbnn::SpikingNetwork disabledNetwork(allDisabled);
    const auto disabledRoleResult =
        disabledNetwork.run(roleStimulus);
    double maximumRoleDifference = 0.0;
    for (std::size_t role = 0; role < 4; ++role) {
        agbnn::NetworkConfig singleRole = allDisabled;
        switch (role) {
            case 0:
                singleRole.eeGateMode = agbnn::GateMode::Kernel;
                break;
            case 1:
                singleRole.eiGateMode = agbnn::GateMode::Kernel;
                break;
            case 2:
                singleRole.ieGateMode = agbnn::GateMode::Kernel;
                break;
            case 3:
                singleRole.iiGateMode = agbnn::GateMode::Kernel;
                break;
        }
        agbnn::SpikingNetwork roleNetwork(singleRole);
        const auto roleResult = roleNetwork.run(roleStimulus);
        require(roleResult.metrics.finite, "single-role run is not finite");
        require(
            roleNetwork.dalePrincipleHolds(),
            "single-role run violates Dale principle");
        results.singleRoleRmse[role] =
            voltageRmse(disabledRoleResult, roleResult);
        maximumRoleDifference = std::max(
            maximumRoleDifference,
            results.singleRoleRmse[role]);
    }
    require(
        maximumRoleDifference > 1e-10,
        "no single operator role changes network dynamics");

    // 5. Extended exact five-seed experiment and its resolution limit.
    const std::array<std::uint64_t, 5> seeds{11, 23, 38, 53, 71};
    std::vector<double> differences;
    for (std::size_t index = 0; index < seeds.size(); ++index) {
        agbnn::NetworkConfig network = extendedNetwork(seeds[index]);
        const auto evaluations =
            agbnn::TemporalClassifier(
                network,
                acceptanceTask(seeds[index]))
                .compareAll();
        require(
            evaluations.size() == 6,
            "multiseed comparison did not return six controls");
        results.kernelAccuracy[index] =
            evaluations[0].meanAccuracy();
        results.disabledAccuracy[index] =
            evaluations[2].meanAccuracy();
        differences.push_back(
            results.kernelAccuracy[index]
            - results.disabledAccuracy[index]);
    }
    results.multiseedP = pairedSignFlipPValue(differences);
    require(
        std::isfinite(results.multiseedP)
            && results.multiseedP >= 0.0625
            && results.multiseedP <= 1.0,
        "five-seed exact p-value violates its resolution limit");

    return results;
}

void writeJson(
    const std::string& path,
    const AcceptanceResults& results) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot open acceptance JSON");
    }
    output << std::fixed << std::setprecision(12)
        << "{\n"
        << "  \"status\": \"passed\",\n"
        << "  \"evidence_scope\": \"synthetic_research_model\",\n"
        << "  \"historical_regression\": {\n"
        << "    \"status\": \"passed\",\n"
        << "    \"effective_gate\": " << results.historicalGate << "\n"
        << "  },\n"
        << "  \"extended_biophysical_null\": {\n"
        << "    \"status\": \"passed\",\n"
        << "    \"spikes\": " << results.extendedNullSpikes << ",\n"
        << "    \"transmissions\": "
        << results.extendedNullTransmissions << "\n"
        << "  },\n"
        << "  \"homogeneous_operator_ecology\": [\n";
    const std::array<agbnn::GateMode, 6> modes{
        agbnn::GateMode::Kernel,
        agbnn::GateMode::Constant,
        agbnn::GateMode::Disabled,
        agbnn::GateMode::Sign,
        agbnn::GateMode::Tanh,
        agbnn::GateMode::Random};
    for (std::size_t index = 0; index < modes.size(); ++index) {
        output << "    {\"mode\": \"" << modeName(modes[index])
            << "\", \"accuracy\": "
            << results.homogeneousAccuracy[index] << "}"
            << (index + 1 == modes.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
        << "  \"single_operator_roles\": {\n"
        << "    \"EE_voltage_rmse\": " << results.singleRoleRmse[0]
        << ",\n"
        << "    \"EI_voltage_rmse\": " << results.singleRoleRmse[1]
        << ",\n"
        << "    \"IE_voltage_rmse\": " << results.singleRoleRmse[2]
        << ",\n"
        << "    \"II_voltage_rmse\": " << results.singleRoleRmse[3]
        << "\n"
        << "  },\n"
        << "  \"extended_multiseed\": {\n"
        << "    \"seeds\": [11, 23, 38, 53, 71],\n"
        << "    \"minimum_two_sided_exact_p\": 0.062500000000,\n"
        << "    \"kernel_vs_disabled_p\": "
        << results.multiseedP << ",\n"
        << "    \"kernel_accuracy\": [";
    for (std::size_t index = 0; index < 5; ++index) {
        if (index) output << ", ";
        output << results.kernelAccuracy[index];
    }
    output << "],\n"
        << "    \"disabled_accuracy\": [";
    for (std::size_t index = 0; index < 5; ++index) {
        if (index) output << ", ";
        output << results.disabledAccuracy[index];
    }
    output << "]\n"
        << "  }\n"
        << "}\n";
    if (!output) {
        throw std::runtime_error("cannot write acceptance JSON");
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const AcceptanceResults results = runAcceptance();
        if (argc > 1) {
            writeJson(argv[1], results);
        }
        std::cout << std::fixed << std::setprecision(6)
            << "All five research acceptance states passed.\n"
            << "Historical effective gate: "
            << results.historicalGate << "\n"
            << "Extended null spikes/transmissions: "
            << results.extendedNullSpikes << "/"
            << results.extendedNullTransmissions << "\n"
            << "Five-seed kernel-vs-disabled p: "
            << results.multiseedP
            << " (minimum possible two-sided exact p: 0.062500)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Acceptance failure: " << error.what() << "\n";
        return 1;
    }
}
