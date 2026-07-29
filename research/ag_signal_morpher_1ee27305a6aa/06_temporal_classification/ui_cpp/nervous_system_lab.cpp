#include "nervous_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Candidate {
    agns::NervousSystemConfig config;
    double reward = 0.0;
    double distance = 0.0;
    double energy = 0.0;
    double rateError = 0.0;
    double score = -std::numeric_limits<double>::infinity();
};

const std::array<std::uint64_t, 3> kDevelopmentSeeds{
    7103, 7211, 7331};

double mean(const std::vector<double>& values) {
    if (values.empty()) {
        throw std::runtime_error("mean requires values");
    }
    return std::accumulate(values.begin(), values.end(), 0.0)
        / static_cast<double>(values.size());
}

Candidate evaluate(agns::NervousSystemConfig config) {
    std::vector<double> rewards;
    std::vector<double> distances;
    std::vector<double> energies;
    std::vector<double> rateErrors;
    for (std::uint64_t seed : kDevelopmentSeeds) {
        config.seed = seed;
        agns::PersistentNervousSystem nervousSystem(config);
        agns::ContinuousEnvironment environment(seed + 1000);
        environment.injectTextUtf8("seek target; avoid boundary; preserve energy");
        const auto result =
            agns::runClosedLoop(nervousSystem, environment, 1200);
        rewards.push_back(result.cumulativeReward);
        distances.push_back(result.finalDistance);
        energies.push_back(result.meanEnergy);
        rateErrors.push_back(std::abs(
            nervousSystem.metrics().meanRateHz - config.targetRateHz));
    }
    Candidate candidate;
    candidate.config = config;
    candidate.reward = mean(rewards);
    candidate.distance = mean(distances);
    candidate.energy = mean(energies);
    candidate.rateError = mean(rateErrors);
    candidate.score =
        candidate.reward
        - 3.0 * candidate.distance
        + 0.5 * candidate.energy
        - 0.02 * candidate.rateError;
    return candidate;
}

agns::NervousSystemConfig mutate(
    const agns::NervousSystemConfig& parent,
    std::mt19937_64& random,
    int generation,
    int child) {
    agns::NervousSystemConfig config = parent;
    std::normal_distribution<double> normal(0.0, 1.0);
    const double scale = 1.0 / std::sqrt(generation + 1.0);
    config.connectionProbability = std::clamp(
        config.connectionProbability + 0.015 * scale * normal(random),
        0.025,
        0.18);
    config.learningRate = std::clamp(
        config.learningRate
            * std::exp(0.35 * scale * normal(random)),
        0.00005,
        0.005);
    config.eligibilityTauMs = std::clamp(
        config.eligibilityTauMs
            * std::exp(0.30 * scale * normal(random)),
        80.0,
        900.0);
    config.homeostasisGain = std::clamp(
        config.homeostasisGain
            * std::exp(0.30 * scale * normal(random)),
        0.0003,
        0.02);
    config.targetRateHz = std::clamp(
        config.targetRateHz + 1.2 * scale * normal(random),
        2.0,
        18.0);
    config.releaseProbability = std::clamp(
        config.releaseProbability + 0.04 * scale * normal(random),
        0.05,
        0.5);
    config.seed =
        8000 + static_cast<std::uint64_t>(generation * 100 + child);
    config.validate();
    return config;
}

Candidate evolve(const std::filesystem::path& outputDirectory) {
    constexpr int generations = 4;
    constexpr int population = 8;
    std::mt19937_64 random(0xE701AULL);
    agns::NervousSystemConfig base;
    Candidate best = evaluate(base);
    std::ofstream raw(
        outputDirectory / "evolution.csv",
        std::ios::binary | std::ios::trunc);
    if (!raw) {
        throw std::runtime_error("evolution.csv konnte nicht erstellt werden");
    }
    raw << "generation,candidate,score,reward,distance,energy,rate_error,"
           "density,learning_rate,eligibility_tau,homeostasis_gain,"
           "target_rate,release_probability\n"
        << std::fixed << std::setprecision(12);
    for (int generation = 0; generation < generations; ++generation) {
        std::vector<Candidate> candidates;
        candidates.push_back(evaluate(best.config));
        for (int child = 1; child < population; ++child) {
            candidates.push_back(evaluate(
                mutate(best.config, random, generation, child)));
        }
        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const Candidate& left, const Candidate& right) {
                return left.score > right.score;
            });
        for (int candidate = 0; candidate < population; ++candidate) {
            const auto& item =
                candidates[static_cast<std::size_t>(candidate)];
            raw << generation << "," << candidate << ","
                << item.score << "," << item.reward << ","
                << item.distance << "," << item.energy << ","
                << item.rateError << ","
                << item.config.connectionProbability << ","
                << item.config.learningRate << ","
                << item.config.eligibilityTauMs << ","
                << item.config.homeostasisGain << ","
                << item.config.targetRateHz << ","
                << item.config.releaseProbability << "\n";
        }
        if (candidates.front().score > best.score) {
            best = candidates.front();
        }
        std::cout << "generation " << (generation + 1)
            << "/" << generations << " score=" << best.score
            << std::endl;
    }
    return best;
}

