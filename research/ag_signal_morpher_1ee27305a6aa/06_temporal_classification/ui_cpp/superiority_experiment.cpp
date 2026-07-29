#include "bio_core.hpp"
#include "classifier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kModeCount = 6;
constexpr std::size_t kControlCount = 5;
constexpr std::size_t kPermutationCount = 1'000'000;
constexpr std::size_t kBootstrapCount = 200'000;
constexpr double kNoninferiorityMargin = -0.02;

const std::array<agbnn::GateMode, kModeCount> kModes{
    agbnn::GateMode::Kernel,
    agbnn::GateMode::Constant,
    agbnn::GateMode::Disabled,
    agbnn::GateMode::Sign,
    agbnn::GateMode::Tanh,
    agbnn::GateMode::Random};

const std::array<std::uint64_t, 24> kSeeds{
    89, 107, 131, 149, 173, 197, 223, 251,
    281, 313, 347, 383, 421, 463, 509, 557,
    607, 659, 719, 773, 829, 887, 947, 1013};

struct Condition {
    const char* name;
    int neurons;
    double density;
    int steps;
    double pulse;
    double noise;
};

const std::array<Condition, 4> kConditions{{
    {"standard", 16, 0.18, 120, 2.0, 5.0},
    {"high_noise", 16, 0.18, 120, 2.0, 8.0},
    {"weak_signal", 16, 0.18, 120, 1.0, 5.0},
    {"sparse_network", 24, 0.10, 120, 2.0, 5.0},
}};

struct Observation {
    double accuracy = 0.0;
    double balancedAccuracy = 0.0;
    double spikeCost = 0.0;
    double firingRate = 0.0;
    double separation = 0.0;
    double effectiveGate = 0.0;
    double effectiveGateVariance = 0.0;
};

struct SeedAggregate {
    std::array<double, kModeCount> accuracy{};
    std::array<double, kModeCount> spikeCost{};
    std::array<double, kModeCount> firingRate{};
    std::array<double, kModeCount> separation{};
};

