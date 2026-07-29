#include "nervous_system.hpp"

#include "ag_signal_morpher_1ee27305a6aa_kernel.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

namespace agns {
namespace {

constexpr double kPi = 3.14159265358979323846;

bool hasPlasticityWriteSignal(const SensorFrame& frame) {
    const auto nonzero = [](const auto& values) {
        return std::any_of(
            values.begin(),
            values.end(),
            [](const auto value) {
                return std::abs(static_cast<double>(value)) > 1e-12;
            });
    };
    return nonzero(frame.visionEvents)
        || nonzero(frame.audioSamples)
        || nonzero(frame.touch)
        || !frame.textBytes.empty()
        || nonzero(frame.contextEvents)
        || std::abs(frame.reward) > 1e-12
        || std::abs(frame.novelty) > 1e-12;
}

template <class T>
void writePod(std::ostream& output, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    output.write(
        reinterpret_cast<const char*>(&value),
        static_cast<std::streamsize>(sizeof(T)));
    if (!output) {
        throw std::runtime_error("Snapshot konnte nicht geschrieben werden");
    }
}

template <class T>
void readPod(std::istream& input, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    input.read(
        reinterpret_cast<char*>(&value),
        static_cast<std::streamsize>(sizeof(T)));
    if (!input) {
        throw std::runtime_error("Snapshot ist abgeschnitten");
    }
}

template <class T>
void writePodVector(std::ostream& output, const std::vector<T>& values) {
    const std::uint64_t size = values.size();
    writePod(output, size);
    if constexpr (std::is_trivially_copyable_v<T>) {
        if (!values.empty()) {
            output.write(
                reinterpret_cast<const char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(T)));
        }
    } else {
        for (const auto& value : values) {
            writePod(output, value);
        }
    }
    if (!output) {
        throw std::runtime_error("Snapshot-Vektor konnte nicht geschrieben werden");
    }
}

template <class T>
void readPodVector(std::istream& input, std::vector<T>& values) {
    std::uint64_t size = 0;
    readPod(input, size);
    if (size > 100'000'000ULL) {
        throw std::runtime_error("Snapshot-Vektor ist unplausibel groß");
    }
    values.resize(static_cast<std::size_t>(size));
    if constexpr (std::is_trivially_copyable_v<T>) {
        if (!values.empty()) {
            input.read(
                reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(T)));
        }
    } else {
        for (auto& value : values) {
            readPod(input, value);
        }
    }
    if (!input) {
        throw std::runtime_error("Snapshot-Vektor ist abgeschnitten");
    }
}

std::string jsonEscape(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (char value : text) {
        switch (value) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += value; break;
        }
    }
    return escaped;
}

double generatedGate(double value) {
    const double kernel =
        ag_signal_morpher_1ee27305a6aa_kernel::kernel(value);
    return std::clamp(
        0.5 * (1.0 + std::tanh(kernel)),
        0.05,
        0.95);
}

std::uint64_t mixHash(std::uint64_t hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

}  // namespace

struct PersistentNervousSystem::Neuron {
    PopulationRole role = PopulationRole::Excitatory;
    double somaMv = -65.0;
    double dendriteMv = -65.0;
    double gAmpa = 0.0;
    double gNmda = 0.0;
    double gGabaA = 0.0;
    double gGabaB = 0.0;
    double adaptationMv = 0.0;
    double homeostaticBiasMv = 0.0;
    double filteredRateHz = 0.0;
    double fastRateHz = 0.0;
    double slowBaselineHz = 0.0;
    double energy = 1.0;
    double excitability = 1.0;
    int refractorySteps = 0;
    std::uint64_t spikeCount = 0;
    bool active = true;
};

struct PersistentNervousSystem::Synapse {
    std::int64_t parentSynapse = -1;
    std::uint32_t pre = 0;
    std::uint32_t post = 0;
    ReceptorType receptor = ReceptorType::Ampa;
    double weight = 0.0;
    double consolidatedWeight = 0.0;
    double eligibility = 0.0;
    double resource = 1.0;
    double facilitation = 0.0;
    double usage = 0.0;
    std::uint32_t delaySteps = 1;
    std::uint64_t ageSteps = 0;
    bool active = true;
};

struct PersistentNervousSystem::AxonEvent {
    std::uint32_t synapse = 0;
    double amplitude = 0.0;
};

struct PersistentNervousSystem::Assembly {
    std::uint64_t id = 0;
    std::vector<double> prototype;
    double activation = 0.0;
    std::uint64_t observations = 0;
    std::uint64_t lastActiveStep = 0;
};

PersistentNervousSystem::~PersistentNervousSystem() = default;

int NervousSystemConfig::neuronCount() const {
    return sensoryNeurons + excitatoryNeurons + inhibitoryNeurons
        + contextNeurons + motorNeurons + modulatoryNeurons;
}

void NervousSystemConfig::validate() const {
    if (sensoryNeurons < 4
        || excitatoryNeurons < 2
        || inhibitoryNeurons < 1
        || contextNeurons < 1
        || motorNeurons < 4
        || modulatoryNeurons < 1
        || neuronCount() > 65536) {
        throw std::invalid_argument("Ungültige Populationsgrößen");
    }
    const std::array<double, 36> finiteValues{
        dtMs, connectionProbability, restingMv, resetMv, thresholdMv,
        tauSomaMs, tauDendriteMs, somaDendriteCoupling, baseCurrent,
        motorRateScaleHz, tauAmpaMs,
        tauNmdaMs, tauGabaAMs, tauGabaBMs, ampaReversalMv,
        nmdaReversalMv, gabaAReversalMv, gabaBReversalMv, refractoryMs,
        adaptationIncrementMv, adaptationTauMs, targetRateHz,
        homeostasisTauMs, homeostasisGain, eligibilityTauMs,
        eligibilityTransmissionGain, eligibilityIncrement, learningRate,
        consolidationRate, dopamineTauMs, acetylcholineTauMs,
        resourceRecoveryTauMs, facilitationTauMs,
        releaseProbability, structuralIntervalMs, assemblySimilarityThreshold};
    if (!std::all_of(
            finiteValues.begin(),
            finiteValues.end(),
            [](double value) { return std::isfinite(value); })) {
        throw std::invalid_argument("Nichtendliche Nervensystemparameter");
    }
    if (dtMs <= 0.0
        || connectionProbability < 0.0
        || connectionProbability > 1.0
        || thresholdMv <= restingMv
        || resetMv > restingMv
        || tauSomaMs <= 0.0
        || motorRateScaleHz <= 0.0
        || tauDendriteMs <= 0.0
        || tauAmpaMs <= 0.0
        || tauNmdaMs <= 0.0
        || tauGabaAMs <= 0.0
        || tauGabaBMs <= 0.0
        || refractoryMs < 0.0
        || adaptationTauMs <= 0.0
        || targetRateHz < 0.0
        || homeostasisTauMs <= 0.0
        || eligibilityTauMs <= 0.0
        || eligibilityTransmissionGain < 0.0
        || eligibilityIncrement < 0.0
        || dopamineTauMs <= 0.0
        || acetylcholineTauMs <= 0.0
        || resourceRecoveryTauMs <= 0.0
        || facilitationTauMs <= 0.0
        || releaseProbability < 0.0
        || releaseProbability > 1.0
        || energyRecoveryPerMs < 0.0
        || spikeEnergyCost < 0.0
        || transmissionEnergyCost < 0.0
        || structuralIntervalMs < dtMs
        || maximumNewSynapsesPerInterval < 0
        || maximumAssemblies < 1
        || assemblySimilarityThreshold < 0.0
        || assemblySimilarityThreshold > 1.0) {
        throw std::invalid_argument("Nervensystemparameter außerhalb des Bereichs");
    }
}