void writeReport(
    const std::filesystem::path& outputDirectory,
    const Candidate& best,
    const agns::ClosedLoopResult& undamaged,
    const agns::ClosedLoopResult& damaged,
    const agns::NervousSystemMetrics& metrics,
    std::uint64_t resumedHash) {
    std::ofstream report(
        outputDirectory / "ENDSYSTEM_REPORT.md",
        std::ios::binary | std::ios::trunc);
    if (!report) {
        throw std::runtime_error("Endsystembericht konnte nicht geschrieben werden");
    }
    report << "# TATARUS – A Persistent Synthetic Nervous System\n\n"
        << "Status: `integrated_experimental`\n\n"
        << std::fixed << std::setprecision(6)
        << "## Evolvierte Konfiguration\n\n"
        << "- Score: " << best.score << "\n"
        << "- Verbindungsdichte: "
        << best.config.connectionProbability << "\n"
        << "- Lernrate: " << best.config.learningRate << "\n"
        << "- Eligibility-tau: "
        << best.config.eligibilityTauMs << " ms\n"
        << "- Homeostase-Gain: "
        << best.config.homeostasisGain << "\n"
        << "- Zielrate: " << best.config.targetRateHz << " Hz\n"
        << "- Freisetzungswahrscheinlichkeit: "
        << best.config.releaseProbability << "\n\n"
        << "## Kausal geschlossene Laufzeit\n\n"
        << "| Zustand | Reward | Enddistanz | Energie | Assemblies | Synapsen |\n"
        << "|---|---:|---:|---:|---:|---:|\n"
        << "| vor Schaden | " << undamaged.cumulativeReward
        << " | " << undamaged.finalDistance
        << " | " << undamaged.meanEnergy
        << " | " << undamaged.assemblies
        << " | " << undamaged.activeSynapses << " |\n"
        << "| nach 10 % Neuronen-/15 % Synapsenschaden | "
        << damaged.cumulativeReward
        << " | " << damaged.finalDistance
        << " | " << damaged.meanEnergy
        << " | " << damaged.assemblies
        << " | " << damaged.activeSynapses << " |\n\n"
        << "## Persistenter Endzustand\n\n"
        << "- Schritte: " << metrics.step << "\n"
        << "- Gesamtspikes: " << metrics.totalSpikes << "\n"
        << "- Übertragungen: " << metrics.totalTransmissions << "\n"
        << "- mittlere Rate: " << metrics.meanRateHz << " Hz\n"
        << "- mittlere Energie: " << metrics.meanEnergy << "\n"
        << "- aktive Synapsen: " << metrics.activeSynapses << "\n"
        << "- entdeckte Assemblies: " << metrics.assemblyCount << "\n"
        << "- strukturelles Wachstum: " << metrics.structuralGrowth << "\n"
        << "- strukturelle Pruningereignisse: "
        << metrics.structuralPruning << "\n"
        << "- Snapshot-Fortsetzungshash: `" << std::hex
        << std::uppercase << resumedHash << std::dec << "`\n\n"
        << "Das System verarbeitet rohe Textbytes, Audio-, Bild-, Berührungs-, "
           "Temperatur- und Interozeptionskanäle ohne Token-IDs oder "
           "Embeddingtabelle. Der Bericht belegt eine ausführbare "
           "synthetische Closed-loop-Architektur, nicht die biologische "
           "Gleichwertigkeit mit einem realen Organismus.\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory =
            argc > 1
                ? std::filesystem::path(argv[1])
                : std::filesystem::current_path();
        std::filesystem::create_directories(outputDirectory);
        const Candidate best = evolve(outputDirectory);

        agns::NervousSystemConfig finalConfig = best.config;
        finalConfig.seed = 9109;
        agns::PersistentNervousSystem nervousSystem(finalConfig);
        agns::ContinuousEnvironment environment(9209);
        environment.injectTextUtf8(
            "continuous raw bytes bind vision audio touch action");
        const auto undamaged =
            agns::runClosedLoop(nervousSystem, environment, 5000);

        const auto snapshotPath = outputDirectory / "nervous_system.agns";
        nervousSystem.saveSnapshot(snapshotPath);
        nervousSystem.writeStateJson(outputDirectory / "state.json");
        agns::MechanismLibrary{}.writeJson(
            outputDirectory / "mechanism_library.json");

        agns::PersistentNervousSystem resumed(finalConfig);
        resumed.loadSnapshot(snapshotPath);
        agns::ContinuousEnvironment recoveryEnvironment(9301);
        recoveryEnvironment.injectTextUtf8("recover adapt continue");
        resumed.applyDamage(0.10, 0.15, 0xDA6A6EULL);
        const auto damaged =
            agns::runClosedLoop(resumed, recoveryEnvironment, 2500);
        const std::uint64_t resumedHash = resumed.stateHash();
        resumed.saveSnapshot(
            outputDirectory / "nervous_system_after_damage.agns");
        resumed.writeStateJson(
            outputDirectory / "state_after_damage.json");

        writeReport(
            outputDirectory,
            best,
            undamaged,
            damaged,
            resumed.metrics(),
            resumedHash);
        std::cout << "report="
            << (outputDirectory / "ENDSYSTEM_REPORT.md").string()
            << "\n";
        return resumed.metrics().finite
            && resumed.dalePrincipleHolds()
            ? 0
            : 2;
    } catch (const std::exception& error) {
        std::cerr << "nervous-system lab failed: "
            << error.what() << "\n";
        return 1;
    }
}