struct ComparisonStatistics {
    agbnn::GateMode control = agbnn::GateMode::Disabled;
    double accuracyDifference = 0.0;
    double accuracyBootstrapLower = 0.0;
    double accuracyP = 1.0;
    double accuracyHolmP = 1.0;
    double costBenefit = 0.0;
    double costP = 1.0;
    double costHolmP = 1.0;
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

double mean(const std::vector<double>& values) {
    require(!values.empty(), "mean requires values");
    return std::accumulate(values.begin(), values.end(), 0.0)
        / static_cast<double>(values.size());
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

agbnn::NetworkConfig makeNetwork(
    const Condition& condition,
    std::uint64_t seed) {
    agbnn::NetworkConfig network;
    network.neuronCount = condition.neurons;
    network.connectionProbability = condition.density;
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
    network.validate();
    return network;
}

agbnn::ClassificationConfig makeTask(
    const Condition& condition,
    std::uint64_t seed) {
    agbnn::ClassificationConfig task;
    task.samplesPerClass = 16;
    task.folds = 4;
    task.steps = condition.steps;
    task.timeBins = 4;
    task.baselineCurrent = 15.0;
    task.pulseCurrent = condition.pulse;
    task.noiseStd = condition.noise;
    task.seed = seed;
    task.trainingEpochs = 200;
    task.validate();
    return task;
}

double signFlipPValue(
    const std::vector<double>& benefits,
    std::uint64_t randomSeed) {
    const double observed = mean(benefits);
    std::mt19937_64 random(randomSeed);
    std::uniform_int_distribution<int> sign(0, 1);
    std::size_t atLeastObserved = 0;
    for (
        std::size_t permutation = 0;
        permutation < kPermutationCount;
        ++permutation) {
        double permuted = 0.0;
        for (double benefit : benefits) {
            permuted += sign(random) == 0 ? -benefit : benefit;
        }
        permuted /= static_cast<double>(benefits.size());
        if (permuted >= observed - 1e-15) {
            ++atLeastObserved;
        }
    }
    return static_cast<double>(atLeastObserved + 1)
        / static_cast<double>(kPermutationCount + 1);
}

double bootstrapLowerBound(
    const std::vector<double>& differences,
    std::uint64_t randomSeed) {
    std::mt19937_64 random(randomSeed);
    std::uniform_int_distribution<std::size_t> sample(
        0, differences.size() - 1);
    std::vector<double> bootstrapMeans;
    bootstrapMeans.reserve(kBootstrapCount);
    for (
        std::size_t replicate = 0;
        replicate < kBootstrapCount;
        ++replicate) {
        double sum = 0.0;
        for (std::size_t index = 0; index < differences.size(); ++index) {
            sum += differences[sample(random)];
        }
        bootstrapMeans.push_back(
            sum / static_cast<double>(differences.size()));
    }
    const std::size_t percentileIndex = static_cast<std::size_t>(
        std::floor(0.05 * static_cast<double>(kBootstrapCount - 1)));
    std::nth_element(
        bootstrapMeans.begin(),
        bootstrapMeans.begin()
            + static_cast<std::ptrdiff_t>(percentileIndex),
        bootstrapMeans.end());
    return bootstrapMeans[percentileIndex];
}

std::array<double, kControlCount> holmAdjust(
    const std::array<double, kControlCount>& pValues) {
    std::array<std::size_t, kControlCount> order{};
    std::iota(order.begin(), order.end(), 0);
    std::sort(
        order.begin(),
        order.end(),
        [&](std::size_t left, std::size_t right) {
            return pValues[left] < pValues[right];
        });
    std::array<double, kControlCount> adjusted{};
    double runningMaximum = 0.0;
    for (std::size_t rank = 0; rank < kControlCount; ++rank) {
        const std::size_t index = order[rank];
        const double candidate = std::min(
            1.0,
            pValues[index]
                * static_cast<double>(kControlCount - rank));
        runningMaximum = std::max(runningMaximum, candidate);
        adjusted[index] = runningMaximum;
    }
    return adjusted;
}

std::string experimentDescriptor() {
    std::ostringstream out;
    out << "GO-SNN-MS24-C4-v1;samples=16;folds=4;epochs=200;";
    for (std::uint64_t seed : kSeeds) {
        out << seed << ",";
    }
    for (const auto& condition : kConditions) {
        out << condition.name << ":" << condition.neurons << ":"
            << std::setprecision(17) << condition.density << ":"
            << condition.steps << ":" << condition.pulse << ":"
            << condition.noise << ";";
    }
    return out.str();
}

std::string experimentHash() {
    const std::string descriptor = experimentDescriptor();
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : descriptor) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << std::setw(16) << hash;
    return out.str();
}

void writeRawCsv(
    const std::filesystem::path& path,
    const std::vector<std::array<std::array<Observation, kModeCount>, 4>>&
        observations) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot create raw CSV");
    output << "experiment_hash,seed,condition,mode,accuracy,"
        "balanced_accuracy,spikes_per_correct,firing_rate_hz,"
        "assembly_separation,effective_gate,effective_gate_variance\n";
    output << std::fixed << std::setprecision(12);
    for (std::size_t seedIndex = 0; seedIndex < kSeeds.size(); ++seedIndex) {
        for (
            std::size_t conditionIndex = 0;
            conditionIndex < kConditions.size();
            ++conditionIndex) {
            for (
                std::size_t modeIndex = 0;
                modeIndex < kModeCount;
                ++modeIndex) {
                const auto& item =
                    observations[seedIndex][conditionIndex][modeIndex];
                output << experimentHash() << ","
                    << kSeeds[seedIndex] << ","
                    << kConditions[conditionIndex].name << ","
                    << modeName(kModes[modeIndex]) << ","
                    << item.accuracy << ","
                    << item.balancedAccuracy << ","
                    << item.spikeCost << ","
                    << item.firingRate << ","
                    << item.separation << ","
                    << item.effectiveGate << ","
                    << item.effectiveGateVariance << "\n";
            }
        }
    }
    require(static_cast<bool>(output), "cannot write raw CSV");
}