MechanismLibrary::MechanismLibrary() {
    entries_ = {
        {
            "generated_polarity_gate",
            "g=clip((1+tanh(K(phi)))/2,0.05,0.95)",
            "praesynaptische Freisetzungsstaerke",
            "phi in [-4,4], g in [0.05,0.95]",
            "RESET_LOCKED und Event-Gate-Ablationen Stufen 6-11",
            "integrated_experimental"},
        {
            "signed_local_eligibility",
            "e<-clip(e*exp(-dt/tau)+post*preTrace-pre*postTrace)",
            "lokaler Zustand jeder aktiven Synapse",
            "tau>0, |e|<=e_max",
            "Trace-essential Recall-XOR: 1.000 vs 0.486111 auf 12 Holdout-Netzen",
            "task_advantage_confirmed_stage18"},
        {
            "short_term_resource",
            "R<-R+(1-R)dt/tau_rec; release=u*R",
            "praesynaptische Vesikelressource",
            "R,u in [0,1]",
            "synthetische Invarianz- und Langzeittests",
            "integrated_experimental"},
        {
            "rate_homeostasis",
            "theta_h<-theta_h+eta*(rate-target)*dt",
            "neuronale Erregbarkeit",
            "target>=0, theta_h in [-12,12] mV",
            "Stabilitaets- und Stoerungstests",
            "integrated_experimental"},
        {
            "reward_modulated_consolidation",
            "dw=eta*dopamine*eligibility",
            "lokale Langzeitplastizitaet",
            "|w| begrenzt; Dale-Vorzeichen erhalten",
            "Closed-loop Softwareumwelt",
            "integrated_experimental"},
        {
            "structural_coactivity",
            "grow/prune=f(trace,usage,age,energy)",
            "rekurrente Topologie",
            "periodisch, deterministisch geseedet",
            "Struktur- und Snapshot-Regression",
            "integrated_experimental"},
        {
            "axonal_path_repair",
            "w_new<-w_consolidated(parent), delay_new<-max(1,delay_parent-1)",
            "inaktive, zuvor benutzte Sensor-Motor-Bahn",
            "|w_parent|>=0.20, usage_parent>=0.05, Endpunkte aktiv",
            "8/8 Holdout-Seeds: Funktionsverlust und >=70 Prozent Wiedergewinn",
            "functional_recovery_confirmed_stage18"},
        {
            "topographic_raw_projection",
            "sensory_channel->overlapping_excitatory_microassembly",
            "rohe multimodale Eingangsprojektion",
            "fanout=5, AMPA/NMDA, geseedete feste Topografie",
            "8/8 Holdout-Seeds; mittlere Transitionserkennung 77.3438 Prozent",
            "sequence_representation_confirmed_stage18"},
        {
            "evoked_state_representation",
            "r_evoked=r_fast-r_slow; p includes signed dendritic deviation",
            "Assembly- und Reaktionszustand",
            "tau_slow=5000 ms, signed dendritic state in [-1,1]",
            "8/8 Holdout-Seeds; Reaktivierung 0.907323, nach Schaden 0.852185",
            "stable_representation_confirmed_stage18"},
        {
            "competitive_temporal_assembly",
            "winner=max cosine(p_k,r); update winner or create if sim<threshold",
            "reizphasengebundener Assembly-Katalog",
            "maxAssemblies>=1, similarity in [0,1]",
            "8/8 Holdout-Seeds; im Mittel 6.125 getrennte Assemblies",
            "representation_catalog_confirmed_stage18"},
        {
            "restricted_cognitive_bridge",
            "C_t=pool(assemblies,recall,salience,needs,error); X_{t+1}=attention+intent+cue+reward",
            "funktionale Grenze zwischen persistentem Nervensystem und hoeherer KI",
            "64 neuronale und 64 recall-gebundene Synapsenpools; kein Einzelzellzugriff",
            "8/8 Holdout-Seeds: 1.0 vs 0.515625 ohne Trace vs 0.5 ohne Nervensystem",
            "ai_coupling_confirmed_stage19"},
        {
            "salience_gated_eligibility_write",
            "e<-decay(e)+1_external_event*local_causality",
            "lokale Eligibility jeder Synapse",
            "write bei Reiz, Recall, Neuheit oder Reward; reiner Zerfall in Leerzeit",
            "Stufe 21: episodisches Signal, Konsolidierung, 99.9955 Prozent Vergessen",
            "multiscale_memory_confirmed_stage21"},
        {
            "sparse_scalable_topology",
            "out_degree=round(p*(N-1)); unique sampled targets for N>2048",
            "Initialisierung grosser rekurrenter Netze",
            "N<=65536, Dale-konforme Rezeptor- und Gewichtszuweisung",
            "Stufe 22: Snapshot- und Sicherheitstest bis 65536 Neuronen",
            "scaling_validated_stage22"},
        {
            "reward_adaptive_cognitive_policy",
            "a=sign(w*phi(C)); w<-decay(w)+eta*reward_inferred_target*phi(C)",
            "hoeherer Kern ueber beschraenkter Cognitive Bridge",
            "begrenzte Gewichte, Exploration, Entscheidung vor Konsequenz",
            "Stufe 20: 6/8 offene Welten und positiver eingefrorener G5-Transfer",
            "open_lifeworld_confirmed_stage20"}};
}

const std::vector<MechanismDescriptor>& MechanismLibrary::entries() const {
    return entries_;
}

void MechanismLibrary::writeJson(const std::filesystem::path& path) const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Mechanismenbibliothek konnte nicht geschrieben werden");
    }
    output << "{\n  \"schema\": \"agns-mechanisms-v1\",\n  \"mechanisms\": [\n";
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const auto& entry = entries_[index];
        output << "    {\n"
            << "      \"id\": \"" << jsonEscape(entry.id) << "\",\n"
            << "      \"formula\": \"" << jsonEscape(entry.formula) << "\",\n"
            << "      \"placement\": \"" << jsonEscape(entry.placement) << "\",\n"
            << "      \"parameter_range\": \"" << jsonEscape(entry.parameterRange) << "\",\n"
            << "      \"evidence\": \"" << jsonEscape(entry.evidence) << "\",\n"
            << "      \"status\": \"" << jsonEscape(entry.status) << "\"\n"
            << "    }" << (index + 1 == entries_.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

PersistentNervousSystem::PersistentNervousSystem(
    NervousSystemConfig config)
    : config_(std::move(config)), random_(config_.seed) {
    config_.validate();
    createNeurons();
    createSynapses();
    spikes_.assign(static_cast<std::size_t>(config_.neuronCount()), 0);
    preTrace_.assign(static_cast<std::size_t>(config_.neuronCount()), 0.0);
    postTrace_.assign(static_cast<std::size_t>(config_.neuronCount()), 0.0);
    assemblyAccumulator_.assign(
        static_cast<std::size_t>(config_.neuronCount()),
        0.0);
    assemblyBaseline_.assign(
        static_cast<std::size_t>(config_.neuronCount()),
        config_.restingMv);
    axonQueue_.resize(33);
    updateMetrics();
}

const NervousSystemConfig& PersistentNervousSystem::config() const {
    return config_;
}

const NervousSystemMetrics& PersistentNervousSystem::metrics() const {
    return metrics_;
}

PopulationRole PersistentNervousSystem::roleForIndex(int index) const {
    int boundary = config_.sensoryNeurons;
    if (index < boundary) return PopulationRole::Sensory;
    boundary += config_.excitatoryNeurons;
    if (index < boundary) return PopulationRole::Excitatory;
    boundary += config_.inhibitoryNeurons;
    if (index < boundary) return PopulationRole::Inhibitory;
    boundary += config_.contextNeurons;
    if (index < boundary) return PopulationRole::Context;
    boundary += config_.motorNeurons;
    if (index < boundary) return PopulationRole::Motor;
    return PopulationRole::Modulatory;
}

void PersistentNervousSystem::createNeurons() {
    neurons_.resize(static_cast<std::size_t>(config_.neuronCount()));
    std::uniform_real_distribution<double> jitter(-1.0, 1.0);
    for (int index = 0; index < config_.neuronCount(); ++index) {
        auto& neuron = neurons_[static_cast<std::size_t>(index)];
        neuron.role = roleForIndex(index);
        neuron.somaMv = config_.restingMv + jitter(random_);
        neuron.dendriteMv = config_.restingMv + jitter(random_);
        neuron.energy = 0.9 + 0.1 * (0.5 + 0.5 * jitter(random_));
    }
}

bool PersistentNervousSystem::connectionExists(int pre, int post) const {
    return std::any_of(
        synapses_.begin(),
        synapses_.end(),
        [pre, post](const Synapse& synapse) {
            return synapse.active
                && synapse.pre == static_cast<std::uint32_t>(pre)
                && synapse.post == static_cast<std::uint32_t>(post);
        });
}

void PersistentNervousSystem::createSynapses() {
    synapses_.clear();
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> excitatoryWeight(0.035, 0.11);
    std::uniform_real_distribution<double> inhibitoryWeight(0.045, 0.13);
    std::uniform_int_distribution<int> delay(1, 12);
    const int count = config_.neuronCount();
    const auto appendSynapse = [&](int pre, int post) {
        Synapse synapse;
        synapse.pre = static_cast<std::uint32_t>(pre);
        synapse.post = static_cast<std::uint32_t>(post);
        synapse.delaySteps = static_cast<std::uint32_t>(delay(random_));
        const bool inhibitory =
            roleForIndex(pre) == PopulationRole::Inhibitory;
        if (inhibitory) {
            synapse.weight = -inhibitoryWeight(random_);
            synapse.receptor =
                unit(random_) < 0.8
                    ? ReceptorType::GabaA
                    : ReceptorType::GabaB;
        } else {
            synapse.weight = excitatoryWeight(random_);
            synapse.receptor =
                unit(random_) < 0.75
                    ? ReceptorType::Ampa
                    : ReceptorType::Nmda;
        }
        if (roleForIndex(pre) == PopulationRole::Modulatory) {
            synapse.receptor = ReceptorType::Modulatory;
            synapse.weight = std::abs(synapse.weight);
        }
        if (roleForIndex(post) == PopulationRole::Motor) {
            synapse.weight *= 0.15;
        }
        synapse.consolidatedWeight = synapse.weight;
        synapses_.push_back(synapse);
    };
    if (count <= 2048) {
        for (int pre = 0; pre < count; ++pre) {
            for (int post = 0; post < count; ++post) {
                if (pre == post
                    || unit(random_) > config_.connectionProbability) {
                    continue;
                }
                appendSynapse(pre, post);
            }
        }
    } else {
        const int outgoingPerNeuron = std::clamp(
            static_cast<int>(std::llround(
                config_.connectionProbability
                * static_cast<double>(count - 1))),
            0,
            count - 1);
        synapses_.reserve(
            static_cast<std::size_t>(count)
            * static_cast<std::size_t>(outgoingPerNeuron + 1));
        std::uniform_int_distribution<int> postChoice(0, count - 1);
        for (int pre = 0; pre < count; ++pre) {
            std::unordered_set<int> selected;
            selected.reserve(
                static_cast<std::size_t>(outgoingPerNeuron * 2 + 1));
            while (static_cast<int>(selected.size()) < outgoingPerNeuron) {
                const int post = postChoice(random_);
                if (post == pre || !selected.insert(post).second) continue;
                appendSynapse(pre, post);
            }
        }
    }

    // Deterministische topografische Sensorprojektion: Jeder Rohkanal
    // rekrutiert eine kleine, überlappende exzitatorische Mikroassembly.
    // Sie ersetzt keine lernbare Verbindung, sondern stellt sicher, dass
    // unterschiedliche Modalitätsereignisse überhaupt interne Kausalpfade
    // erreichen.
    constexpr int sensoryFanout = 5;
    for (int sensory = 0; sensory < config_.sensoryNeurons; ++sensory) {
        for (int branch = 0; branch < sensoryFanout; ++branch) {
            const int offset =
                (sensory * 7 + branch * 3) % config_.excitatoryNeurons;
            const int post = config_.sensoryNeurons + offset;
            if (count <= 2048 && connectionExists(sensory, post)) continue;
            Synapse projection;
            projection.pre = static_cast<std::uint32_t>(sensory);
            projection.post = static_cast<std::uint32_t>(post);
            projection.receptor =
                branch == sensoryFanout - 1
                    ? ReceptorType::Nmda
                    : ReceptorType::Ampa;
            projection.weight = 0.50;
            projection.consolidatedWeight = projection.weight;
            projection.delaySteps = static_cast<std::uint32_t>(1 + branch % 3);
            synapses_.push_back(projection);
        }
    }

    const int motorStart =
        config_.sensoryNeurons + config_.excitatoryNeurons
        + config_.inhibitoryNeurons + config_.contextNeurons;
    const int motorHalf = config_.motorNeurons / 2;
    for (int side = 0; side < 2; ++side) {
        const int sensory = side;
        for (int motor = 0; motor < motorHalf; ++motor) {
            const int post = motorStart + side * motorHalf + motor;
            if (post >= motorStart + config_.motorNeurons) {
                continue;
            }
            Synapse scaffold;
            scaffold.pre = static_cast<std::uint32_t>(sensory);
            scaffold.post = static_cast<std::uint32_t>(post);
            scaffold.receptor = ReceptorType::Ampa;
            scaffold.weight = 1.20;
            scaffold.consolidatedWeight = scaffold.weight;
            scaffold.delaySteps = 2;
            synapses_.push_back(scaffold);
        }
    }
    rebuildOutgoing();
}

void PersistentNervousSystem::rebuildOutgoing() {
    outgoing_.assign(
        static_cast<std::size_t>(config_.neuronCount()),
        {});
    for (std::size_t index = 0; index < synapses_.size(); ++index) {
        const auto& synapse = synapses_[index];
        if (synapse.active && synapse.pre < outgoing_.size()) {
            outgoing_[synapse.pre].push_back(index);
        }
    }
}

void PersistentNervousSystem::encodeSensors(
    const SensorFrame& frame,
    std::vector<double>& somaDrive,
    std::vector<double>& dendriteDrive) const {
    const int sensory = config_.sensoryNeurons;
    auto addChannel = [&](int channel, double value, double scale) {
        if (sensory <= 0) return;
        const std::size_t index =
            static_cast<std::size_t>((channel % sensory + sensory) % sensory);
        somaDrive[index] += scale * std::clamp(value, -1.0, 1.0);
    };
    for (std::size_t index = 0; index < frame.visionEvents.size(); ++index) {
        addChannel(
            static_cast<int>(index),
            frame.visionEvents[index],
            32.0);
    }
    for (std::size_t index = 0; index < frame.audioSamples.size(); ++index) {
        addChannel(
            4 + static_cast<int>(index),
            frame.audioSamples[index],
            24.0);
    }
    for (std::size_t index = 0; index < frame.touch.size(); ++index) {
        addChannel(
            8 + static_cast<int>(index),
            frame.touch[index],
            28.0);
    }
    for (std::uint8_t byte : frame.textBytes) {
        for (int bit = 0; bit < 8; ++bit) {
            const double value = ((byte >> bit) & 1U) ? 1.0 : -0.35;
            addChannel(12 + bit, value, 18.0);
        }
    }
    addChannel(sensory - 2, frame.temperature, 18.0);
    addChannel(
        sensory - 1,
        2.0 * std::clamp(frame.internalEnergy, 0.0, 1.0) - 1.0,
        16.0);

    const int contextStart =
        config_.sensoryNeurons + config_.excitatoryNeurons
        + config_.inhibitoryNeurons;
    for (int offset = 0; offset < config_.contextNeurons; ++offset) {
        dendriteDrive[static_cast<std::size_t>(contextStart + offset)] +=
            8.0 * frame.novelty
            * std::sin((offset + 1) * 0.5);
        if (!frame.contextEvents.empty()) {
            const auto channel = static_cast<std::size_t>(
                offset % static_cast<int>(frame.contextEvents.size()));
            dendriteDrive[static_cast<std::size_t>(contextStart + offset)] +=
                10.0
                * std::clamp(frame.contextEvents[channel], -1.0, 1.0)
                * ((offset / static_cast<int>(frame.contextEvents.size())) % 2
                    == 0 ? 1.0 : -0.35);
        }
    }
    const int modStart =
        config_.neuronCount() - config_.modulatoryNeurons;
    for (int offset = 0; offset < config_.modulatoryNeurons; ++offset) {
        somaDrive[static_cast<std::size_t>(modStart + offset)] +=
            12.0 * frame.reward
            + 7.0 * frame.novelty;
    }
}

void PersistentNervousSystem::deliverAxonEvents(
    const SensorFrame& frame) {
    const bool plasticityWrite = hasPlasticityWriteSignal(frame);
    auto& events = axonQueue_[
        static_cast<std::size_t>(metrics_.step % axonQueue_.size())];
    for (const auto& event : events) {
        if (event.synapse >= synapses_.size()) {
            continue;
        }
        Synapse& synapse = synapses_[event.synapse];
        if (!synapse.active || synapse.post >= neurons_.size()) {
            continue;
        }
        auto& post = neurons_[synapse.post];
        if (config_.eligibilityMemoryEnabled && plasticityWrite) {
            const double localPostState = std::tanh(
                (post.dendriteMv - config_.restingMv) / 8.0);
            synapse.eligibility = std::clamp(
                synapse.eligibility
                    + 0.02 * config_.eligibilityIncrement
                        * localPostState,
                -4.0,
                4.0);
        }
        switch (synapse.receptor) {
            case ReceptorType::Ampa:
                post.gAmpa += event.amplitude;
                break;
            case ReceptorType::Nmda:
                post.gNmda += event.amplitude;
                break;
            case ReceptorType::GabaA:
                post.gGabaA += event.amplitude;
                break;
            case ReceptorType::GabaB:
                post.gGabaB += event.amplitude;
                break;
            case ReceptorType::Modulatory:
                acetylcholine_ += 0.02 * event.amplitude;
                break;
        }
        if (config_.energyRegulationEnabled) {
            post.energy = std::max(
                0.0,
                post.energy - config_.transmissionEnergyCost);
        }
        ++metrics_.totalTransmissions;
    }
    events.clear();
}

void PersistentNervousSystem::updateNeurons(
    const std::vector<double>& somaDrive,
    const std::vector<double>& dendriteDrive) {
    const double ampaDecay = std::exp(-config_.dtMs / config_.tauAmpaMs);
    const double nmdaDecay = std::exp(-config_.dtMs / config_.tauNmdaMs);
    const double gabaADecay = std::exp(-config_.dtMs / config_.tauGabaAMs);
    const double gabaBDecay = std::exp(-config_.dtMs / config_.tauGabaBMs);
    const double adaptationDecay =
        std::exp(-config_.dtMs / config_.adaptationTauMs);
    const int refractoryDuration = std::max(
        1,
        static_cast<int>(std::ceil(
            config_.refractoryMs / config_.dtMs)));
    std::fill(spikes_.begin(), spikes_.end(), static_cast<std::uint8_t>(0));
    for (std::size_t index = 0; index < neurons_.size(); ++index) {
        auto& neuron = neurons_[index];
        if (!neuron.active) {
            neuron.somaMv = config_.restingMv;
            neuron.dendriteMv = config_.restingMv;
            neuron.gAmpa = 0.0;
            neuron.gNmda = 0.0;
            neuron.gGabaA = 0.0;
            neuron.gGabaB = 0.0;
            neuron.filteredRateHz = 0.0;
            neuron.fastRateHz = 0.0;
            continue;
        }
        neuron.gAmpa *= ampaDecay;
        neuron.gNmda *= nmdaDecay;
        neuron.gGabaA *= gabaADecay;
        neuron.gGabaB *= gabaBDecay;
        neuron.adaptationMv *= adaptationDecay;
        const double magnesiumBlock =
            1.0 / (1.0 + 0.28 * std::exp(-0.062 * neuron.dendriteMv));
        const double synapticDrive =
            neuron.gAmpa * (config_.ampaReversalMv - neuron.dendriteMv)
            + neuron.gNmda * magnesiumBlock
                * (config_.nmdaReversalMv - neuron.dendriteMv)
            + neuron.gGabaA
                * (config_.gabaAReversalMv - neuron.dendriteMv)
            + neuron.gGabaB
                * (config_.gabaBReversalMv - neuron.dendriteMv);
        neuron.dendriteMv +=
            config_.dtMs / config_.tauDendriteMs
            * (
                config_.restingMv - neuron.dendriteMv
                + synapticDrive
                + dendriteDrive[index]
                + config_.somaDendriteCoupling
                    * (neuron.somaMv - neuron.dendriteMv));
        if (neuron.refractorySteps > 0) {
            --neuron.refractorySteps;
            neuron.somaMv = config_.resetMv;
            continue;
        }
        const double energyFactor =
            config_.energyRegulationEnabled
                ? std::clamp(neuron.energy, 0.15, 1.0)
                : 1.0;
        neuron.somaMv +=
            config_.dtMs / config_.tauSomaMs
            * (
                config_.restingMv - neuron.somaMv
                + somaDrive[index]
                + config_.baseCurrent * neuron.excitability
                + config_.somaDendriteCoupling
                    * (neuron.dendriteMv - neuron.somaMv));
        const double threshold =
            config_.thresholdMv
            + neuron.adaptationMv
            + neuron.homeostaticBiasMv
            + (1.0 - energyFactor) * 8.0;
        if (neuron.somaMv >= threshold) {
            spikes_[index] = 1;
            neuron.somaMv = config_.resetMv;
            neuron.refractorySteps = refractoryDuration;
            neuron.adaptationMv += config_.adaptationIncrementMv;
            ++neuron.spikeCount;
            ++metrics_.totalSpikes;
            if (config_.energyRegulationEnabled) {
                neuron.energy = std::max(
                    0.0,
                    neuron.energy - config_.spikeEnergyCost);
            }
        }
    }
}

void PersistentNervousSystem::scheduleSpikeEvents() {
    for (std::size_t pre = 0; pre < spikes_.size(); ++pre) {
        if (!spikes_[pre] || !neurons_[pre].active) {
            continue;
        }
        for (std::size_t synapseIndex : outgoing_[pre]) {
            auto& synapse = synapses_[synapseIndex];
            if (!synapse.active) continue;
            const double utilization = std::clamp(
                config_.releaseProbability + synapse.facilitation,
                0.01,
                0.95);
            const double gate =
                config_.generatedOperatorEnabled
                    ? generatedGate(
                        config_.eligibilityMemoryEnabled
                            ? synapse.eligibility
                            : 0.0)
                    : 1.0;
            const double eligibilityModulation =
                config_.eligibilityMemoryEnabled
                    ? std::clamp(
                        std::exp(
                            config_.eligibilityTransmissionGain
                            * std::tanh(synapse.eligibility)),
                        0.05,
                        8.00)
                    : 1.0;
            const double amplitude =
                std::abs(synapse.weight)
                * utilization
                * synapse.resource
                * gate
                * eligibilityModulation;
            const std::size_t slot = static_cast<std::size_t>(
                (metrics_.step + synapse.delaySteps)
                % axonQueue_.size());
            axonQueue_[slot].push_back(AxonEvent{
                static_cast<std::uint32_t>(synapseIndex),
                amplitude});
            if (config_.shortTermPlasticityEnabled) {
                synapse.resource *= (1.0 - utilization);
                synapse.facilitation = std::clamp(
                    synapse.facilitation
                        + 0.12 * (1.0 - synapse.facilitation),
                    0.0,
                    0.8);
            }
            synapse.usage += 1.0;
        }
    }
}

void PersistentNervousSystem::updatePlasticity(const SensorFrame& frame) {
    const double traceDecay = std::exp(-config_.dtMs / 20.0);
    const double eligibilityDecay =
        std::exp(-config_.dtMs / config_.eligibilityTauMs);
    const double resourceRecovery =
        config_.dtMs / config_.resourceRecoveryTauMs;
    const double facilitationDecay =
        std::exp(-config_.dtMs / config_.facilitationTauMs);
    const bool plasticityWrite = hasPlasticityWriteSignal(frame);
    for (std::size_t index = 0; index < neurons_.size(); ++index) {
        preTrace_[index] *= traceDecay;
        postTrace_[index] *= traceDecay;
    }
    dopamine_ =
        dopamine_ * std::exp(-config_.dtMs / config_.dopamineTauMs)
        + 0.6 * frame.reward;
    acetylcholine_ =
        acetylcholine_ * std::exp(
            -config_.dtMs / config_.acetylcholineTauMs)
        + 0.25 * std::abs(frame.novelty);
    dopamine_ = std::clamp(dopamine_, -2.0, 2.0);
    acetylcholine_ = std::clamp(acetylcholine_, 0.0, 2.0);
    for (auto& synapse : synapses_) {
        if (!synapse.active) continue;
        const std::size_t pre = synapse.pre;
        const std::size_t post = synapse.post;
        const double causal = spikes_[post] * preTrace_[pre];
        const double antiCausal = spikes_[pre] * postTrace_[post];
        synapse.eligibility = config_.eligibilityMemoryEnabled
            ? std::clamp(
                synapse.eligibility * eligibilityDecay
                    + (plasticityWrite ? config_.eligibilityIncrement : 0.0)
                        * (causal - antiCausal),
                -4.0,
                4.0)
            : 0.0;
        synapse.resource = std::clamp(
            synapse.resource
                + resourceRecovery * (1.0 - synapse.resource),
            0.0,
            1.0);
        synapse.facilitation *= facilitationDecay;
        synapse.usage *= std::exp(-config_.dtMs / 5000.0);
        ++synapse.ageSteps;
        if (config_.longTermPlasticityEnabled) {
            const bool inhibitory =
                neurons_[pre].role == PopulationRole::Inhibitory;
            const double sign = inhibitory ? -1.0 : 1.0;
            const bool sensoryProjection =
                neurons_[pre].role == PopulationRole::Sensory;
            const bool sensoryMotor =
                sensoryProjection
                && neurons_[synapse.post].role == PopulationRole::Motor;
            const double plasticitySignal = sensoryMotor
                ? std::abs(synapse.eligibility)
                : synapse.eligibility;
            double magnitude = std::abs(synapse.weight);
            magnitude +=
                config_.learningRate
                * dopamine_
                * plasticitySignal
                * (sensoryMotor ? 100.0 : 1.0);
            magnitude +=
                0.02 * config_.learningRate
                * acetylcholine_
                * std::abs(synapse.eligibility);
            magnitude = std::clamp(
                magnitude,
                0.001,
                sensoryProjection ? 1.20 : 0.35);
            synapse.weight = sign * magnitude;
            if (dopamine_ > 0.0) {
                synapse.consolidatedWeight +=
                    config_.consolidationRate
                    * dopamine_
                    * (synapse.weight - synapse.consolidatedWeight);
            } else {
                synapse.weight +=
                    config_.consolidationRate
                    * (synapse.consolidatedWeight - synapse.weight);
            }
            if (!inhibitory) {
                synapse.weight = std::abs(synapse.weight);
            } else {
                synapse.weight = -std::abs(synapse.weight);
            }
        }
    }
    for (std::size_t index = 0; index < neurons_.size(); ++index) {
        preTrace_[index] += spikes_[index];
        postTrace_[index] += spikes_[index];
    }
}

void PersistentNervousSystem::updateAssemblies(const SensorFrame& frame) {
    const auto nonzero = [](const auto& values) {
        return std::any_of(values.begin(), values.end(), [](const auto value) {
            return std::abs(static_cast<double>(value)) > 1e-12;
        });
    };
    const bool stimulusPresent =
        nonzero(frame.visionEvents)
        || nonzero(frame.audioSamples)
        || nonzero(frame.touch)
        || !frame.textBytes.empty();
    std::uint64_t signature = 14695981039346656037ULL;
    const auto hashVector = [&signature](const auto& values) {
        const auto size = values.size();
        signature = mixHash(signature, &size, sizeof(size));
        if (!values.empty()) {
            signature = mixHash(
                signature,
                values.data(),
                values.size() * sizeof(values.front()));
        }
    };
    hashVector(frame.visionEvents);
    hashVector(frame.audioSamples);
    hashVector(frame.touch);
    hashVector(frame.textBytes);
    hashVector(frame.contextEvents);
    if (!stimulusPresent) {
        stimulusWasPresent_ = false;
        stimulusAgeSteps_ = 0;
        std::fill(
            assemblyAccumulator_.begin(),
            assemblyAccumulator_.end(),
            0.0);
    } else if (!stimulusWasPresent_ || signature != stimulusSignature_) {
        stimulusWasPresent_ = true;
        stimulusSignature_ = signature;
        stimulusAgeSteps_ = 1;
        std::fill(
            assemblyAccumulator_.begin(),
            assemblyAccumulator_.end(),
            0.0);
        for (std::size_t index = 0; index < neurons_.size(); ++index) {
            assemblyBaseline_[index] = neurons_[index].dendriteMv;
        }
    } else {
        ++stimulusAgeSteps_;
    }
    const int samplingAge = std::max(
        2,
        static_cast<int>(std::round(22.0 / config_.dtMs)));
    const int first =
        config_.sensoryNeurons;
    const int last =
        config_.sensoryNeurons + config_.excitatoryNeurons
        + config_.inhibitoryNeurons + config_.contextNeurons;
    std::vector<double> pattern(
        static_cast<std::size_t>(last - first),
        0.0);
    int active = 0;
    for (int neuron = first; neuron < last; ++neuron) {
        const auto index = static_cast<std::size_t>(neuron);
        const double evokedRate = std::max(
            0.0,
            neurons_[index].filteredRateHz - neurons_[index].slowBaselineHz);
        const double dendriticActivation = std::clamp(
            (neurons_[index].dendriteMv - assemblyBaseline_[index]) / 4.0,
            -1.0,
            1.0);
        const double rateActivation = std::clamp(
            evokedRate / std::max(1.0, config_.targetRateHz),
            0.0,
            1.0);
        const double instantaneous = spikes_[index]
            ? 1.0
            : (
                std::abs(dendriticActivation) > rateActivation
                    ? dendriticActivation
                    : rateActivation);
        assemblyAccumulator_[index] += instantaneous;
        const double activation =
            assemblyAccumulator_[index]
            / static_cast<double>(std::max(1, stimulusAgeSteps_));
        pattern[static_cast<std::size_t>(neuron - first)] = activation;
        if (std::abs(activation) > 0.02) {
            ++active;
        }
    }
    metrics_.activeAssembly = -1;
    for (auto& assembly : assemblies_) {
        assembly.activation *= 0.96;
    }
    if (
        !stimulusPresent
        || stimulusAgeSteps_ != samplingAge
        || active < 2) {
        return;
    }
    double bestSimilarity = -1.0;
    int bestIndex = -1;
    for (std::size_t index = 0; index < assemblies_.size(); ++index) {
        const auto& prototype = assemblies_[index].prototype;
        double dot = 0.0;
        double normPattern = 0.0;
        double normPrototype = 0.0;
        for (std::size_t feature = 0; feature < pattern.size(); ++feature) {
            dot += pattern[feature] * prototype[feature];
            normPattern += pattern[feature] * pattern[feature];
            normPrototype += prototype[feature] * prototype[feature];
        }
        const double similarity =
            dot / (std::sqrt(normPattern * normPrototype) + 1e-12);
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity;
            bestIndex = static_cast<int>(index);
        }
    }
    if (
        bestIndex < 0
        || (
            bestSimilarity < config_.assemblySimilarityThreshold
            && assemblies_.size()
                < static_cast<std::size_t>(config_.maximumAssemblies))) {
        Assembly assembly;
        assembly.id =
            assemblies_.empty() ? 1 : assemblies_.back().id + 1;
        assembly.prototype = pattern;
        assembly.activation = 1.0;
        assembly.observations = 1;
        assembly.lastActiveStep = metrics_.step;
        assemblies_.push_back(std::move(assembly));
        metrics_.activeAssembly =
            static_cast<int>(assemblies_.size() - 1);
    } else {
        auto& assembly = assemblies_[static_cast<std::size_t>(bestIndex)];
        const double rate =
            1.0 / static_cast<double>(std::min<std::uint64_t>(
                assembly.observations + 1,
                100));
        for (std::size_t feature = 0; feature < pattern.size(); ++feature) {
            assembly.prototype[feature] +=
                rate * (pattern[feature] - assembly.prototype[feature]);
        }
        assembly.activation = 1.0;
        ++assembly.observations;
        assembly.lastActiveStep = metrics_.step;
        metrics_.activeAssembly = bestIndex;
    }
}