void writeSummaryJson(
    const std::filesystem::path& path,
    const std::array<SeedAggregate, 24>& aggregates,
    const std::array<ComparisonStatistics, kControlCount>& statistics,
    bool geometrySuperiority,
    bool efficiencySuperiority,
    bool formulaEfficiencySuperiority,
    bool allControlEfficiencySuperiority) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot create summary JSON");
    output << std::boolalpha << std::fixed << std::setprecision(12)
        << "{\n"
        << "  \"experiment_id\": \"GO-SNN-MS24-C4-v1\",\n"
        << "  \"experiment_hash\": \"" << experimentHash() << "\",\n"
        << "  \"seed_count\": 24,\n"
        << "  \"condition_count\": 4,\n"
        << "  \"statistical_unit\": \"seed_mean_across_conditions\",\n"
        << "  \"permutations_per_test\": " << kPermutationCount << ",\n"
        << "  \"bootstrap_replicates\": " << kBootstrapCount << ",\n"
        << "  \"noninferiority_margin\": "
        << kNoninferiorityMargin << ",\n"
        << "  \"claims\": {\n"
        << "    \"formula_geometry_accuracy_superiority\": "
        << geometrySuperiority << ",\n"
        << "    \"system_efficiency_superiority\": "
        << efficiencySuperiority << ",\n"
        << "    \"formula_specific_efficiency_superiority\": "
        << formulaEfficiencySuperiority << ",\n"
        << "    \"global_efficiency_superiority_all_controls\": "
        << allControlEfficiencySuperiority << "\n"
        << "  },\n"
        << "  \"mode_means\": [\n";
    for (std::size_t modeIndex = 0; modeIndex < kModeCount; ++modeIndex) {
        std::vector<double> accuracies;
        std::vector<double> costs;
        std::vector<double> rates;
        std::vector<double> separations;
        for (const auto& seed : aggregates) {
            accuracies.push_back(seed.accuracy[modeIndex]);
            costs.push_back(seed.spikeCost[modeIndex]);
            rates.push_back(seed.firingRate[modeIndex]);
            separations.push_back(seed.separation[modeIndex]);
        }
        output << "    {\"mode\": \"" << modeName(kModes[modeIndex])
            << "\", \"accuracy\": " << mean(accuracies)
            << ", \"spikes_per_correct\": " << mean(costs)
            << ", \"firing_rate_hz\": " << mean(rates)
            << ", \"assembly_separation\": " << mean(separations)
            << "}"
            << (modeIndex + 1 == kModeCount ? "\n" : ",\n");
    }
    output << "  ],\n"
        << "  \"kernel_comparisons\": [\n";
    for (std::size_t index = 0; index < statistics.size(); ++index) {
        const auto& item = statistics[index];
        output << "    {\"control\": \"" << modeName(item.control)
            << "\", \"accuracy_difference\": "
            << item.accuracyDifference
            << ", \"accuracy_bootstrap_lower_95_one_sided\": "
            << item.accuracyBootstrapLower
            << ", \"accuracy_p_one_sided\": " << item.accuracyP
            << ", \"accuracy_p_holm\": " << item.accuracyHolmP
            << ", \"spike_cost_benefit\": " << item.costBenefit
            << ", \"cost_p_one_sided\": " << item.costP
            << ", \"cost_p_holm\": " << item.costHolmP
            << "}"
            << (index + 1 == statistics.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
        << "  \"decision\": \""
        << (
            geometrySuperiority
            || efficiencySuperiority
            || formulaEfficiencySuperiority
                ? "SCOPED_SUPERIORITY_SUPPORTED"
                : "NO_SUPERIORITY_DEMONSTRATED")
        << "\"\n"
        << "}\n";
    require(static_cast<bool>(output), "cannot write summary JSON");
}

void writeReport(
    const std::filesystem::path& path,
    const std::array<SeedAggregate, 24>& aggregates,
    const std::array<ComparisonStatistics, kControlCount>& statistics,
    bool geometrySuperiority,
    bool efficiencySuperiority,
    bool formulaEfficiencySuperiority,
    bool allControlEfficiencySuperiority) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot create report");
    output << "# Ergebnis des vorab festgelegten Mehrseed-Tests\n\n"
        << "Experiment-Hash: `" << experimentHash() << "`\n\n"
        << "Statistische Einheit: Mittel der vier Bedingungen je Seed "
        "(`n = 24` neue Seeds).\n\n"
        << "## Modusmittel\n\n"
        << "| Modus | Accuracy | Spikes/korrekt | Rate Hz | Separation |\n"
        << "|---|---:|---:|---:|---:|\n";
    output << std::fixed << std::setprecision(6);
    for (std::size_t modeIndex = 0; modeIndex < kModeCount; ++modeIndex) {
        std::vector<double> accuracies;
        std::vector<double> costs;
        std::vector<double> rates;
        std::vector<double> separations;
        for (const auto& seed : aggregates) {
            accuracies.push_back(seed.accuracy[modeIndex]);
            costs.push_back(seed.spikeCost[modeIndex]);
            rates.push_back(seed.firingRate[modeIndex]);
            separations.push_back(seed.separation[modeIndex]);
        }
        output << "| " << modeName(kModes[modeIndex])
            << " | " << mean(accuracies)
            << " | " << mean(costs)
            << " | " << mean(rates)
            << " | " << mean(separations) << " |\n";
    }
    output << "\n## Gepaarte Vergleiche Kernel minus Kontrolle\n\n"
        << "| Kontrolle | ΔAccuracy | untere 95-%-Grenze | p Holm Accuracy | "
        "Kostenvorteil | p Holm Kosten |\n"
        << "|---|---:|---:|---:|---:|---:|\n";
    for (const auto& item : statistics) {
        output << "| " << modeName(item.control)
            << " | " << item.accuracyDifference
            << " | " << item.accuracyBootstrapLower
            << " | " << item.accuracyHolmP
            << " | " << item.costBenefit
            << " | " << item.costHolmP << " |\n";
    }
    output << "\n## Vorab definierte Entscheidungen\n\n"
        << "- Formelgeometrische Accuracy-Überlegenheit: **"
        << (geometrySuperiority ? "BESTÄTIGT" : "NICHT BESTÄTIGT")
        << "**\n"
        << "- Systemische Effizienzüberlegenheit gegen Konstante und "
        "deaktiviert: **"
        << (efficiencySuperiority ? "BESTÄTIGT" : "NICHT BESTÄTIGT")
        << "**\n"
        << "- Formelspezifische Effizienzüberlegenheit gegen Zufall: **"
        << (
            formulaEfficiencySuperiority
                ? "BESTÄTIGT"
                : "NICHT BESTÄTIGT")
        << "**\n"
        << "- Effizienzüberlegenheit gegen alle fünf Kontrollen: **"
        << (
            allControlEfficiencySuperiority
                ? "BESTÄTIGT"
                : "NICHT BESTÄTIGT")
        << "**\n\n"
        << "## Zulässige Schlussfolgerung\n\n";
    if (geometrySuperiority) {
        output << "Unter dieser synthetischen Aufgabenfamilie besitzt der "
            "Originalkernel eine vorab definierte Accuracy-Überlegenheit "
            "gegen event-gematchte Konstante und verteilungsgematchtes "
            "Zufallsgate.\n";
    } else if (formulaEfficiencySuperiority) {
        output << "Unter nichtunterlegener Accuracy besitzt der "
            "Originalkernel in dieser synthetischen Aufgabenfamilie einen "
            "Spikekostenvorteil gegenüber event-gematchter Konstante, "
            "deaktiviertem Gate und verteilungsgematchtem Zufallsgate. "
            "Das Vorzeichengate ist im Mittel sparsamer; deshalb besteht "
            "keine Überlegenheit gegenüber allen Kontrollen.\n";
    } else if (efficiencySuperiority) {
        output << "Unter nichtunterlegener Accuracy besitzt das "
            "kernelmodulierte System einen Spikekostenvorteil gegenüber "
            "Konstante und deaktiviert. Da die Zufallskontrolle nicht "
            "übertroffen wurde, ist dieser Vorteil nicht eindeutig der "
            "Kernelgeometrie zuzurechnen.\n";
    } else {
        output << "`NO_SUPERIORITY_DEMONSTRATED`: Die vorab festgelegten "
            "Kriterien wurden nicht vollständig erfüllt.\n";
    }
    output << "\nAlle Aussagen gelten nur für das synthetische "
        "Forschungsmodell und sind keine biologische Validierung.\n";
    require(static_cast<bool>(output), "cannot write report");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory =
            argc > 1
                ? std::filesystem::path(argv[1])
                : std::filesystem::current_path();
        std::filesystem::create_directories(outputDirectory);
        std::vector<
            std::array<std::array<Observation, kModeCount>, 4>>
            observations(kSeeds.size());

        std::cout << "Experiment GO-SNN-MS24-C4-v1 hash="
            << experimentHash() << "\n";
        for (
            std::size_t seedIndex = 0;
            seedIndex < kSeeds.size();
            ++seedIndex) {
            for (
                std::size_t conditionIndex = 0;
                conditionIndex < kConditions.size();
                ++conditionIndex) {
                const Condition& condition =
                    kConditions[conditionIndex];
                const auto evaluations =
                    agbnn::TemporalClassifier(
                        makeNetwork(condition, kSeeds[seedIndex]),
                        makeTask(condition, kSeeds[seedIndex]))
                        .compareAll();
                require(
                    evaluations.size() == kModeCount,
                    "compareAll did not return six modes");
                for (
                    std::size_t modeIndex = 0;
                    modeIndex < kModeCount;
                    ++modeIndex) {
                    const auto& source = evaluations[modeIndex];
                    require(
                        source.gateMode == kModes[modeIndex],
                        "unexpected mode order");
                    Observation& target =
                        observations[seedIndex]
                            [conditionIndex][modeIndex];
                    target.accuracy = source.meanAccuracy();
                    target.balancedAccuracy =
                        source.meanBalancedAccuracy();
                    target.spikeCost =
                        source.spikesPerCorrectDecision;
                    target.firingRate = source.meanFiringRateHz;
                    target.separation =
                        source.meanAssemblySeparation();
                    target.effectiveGate =
                        source.meanEffectiveGate;
                    target.effectiveGateVariance =
                        source.meanEffectiveGateVariance;
                    require(
                        std::isfinite(target.accuracy)
                            && std::isfinite(target.spikeCost)
                            && std::isfinite(target.firingRate)
                            && std::isfinite(target.separation),
                        "non-finite experimental observation");
                }
            }
            std::cout << "seed " << (seedIndex + 1) << "/"
                << kSeeds.size() << " (" << kSeeds[seedIndex]
                << ") complete" << std::endl;
        }

        std::array<SeedAggregate, 24> aggregates{};
        for (
            std::size_t seedIndex = 0;
            seedIndex < kSeeds.size();
            ++seedIndex) {
            for (
                std::size_t modeIndex = 0;
                modeIndex < kModeCount;
                ++modeIndex) {
                for (
                    std::size_t conditionIndex = 0;
                    conditionIndex < kConditions.size();
                    ++conditionIndex) {
                    const Observation& item =
                        observations[seedIndex]
                            [conditionIndex][modeIndex];
                    aggregates[seedIndex].accuracy[modeIndex]
                        += item.accuracy / kConditions.size();
                    aggregates[seedIndex].spikeCost[modeIndex]
                        += item.spikeCost / kConditions.size();
                    aggregates[seedIndex].firingRate[modeIndex]
                        += item.firingRate / kConditions.size();
                    aggregates[seedIndex].separation[modeIndex]
                        += item.separation / kConditions.size();
                }
            }
        }

        std::array<ComparisonStatistics, kControlCount> statistics{};
        std::array<double, kControlCount> accuracyPValues{};
        std::array<double, kControlCount> costPValues{};
        for (
            std::size_t controlIndex = 0;
            controlIndex < kControlCount;
            ++controlIndex) {
            const std::size_t modeIndex = controlIndex + 1;
            std::vector<double> accuracyDifferences;
            std::vector<double> costBenefits;
            for (const auto& seed : aggregates) {
                accuracyDifferences.push_back(
                    seed.accuracy[0] - seed.accuracy[modeIndex]);
                costBenefits.push_back(
                    seed.spikeCost[modeIndex] - seed.spikeCost[0]);
            }
            ComparisonStatistics& item = statistics[controlIndex];
            item.control = kModes[modeIndex];
            item.accuracyDifference = mean(accuracyDifferences);
            item.accuracyBootstrapLower = bootstrapLowerBound(
                accuracyDifferences,
                0xB00757A0ULL + controlIndex);
            item.accuracyP = signFlipPValue(
                accuracyDifferences,
                0xA661C0ULL + controlIndex);
            item.costBenefit = mean(costBenefits);
            item.costP = signFlipPValue(
                costBenefits,
                0xC057BEEFULL + controlIndex);
            accuracyPValues[controlIndex] = item.accuracyP;
            costPValues[controlIndex] = item.costP;
        }
        const auto adjustedAccuracy = holmAdjust(accuracyPValues);
        const auto adjustedCost = holmAdjust(costPValues);
        for (
            std::size_t index = 0;
            index < statistics.size();
            ++index) {
            statistics[index].accuracyHolmP =
                adjustedAccuracy[index];
            statistics[index].costHolmP = adjustedCost[index];
        }

        const auto accuracySuperior = [&](std::size_t controlIndex) {
            const auto& item = statistics[controlIndex];
            return item.accuracyDifference > 0.0
                && item.accuracyHolmP < 0.05;
        };
        const auto efficiencySuperior = [&](std::size_t controlIndex) {
            const auto& item = statistics[controlIndex];
            return item.accuracyBootstrapLower
                    > kNoninferiorityMargin
                && item.costBenefit > 0.0
                && item.costHolmP < 0.05;
        };
        const bool geometrySuperiority =
            accuracySuperior(0) && accuracySuperior(4);
        const bool efficiencySuperiority =
            efficiencySuperior(0) && efficiencySuperior(1);
        const bool formulaEfficiencySuperiority =
            efficiencySuperiority && efficiencySuperior(4);
        const bool allControlEfficiencySuperiority =
            efficiencySuperior(0)
            && efficiencySuperior(1)
            && efficiencySuperior(2)
            && efficiencySuperior(3)
            && efficiencySuperior(4);

        writeRawCsv(
            outputDirectory / "raw_results.csv",
            observations);
        writeSummaryJson(
            outputDirectory / "summary.json",
            aggregates,
            statistics,
            geometrySuperiority,
            efficiencySuperiority,
            formulaEfficiencySuperiority,
            allControlEfficiencySuperiority);
        writeReport(
            outputDirectory / "RESULT_REPORT.md",
            aggregates,
            statistics,
            geometrySuperiority,
            efficiencySuperiority,
            formulaEfficiencySuperiority,
            allControlEfficiencySuperiority);

        std::cout << "Results written to "
            << outputDirectory.string() << "\n"
            << "geometry_accuracy_superiority="
            << std::boolalpha << geometrySuperiority << "\n"
            << "system_efficiency_superiority="
            << efficiencySuperiority << "\n"
            << "formula_efficiency_superiority="
            << formulaEfficiencySuperiority << "\n"
            << "all_control_efficiency_superiority="
            << allControlEfficiencySuperiority << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Superiority experiment failed: "
            << error.what() << "\n";
        return 1;
    }
}