void PersistentNervousSystem::updateHomeostasisAndEnergy() {
    const double rateAlpha =
        1.0 - std::exp(-config_.dtMs / config_.homeostasisTauMs);
    for (std::size_t index = 0; index < neurons_.size(); ++index) {
        auto& neuron = neurons_[index];
        if (!neuron.active) {
            neuron.filteredRateHz = 0.0;
            continue;
        }
        const double instantaneousRate =
            spikes_[index] ? 1000.0 / config_.dtMs : 0.0;
        const double fastAlpha =
            1.0 - std::exp(-config_.dtMs / 20.0);
        neuron.fastRateHz +=
            fastAlpha * (instantaneousRate - neuron.fastRateHz);
        neuron.filteredRateHz +=
            rateAlpha * (instantaneousRate - neuron.filteredRateHz);
        const double baselineAlpha =
            1.0 - std::exp(-config_.dtMs / 5000.0);
        neuron.slowBaselineHz += baselineAlpha
            * (neuron.filteredRateHz - neuron.slowBaselineHz);
        if (config_.homeostasisEnabled) {
            neuron.homeostaticBiasMv = std::clamp(
                neuron.homeostaticBiasMv
                    + config_.homeostasisGain
                        * (neuron.filteredRateHz - config_.targetRateHz)
                        * config_.dtMs,
                -12.0,
                12.0);
        }
        if (config_.energyRegulationEnabled) {
            neuron.energy = std::clamp(
                neuron.energy + config_.energyRecoveryPerMs,
                0.0,
                1.0);
        } else {
            neuron.energy = 1.0;
        }
    }
}

void PersistentNervousSystem::updateStructuralPlasticity() {
    if (!config_.structuralPlasticityEnabled) return;
    const std::uint64_t interval = static_cast<std::uint64_t>(std::max(
        1.0,
        std::round(config_.structuralIntervalMs / config_.dtMs)));
    if (metrics_.step == 0
        || metrics_.step - lastStructuralStep_ < interval) {
        return;
    }
    lastStructuralStep_ = metrics_.step;
    int pruned = 0;
    for (auto& synapse : synapses_) {
        if (!synapse.active) continue;
        if (
            synapse.ageSteps > interval * 2
            && synapse.usage < config_.pruneUsageThreshold
            && std::abs(synapse.weight)
                < config_.pruneWeightThreshold) {
            synapse.active = false;
            ++pruned;
        }
    }
    std::uniform_int_distribution<int> preChoice(
        0,
        config_.neuronCount() - config_.modulatoryNeurons - 1);
    std::uniform_int_distribution<int> postChoice(
        config_.sensoryNeurons,
        config_.neuronCount() - config_.modulatoryNeurons - 1);
    std::uniform_int_distribution<int> delayChoice(1, 12);
    int grown = 0;
    const std::size_t dormantCount = synapses_.size();
    for (std::size_t index = 0;
         index < dormantCount
            && grown < config_.maximumNewSynapsesPerInterval;
         ++index) {
        const auto& dormant = synapses_[index];
        if (dormant.active
            || dormant.pre >= neurons_.size()
            || dormant.post >= neurons_.size()
            || !neurons_[dormant.pre].active
            || !neurons_[dormant.post].active
            || std::abs(dormant.consolidatedWeight) < 0.20
            || dormant.usage < 0.05
            || connectionExists(
                static_cast<int>(dormant.pre),
                static_cast<int>(dormant.post))) {
            continue;
        }
        Synapse replacement;
        replacement.parentSynapse = static_cast<std::int64_t>(index);
        replacement.pre = dormant.pre;
        replacement.post = dormant.post;
        replacement.receptor = dormant.receptor;
        replacement.weight = dormant.consolidatedWeight;
        replacement.consolidatedWeight = replacement.weight;
        replacement.delaySteps = static_cast<std::uint32_t>(
            std::max(1, static_cast<int>(dormant.delaySteps) - 1));
        synapses_.push_back(replacement);
        ++grown;
    }
    int attempts = 0;
    while (
        grown < config_.maximumNewSynapsesPerInterval
        && attempts < config_.maximumNewSynapsesPerInterval * 40) {
        ++attempts;
        const int pre = preChoice(random_);
        const int post = postChoice(random_);
        if (pre == post || connectionExists(pre, post)) continue;
        const double coactivity =
            preTrace_[static_cast<std::size_t>(pre)]
            * postTrace_[static_cast<std::size_t>(post)];
        const bool sensoryMotor =
            roleForIndex(pre) == PopulationRole::Sensory
            && roleForIndex(post) == PopulationRole::Motor;
        const double repairDrive = sensoryMotor
            ? 0.20
                + 0.15 * std::abs(dopamine_)
                + 0.10 * acetylcholine_
            : 0.0;
        std::uniform_real_distribution<double> acceptance(0.0, 1.0);
        if (acceptance(random_) > std::clamp(
                0.05 + 0.2 * coactivity + repairDrive,
                0.0,
                0.9)) {
            continue;
        }
        Synapse synapse;
        synapse.pre = static_cast<std::uint32_t>(pre);
        synapse.post = static_cast<std::uint32_t>(post);
        synapse.delaySteps =
            static_cast<std::uint32_t>(delayChoice(random_));
        const bool inhibitory =
            roleForIndex(pre) == PopulationRole::Inhibitory;
        synapse.weight = inhibitory
            ? -0.035
            : (sensoryMotor ? 0.30 : 0.03);
        synapse.consolidatedWeight = synapse.weight;
        synapse.receptor =
            inhibitory ? ReceptorType::GabaA : ReceptorType::Ampa;
        synapses_.push_back(synapse);
        ++grown;
    }
    metrics_.structuralGrowth += grown;
    metrics_.structuralPruning += pruned;
    rebuildOutgoing();
}

MotorAction PersistentNervousSystem::decodeAction() const {
    MotorAction action;
    const int motorStart =
        config_.sensoryNeurons + config_.excitatoryNeurons
        + config_.inhibitoryNeurons + config_.contextNeurons;
    const int half = config_.motorNeurons / 2;
    double left = 0.0;
    double right = 0.0;
    for (int offset = 0; offset < config_.motorNeurons; ++offset) {
        const double rate = neurons_[
            static_cast<std::size_t>(motorStart + offset)].fastRateHz;
        if (offset < half) left += rate;
        else right += rate;
    }
    left /= std::max(1, half);
    right /= std::max(1, config_.motorNeurons - half);
    double leftPotential = 0.0;
    double rightPotential = 0.0;
    for (int offset = 0; offset < config_.motorNeurons; ++offset) {
        const auto& neuron = neurons_[
            static_cast<std::size_t>(motorStart + offset)];
        const double normalized = std::clamp(
            (neuron.dendriteMv - config_.restingMv)
                / (config_.thresholdMv - config_.restingMv),
            0.0,
            1.0);
        if (offset < half) leftPotential += normalized;
        else rightPotential += normalized;
    }
    leftPotential /= std::max(1, half);
    rightPotential /= std::max(1, config_.motorNeurons - half);
    action.movement = std::tanh(
        (right - left) / config_.motorRateScaleHz
            + 0.25 * (rightPotential - leftPotential));
    action.confidence =
        std::clamp((left + right) / 40.0, 0.0, 1.0);
    action.attention =
        std::clamp(acetylcholine_ / 2.0, 0.0, 1.0);
    const int modStart =
        config_.neuronCount() - config_.modulatoryNeurons;
    double modRate = 0.0;
    for (int index = modStart; index < config_.neuronCount(); ++index) {
        modRate += neurons_[static_cast<std::size_t>(index)].filteredRateHz;
    }
    action.vocalization = std::tanh(
        modRate / std::max(1, config_.modulatoryNeurons) / 20.0);
    return action;
}

void PersistentNervousSystem::updateMetrics() {
    metrics_.activeSynapses = 0;
    metrics_.meanEligibility = 0.0;
    metrics_.meanResource = 0.0;
    metrics_.meanRateHz = 0.0;
    metrics_.meanEnergy = 0.0;
    metrics_.finite = true;
    for (const auto& synapse : synapses_) {
        if (!synapse.active) continue;
        ++metrics_.activeSynapses;
        metrics_.meanEligibility += synapse.eligibility;
        metrics_.meanResource += synapse.resource;
        metrics_.finite =
            metrics_.finite
            && std::isfinite(synapse.weight)
            && std::isfinite(synapse.eligibility)
            && std::isfinite(synapse.resource);
    }
    if (metrics_.activeSynapses > 0) {
        metrics_.meanEligibility /= metrics_.activeSynapses;
        metrics_.meanResource /= metrics_.activeSynapses;
    }
    for (const auto& neuron : neurons_) {
        metrics_.meanRateHz += neuron.filteredRateHz;
        metrics_.meanEnergy += neuron.energy;
        metrics_.finite =
            metrics_.finite
            && std::isfinite(neuron.somaMv)
            && std::isfinite(neuron.dendriteMv)
            && std::isfinite(neuron.energy);
    }
    metrics_.meanRateHz /= neurons_.size();
    metrics_.meanEnergy /= neurons_.size();
    metrics_.assemblyCount = static_cast<int>(assemblies_.size());
    metrics_.dopamine = dopamine_;
    metrics_.acetylcholine = acetylcholine_;
}

MotorAction PersistentNervousSystem::step(const SensorFrame& frame) {
    if (
        !std::isfinite(frame.temperature)
        || !std::isfinite(frame.internalEnergy)
        || !std::isfinite(frame.reward)
        || !std::isfinite(frame.novelty)
        || !std::all_of(
            frame.visionEvents.begin(),
            frame.visionEvents.end(),
            [](double value) { return std::isfinite(value); })
        || !std::all_of(
            frame.audioSamples.begin(),
            frame.audioSamples.end(),
            [](double value) { return std::isfinite(value); })
        || !std::all_of(
            frame.touch.begin(),
            frame.touch.end(),
            [](double value) { return std::isfinite(value); })
        || !std::all_of(
            frame.contextEvents.begin(),
            frame.contextEvents.end(),
            [](double value) { return std::isfinite(value); })) {
        throw std::invalid_argument("Sensorframe enthält nichtendliche Werte");
    }
    std::vector<double> somaDrive(neurons_.size(), 0.0);
    std::vector<double> dendriteDrive(neurons_.size(), 0.0);
    deliverAxonEvents(frame);
    encodeSensors(frame, somaDrive, dendriteDrive);
    updateNeurons(somaDrive, dendriteDrive);
    scheduleSpikeEvents();
    updatePlasticity(frame);
    updateAssemblies(frame);
    updateHomeostasisAndEnergy();
    updateStructuralPlasticity();
    ++metrics_.step;
    updateMetrics();
    return decodeAction();
}

std::vector<MotorAction> PersistentNervousSystem::run(
    const std::vector<SensorFrame>& frames) {
    std::vector<MotorAction> actions;
    actions.reserve(frames.size());
    for (const auto& frame : frames) {
        actions.push_back(step(frame));
    }
    return actions;
}

bool PersistentNervousSystem::dalePrincipleHolds() const {
    for (const auto& synapse : synapses_) {
        if (!synapse.active) continue;
        const bool inhibitory =
            neurons_[synapse.pre].role == PopulationRole::Inhibitory;
        if (inhibitory && synapse.weight > 0.0) return false;
        if (!inhibitory && synapse.weight < 0.0) return false;
    }
    return true;
}

RepresentationState PersistentNervousSystem::inspect() const {
    RepresentationState state;
    state.step = metrics_.step;
    state.activeAssembly = metrics_.activeAssembly;
    state.neurons.reserve(neurons_.size());
    for (const auto& neuron : neurons_) {
        state.neurons.push_back(NeuronStateView{
            neuron.role,
            neuron.somaMv,
            neuron.dendriteMv,
            neuron.filteredRateHz,
            neuron.fastRateHz,
            neuron.energy,
            neuron.spikeCount,
            neuron.active});
    }
    state.synapses.reserve(synapses_.size());
    for (std::size_t index = 0; index < synapses_.size(); ++index) {
        const auto& synapse = synapses_[index];
        state.synapses.push_back(SynapseStateView{
            index,
            synapse.parentSynapse,
            synapse.pre,
            synapse.post,
            synapse.receptor,
            synapse.weight,
            synapse.consolidatedWeight,
            synapse.eligibility,
            synapse.resource,
            synapse.usage,
            synapse.active});
    }
    state.assemblies.reserve(assemblies_.size());
    for (const auto& assembly : assemblies_) {
        state.assemblies.push_back(AssemblyStateView{
            assembly.id,
            assembly.prototype,
            assembly.activation,
            assembly.observations,
            assembly.lastActiveStep});
    }
    return state;
}

std::uint64_t PersistentNervousSystem::stateHash() const {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto add = [&hash](const auto& value) {
        hash = mixHash(hash, &value, sizeof(value));
    };
    add(metrics_.step);
    for (const auto& neuron : neurons_) {
        add(neuron.role);
        add(neuron.somaMv);
        add(neuron.dendriteMv);
        add(neuron.gAmpa);
        add(neuron.gNmda);
        add(neuron.gGabaA);
        add(neuron.gGabaB);
        add(neuron.adaptationMv);
        add(neuron.homeostaticBiasMv);
        add(neuron.filteredRateHz);
        add(neuron.fastRateHz);
        add(neuron.slowBaselineHz);
        add(neuron.energy);
        add(neuron.excitability);
        add(neuron.refractorySteps);
        add(neuron.spikeCount);
        add(neuron.active);
    }
    for (const auto& synapse : synapses_) {
        add(synapse.pre);
        add(synapse.parentSynapse);
        add(synapse.post);
        add(synapse.receptor);
        add(synapse.weight);
        add(synapse.consolidatedWeight);
        add(synapse.eligibility);
        add(synapse.resource);
        add(synapse.facilitation);
        add(synapse.usage);
        add(synapse.delaySteps);
        add(synapse.ageSteps);
        add(synapse.active);
    }
    for (const auto& trace : {&preTrace_, &postTrace_}) {
        hash = mixHash(
            hash,
            trace->data(),
            trace->size() * sizeof(double));
    }
    if (!assemblyAccumulator_.empty()) {
        hash = mixHash(
            hash,
            assemblyAccumulator_.data(),
            assemblyAccumulator_.size() * sizeof(double));
    }
    if (!assemblyBaseline_.empty()) {
        hash = mixHash(
            hash,
            assemblyBaseline_.data(),
            assemblyBaseline_.size() * sizeof(double));
    }
    for (const auto& queue : axonQueue_) {
        const auto size = queue.size();
        add(size);
        for (const auto& event : queue) {
            add(event.synapse);
            add(event.amplitude);
        }
    }
    for (const auto& assembly : assemblies_) {
        add(assembly.id);
        if (!assembly.prototype.empty()) {
            hash = mixHash(
                hash,
                assembly.prototype.data(),
                assembly.prototype.size() * sizeof(double));
        }
        add(assembly.activation);
        add(assembly.observations);
        add(assembly.lastActiveStep);
    }
    add(dopamine_);
    add(acetylcholine_);
    add(lastStructuralStep_);
    add(stimulusSignature_);
    add(stimulusAgeSteps_);
    add(stimulusWasPresent_);
    std::ostringstream randomState;
    randomState << random_;
    const auto randomText = randomState.str();
    hash = mixHash(hash, randomText.data(), randomText.size());
    return hash;
}

void PersistentNervousSystem::saveSnapshot(
    const std::filesystem::path& path) const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Snapshotdatei konnte nicht erstellt werden");
    }
    const std::array<char, 8> magic{'A','G','N','S','V','9','\0','\0'};
    output.write(magic.data(), magic.size());
    writePod(output, config_);
    writePodVector(output, neurons_);
    writePodVector(output, synapses_);
    writePodVector(output, spikes_);
    writePodVector(output, preTrace_);
    writePodVector(output, postTrace_);
    writePodVector(output, assemblyAccumulator_);
    writePodVector(output, assemblyBaseline_);
    writePod(output, metrics_);
    writePod(output, dopamine_);
    writePod(output, acetylcholine_);
    writePod(output, lastStructuralStep_);
    writePod(output, stimulusSignature_);
    writePod(output, stimulusAgeSteps_);
    writePod(output, stimulusWasPresent_);
    const std::uint64_t queueSize = axonQueue_.size();
    writePod(output, queueSize);
    for (const auto& queue : axonQueue_) {
        writePodVector(output, queue);
    }
    const std::uint64_t assemblyCount = assemblies_.size();
    writePod(output, assemblyCount);
    for (const auto& assembly : assemblies_) {
        writePod(output, assembly.id);
        writePodVector(output, assembly.prototype);
        writePod(output, assembly.activation);
        writePod(output, assembly.observations);
        writePod(output, assembly.lastActiveStep);
    }
    std::ostringstream randomState;
    randomState << random_;
    const std::string randomText = randomState.str();
    const std::uint64_t randomLength = randomText.size();
    writePod(output, randomLength);
    output.write(
        randomText.data(),
        static_cast<std::streamsize>(randomText.size()));
    if (!output) {
        throw std::runtime_error("Snapshot konnte nicht abgeschlossen werden");
    }
}

void PersistentNervousSystem::loadSnapshot(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Snapshotdatei konnte nicht geöffnet werden");
    }
    std::array<char, 8> magic{};
    input.read(magic.data(), magic.size());
    const std::array<char, 8> expected{'A','G','N','S','V','9','\0','\0'};
    if (magic != expected) {
        throw std::runtime_error("Unbekanntes Snapshotformat");
    }
    NervousSystemConfig savedConfig;
    readPod(input, savedConfig);
    savedConfig.validate();
    config_ = savedConfig;
    readPodVector(input, neurons_);
    readPodVector(input, synapses_);
    readPodVector(input, spikes_);
    readPodVector(input, preTrace_);
    readPodVector(input, postTrace_);
    readPodVector(input, assemblyAccumulator_);
    readPodVector(input, assemblyBaseline_);
    readPod(input, metrics_);
    readPod(input, dopamine_);
    readPod(input, acetylcholine_);
    readPod(input, lastStructuralStep_);
    readPod(input, stimulusSignature_);
    readPod(input, stimulusAgeSteps_);
    readPod(input, stimulusWasPresent_);
    std::uint64_t queueSize = 0;
    readPod(input, queueSize);
    if (queueSize == 0 || queueSize > 10000) {
        throw std::runtime_error("Ungültige Axonwarteschlange im Snapshot");
    }
    axonQueue_.resize(static_cast<std::size_t>(queueSize));
    for (auto& queue : axonQueue_) {
        readPodVector(input, queue);
    }
    std::uint64_t assemblyCount = 0;
    readPod(input, assemblyCount);
    if (assemblyCount > static_cast<std::uint64_t>(config_.maximumAssemblies)) {
        throw std::runtime_error("Zu viele Assemblies im Snapshot");
    }
    assemblies_.resize(static_cast<std::size_t>(assemblyCount));
    for (auto& assembly : assemblies_) {
        readPod(input, assembly.id);
        readPodVector(input, assembly.prototype);
        readPod(input, assembly.activation);
        readPod(input, assembly.observations);
        readPod(input, assembly.lastActiveStep);
    }
    std::uint64_t randomLength = 0;
    readPod(input, randomLength);
    if (randomLength > 1'000'000) {
        throw std::runtime_error("RNG-Zustand im Snapshot ist zu groß");
    }
    std::string randomText(static_cast<std::size_t>(randomLength), '\0');
    input.read(
        randomText.data(),
        static_cast<std::streamsize>(randomText.size()));
    std::istringstream randomState(randomText);
    randomState >> random_;
    if (!input || !randomState) {
        throw std::runtime_error("RNG-Zustand im Snapshot ist ungültig");
    }
    if (
        neurons_.size() != static_cast<std::size_t>(config_.neuronCount())
        || spikes_.size() != neurons_.size()
        || preTrace_.size() != neurons_.size()
        || postTrace_.size() != neurons_.size()
        || assemblyAccumulator_.size() != neurons_.size()
        || assemblyBaseline_.size() != neurons_.size()) {
        throw std::runtime_error("Snapshotdimensionen stimmen nicht");
    }
    rebuildOutgoing();
    updateMetrics();
}

void PersistentNervousSystem::writeStateJson(
    const std::filesystem::path& path) const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Statusbericht konnte nicht geschrieben werden");
    }
    output << std::fixed << std::setprecision(9)
        << "{\n"
        << "  \"schema\": \"agns-state-v1\",\n"
        << "  \"step\": " << metrics_.step << ",\n"
        << "  \"state_hash\": \"" << std::hex << std::uppercase
        << stateHash() << std::dec << "\",\n"
        << "  \"neurons\": " << neurons_.size() << ",\n"
        << "  \"active_synapses\": " << metrics_.activeSynapses << ",\n"
        << "  \"assemblies\": " << metrics_.assemblyCount << ",\n"
        << "  \"total_spikes\": " << metrics_.totalSpikes << ",\n"
        << "  \"total_transmissions\": " << metrics_.totalTransmissions << ",\n"
        << "  \"mean_rate_hz\": " << metrics_.meanRateHz << ",\n"
        << "  \"mean_energy\": " << metrics_.meanEnergy << ",\n"
        << "  \"dopamine\": " << metrics_.dopamine << ",\n"
        << "  \"acetylcholine\": " << metrics_.acetylcholine << ",\n"
        << "  \"finite\": " << (metrics_.finite ? "true" : "false") << "\n"
        << "}\n";
}

void PersistentNervousSystem::applyDamage(
    double neuronFraction,
    double synapseFraction,
    std::uint64_t seed) {
    static_cast<void>(applyDamageWithReport(
        neuronFraction,
        synapseFraction,
        seed));
}

DamageReport PersistentNervousSystem::applyDamageWithReport(
    double neuronFraction,
    double synapseFraction,
    std::uint64_t seed) {
    if (
        !std::isfinite(neuronFraction)
        || !std::isfinite(synapseFraction)
        || neuronFraction < 0.0
        || neuronFraction > 0.9
        || synapseFraction < 0.0
        || synapseFraction > 0.9) {
        throw std::invalid_argument(
            "Schadensanteile müssen in [0,0.9] liegen");
    }
    std::mt19937_64 damageRandom(seed);
    std::vector<std::size_t> neuronIndices;
    for (std::size_t index = 0; index < neurons_.size(); ++index) {
        if (
            neurons_[index].role != PopulationRole::Sensory
            && neurons_[index].role != PopulationRole::Motor) {
            neuronIndices.push_back(index);
        }
    }
    std::shuffle(
        neuronIndices.begin(),
        neuronIndices.end(),
        damageRandom);
    const std::size_t neuronDamage = static_cast<std::size_t>(
        std::floor(neuronFraction * neuronIndices.size()));
    DamageReport report;
    report.disabledNeurons.reserve(neuronDamage);
    for (std::size_t index = 0; index < neuronDamage; ++index) {
        const auto disabled = neuronIndices[index];
        neurons_[disabled].active = false;
        report.disabledNeurons.push_back(disabled);
    }
    std::vector<std::size_t> synapseIndices;
    for (std::size_t index = 0; index < synapses_.size(); ++index) {
        if (synapses_[index].active) synapseIndices.push_back(index);
    }
    std::shuffle(
        synapseIndices.begin(),
        synapseIndices.end(),
        damageRandom);
    const std::size_t synapseDamage = static_cast<std::size_t>(
        std::floor(synapseFraction * synapseIndices.size()));
    report.disabledSynapses.reserve(synapseDamage);
    for (std::size_t index = 0; index < synapseDamage; ++index) {
        const auto disabled = synapseIndices[index];
        synapses_[disabled].active = false;
        report.disabledSynapses.push_back(disabled);
    }
    rebuildOutgoing();
    updateMetrics();
    return report;
}

DamageReport PersistentNervousSystem::disableSynapses(
    const std::vector<std::size_t>& synapseIndices) {
    DamageReport report;
    report.disabledSynapses.reserve(synapseIndices.size());
    for (const auto index : synapseIndices) {
        if (index >= synapses_.size()) {
            throw std::out_of_range(
                "Synapsenindex fuer gezielten Schaden ausserhalb des Netzes");
        }
        if (!synapses_[index].active) continue;
        synapses_[index].active = false;
        report.disabledSynapses.push_back(index);
    }
    rebuildOutgoing();
    updateMetrics();
    return report;
}

void PersistentNervousSystem::setStructuralPlasticityEnabled(bool enabled) {
    config_.structuralPlasticityEnabled = enabled;
}

ContinuousEnvironment::ContinuousEnvironment(std::uint64_t seed)
    : seed_(seed) {
    const double offset =
        static_cast<double>(seed_ % 17) / 100.0;
    position_ -= offset;
    target_ -= offset * 0.5;
}

SensorFrame ContinuousEnvironment::sense() const {
    SensorFrame frame;
    const double difference = target_ - position_;
    frame.visionEvents = {
        std::clamp(-difference, 0.0, 1.0),
        std::clamp(difference, 0.0, 1.0),
        std::clamp(std::abs(difference), 0.0, 1.0),
        reachedTarget() ? 1.0 : 0.0};
    frame.audioSamples = {
        std::sin(kPi * position_),
        std::cos(kPi * target_)};
    frame.touch = {
        position_ <= -0.99 ? 1.0 : 0.0,
        position_ >= 0.99 ? 1.0 : 0.0};
    if (textIndex_ < textStream_.size()) {
        frame.textBytes.push_back(textStream_[textIndex_]);
    }
    frame.temperature = temperature_;
    frame.internalEnergy =
        std::clamp(1.0 - 0.2 * std::abs(position_), 0.0, 1.0);
    frame.reward = lastReward_;
    frame.novelty = std::clamp(std::abs(difference), 0.0, 1.0);
    return frame;
}

double ContinuousEnvironment::apply(const MotorAction& action) {
    const double previousDistance = std::abs(target_ - position_);
    position_ = std::clamp(
        position_ + 0.035 * std::clamp(action.movement, -1.0, 1.0),
        -1.0,
        1.0);
    const double currentDistance = std::abs(target_ - position_);
    lastReward_ =
        8.0 * (previousDistance - currentDistance)
        - 0.002 * std::abs(action.movement);
    if (currentDistance < 0.05) {
        lastReward_ += 0.1;
    }
    temperature_ =
        0.98 * temperature_ + 0.02 * std::abs(action.movement);
    cumulativeReward_ += lastReward_;
    if (textIndex_ < textStream_.size()) {
        ++textIndex_;
    }
    return lastReward_;
}

void ContinuousEnvironment::injectTextUtf8(const std::string& text) {
    textStream_.assign(text.begin(), text.end());
    textIndex_ = 0;
}

bool ContinuousEnvironment::reachedTarget() const {
    return std::abs(target_ - position_) < 0.05;
}

double ContinuousEnvironment::position() const {
    return position_;
}

double ContinuousEnvironment::target() const {
    return target_;
}

double ContinuousEnvironment::cumulativeReward() const {
    return cumulativeReward_;
}

ClosedLoopResult runClosedLoop(
    PersistentNervousSystem& nervousSystem,
    ContinuousEnvironment& environment,
    int steps) {
    if (steps <= 0) {
        throw std::invalid_argument("Closed-loop Schritte müssen positiv sein");
    }
    for (int step = 0; step < steps; ++step) {
        const SensorFrame frame = environment.sense();
        const MotorAction action = nervousSystem.step(frame);
        environment.apply(action);
    }
    ClosedLoopResult result;
    result.steps = steps;
    result.cumulativeReward = environment.cumulativeReward();
    result.finalDistance =
        std::abs(environment.target() - environment.position());
    result.meanEnergy = nervousSystem.metrics().meanEnergy;
    result.assemblies = nervousSystem.metrics().assemblyCount;
    result.activeSynapses = nervousSystem.metrics().activeSynapses;
    result.stateHash = nervousSystem.stateHash();
    return result;
}

}  // namespace agns
