#define NOMINMAX

#include "bio_core.hpp"
#include "classifier.hpp"

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kPanelWidth = 800;
constexpr int IDC_NEURONS = 1001;
constexpr int IDC_STEPS = 1002;
constexpr int IDC_SEED = 1003;
constexpr int IDC_NOISE = 1004;
constexpr int IDC_PULSE = 1005;
constexpr int IDC_DENSITY = 1006;
constexpr int IDC_CONSTANT_GATE = 1007;
constexpr int IDC_SAMPLES = 1008;
constexpr int IDC_FOLDS = 1009;
constexpr int IDC_GATE_MODE = 1010;
constexpr int IDC_PATTERN = 1011;
constexpr int IDC_PLASTICITY = 1012;
constexpr int IDC_GATE_TIMING = 1013;
constexpr int IDC_GATE_PERTURBATION = 1014;
constexpr int IDC_EMISSION_FEATURE = 1015;
constexpr int IDC_PROJECTION_PARAMETERS = 1016;
constexpr int IDC_MEMBRANE_PARAMETERS = 1017;
constexpr int IDC_EI_PARAMETERS = 1018;
constexpr int IDC_ADAPTATION_PARAMETERS = 1019;
constexpr int IDC_TASK_INTERNALS = 1020;
constexpr int IDC_READOUT_PARAMETERS = 1021;
constexpr int IDC_STDP_PARAMETERS = 1022;
constexpr int IDC_GATE_INTERNALS = 1023;
constexpr int IDC_AXON_DELAYS = 1024;
constexpr int IDC_SYNAPSE_MODEL = 1025;
constexpr int IDC_CONDUCTANCE_PARAMETERS = 1026;
constexpr int IDC_DENDRITE_ENABLED = 1027;
constexpr int IDC_DENDRITE_PARAMETERS = 1028;
constexpr int IDC_CLASS_OPERATORS_ENABLED = 1029;
constexpr int IDC_CLASS_OPERATORS = 1030;
constexpr int IDC_RESEARCH_SEEDS = 1031;
constexpr int IDC_ELIGIBILITY_TAUS = 1032;
constexpr int IDC_ELIGIBILITY_ENABLED = 1033;
constexpr int IDC_INTERACTION_PRODUCTS = 1034;
constexpr int IDC_LOCAL_ELIGIBILITY_PARAMETERS = 1035;
constexpr int IDC_LOCAL_ELIGIBILITY_ENABLED = 1036;
constexpr int IDC_SINGLE = 1101;
constexpr int IDC_COMPARE = 1102;
constexpr int IDC_SAVE = 1103;
constexpr int IDC_MULTI_SEED = 1104;
constexpr int IDC_OPTIMIZE = 1105;
constexpr int IDC_XOR_ABLATIONS = 1106;
constexpr int IDC_TRACE_ESSENTIAL = 1107;
constexpr int IDC_OUTPUT = 1201;

struct UiState {
    HFONT font = nullptr;
    HWND tooltip = nullptr;
    agbnn::SimulationResult lastRun;
    std::vector<agbnn::GateEvaluation> comparisons;
    std::wstring report;
    std::unordered_map<HWND, std::wstring> tooltipDescriptions;
    std::unordered_map<int, HWND> validationMarkers;
    std::unordered_map<HWND, bool> markerValidity;
    std::wstring tooltipBuffer;
    enum class View { None, Single, Comparison } view = View::None;
};

UiState g_state;

std::wstring readText(HWND window, int id) {
    wchar_t buffer[256]{};
    GetDlgItemTextW(window, id, buffer, 255);
    return buffer;
}

int readInt(HWND window, int id, const wchar_t* name) {
    const std::wstring text = readText(window, id);
    std::size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    if (consumed != text.size()) {
        throw std::invalid_argument("invalid integer");
    }
    (void)name;
    return value;
}

double readDouble(HWND window, int id, const wchar_t* name) {
    std::wstring text = readText(window, id);
    std::replace(text.begin(), text.end(), L',', L'.');
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size()) {
        throw std::invalid_argument("invalid floating-point value");
    }
    (void)name;
    return value;
}

agbnn::GateMode selectedGate(HWND window) {
    const LRESULT selection = SendDlgItemMessageW(
        window, IDC_GATE_MODE, CB_GETCURSEL, 0, 0);
    switch (selection) {
        case 0: return agbnn::GateMode::Kernel;
        case 1: return agbnn::GateMode::Constant;
        case 2: return agbnn::GateMode::Disabled;
        case 3: return agbnn::GateMode::Sign;
        case 4: return agbnn::GateMode::Tanh;
        case 5: return agbnn::GateMode::Random;
        default: return agbnn::GateMode::Kernel;
    }
}

agbnn::GateTiming selectedGateTiming(HWND window) {
    const LRESULT selection = SendDlgItemMessageW(
        window, IDC_GATE_TIMING, CB_GETCURSEL, 0, 0);
    return selection == 1
        ? agbnn::GateTiming::EmissionState
        : agbnn::GateTiming::ResetLocked;
}

agbnn::GatePerturbation selectedGatePerturbation(HWND window) {
    const LRESULT selection = SendDlgItemMessageW(
        window, IDC_GATE_PERTURBATION, CB_GETCURSEL, 0, 0);
    if (selection == 1) {
        return agbnn::GatePerturbation::TimeShifted;
    }
    if (selection == 2) {
        return agbnn::GatePerturbation::StateShuffled;
    }
    return agbnn::GatePerturbation::None;
}

agbnn::EmissionFeature selectedEmissionFeature(HWND window) {
    const LRESULT selection = SendDlgItemMessageW(
        window, IDC_EMISSION_FEATURE, CB_GETCURSEL, 0, 0);
    if (selection == 1) {
        return agbnn::EmissionFeature::EiBalance;
    }
    if (selection == 2) {
        return agbnn::EmissionFeature::FeatureProjection;
    }
    return agbnn::EmissionFeature::PreResetVoltage;
}

void readProjectionParameters(
    HWND window,
    agbnn::NetworkConfig& config) {
    std::wstringstream input(readText(window, IDC_PROJECTION_PARAMETERS));
    std::vector<double> values;
    std::wstring token;
    while (std::getline(input, token, L';')) {
        std::replace(token.begin(), token.end(), L',', L'.');
        std::size_t consumed = 0;
        const double value = std::stod(token, &consumed);
        if (consumed != token.size()) {
            throw std::invalid_argument("invalid projection parameter");
        }
        values.push_back(value);
    }
    if (values.size() != 7) {
        throw std::invalid_argument(
            "Projektion benötigt 7 mit Semikolon getrennte Werte");
    }
    config.projectionEiWeight = values[0];
    config.projectionSlopeWeight = values[1];
    config.projectionOvershootWeight = values[2];
    config.projectionIsiWeight = values[3];
    config.membraneSlopeScaleMvPerMs = values[4];
    config.thresholdOvershootScaleMv = values[5];
    config.isiTauMs = values[6];
}

std::vector<double> readDelimitedDoubles(
    HWND window,
    int id,
    std::size_t expected,
    const char* errorMessage) {
    std::wstringstream input(readText(window, id));
    std::vector<double> values;
    std::wstring token;
    while (std::getline(input, token, L';')) {
        std::replace(token.begin(), token.end(), L',', L'.');
        std::size_t consumed = 0;
        const double value = std::stod(token, &consumed);
        if (consumed != token.size()) {
            throw std::invalid_argument(errorMessage);
        }
        values.push_back(value);
    }
    if (values.size() != expected) {
        throw std::invalid_argument(errorMessage);
    }
    return values;
}

agbnn::GateMode parseGateMode(std::wstring token) {
    std::transform(
        token.begin(),
        token.end(),
        token.begin(),
        [](wchar_t value) {
            return static_cast<wchar_t>(std::towlower(value));
        });
    if (token == L"kernel") return agbnn::GateMode::Kernel;
    if (token == L"constant") return agbnn::GateMode::Constant;
    if (token == L"disabled") return agbnn::GateMode::Disabled;
    if (token == L"sign") return agbnn::GateMode::Sign;
    if (token == L"tanh") return agbnn::GateMode::Tanh;
    if (token == L"random") return agbnn::GateMode::Random;
    throw std::invalid_argument(
        "Operatoren: kernel/constant/disabled/sign/tanh/random");
}

void readAdvancedNetworkConfig(
    HWND window,
    agbnn::NetworkConfig& config) {
    const auto membrane = readDelimitedDoubles(
        window,
        IDC_MEMBRANE_PARAMETERS,
        7,
        "Membranparameter benötigen 7 Werte");
    config.dtMs = membrane[0];
    config.tauMembraneMs = membrane[1];
    config.tauSynapseMs = membrane[2];
    config.restingMv = membrane[3];
    config.resetMv = membrane[4];
    config.thresholdMv = membrane[5];
    config.refractoryMs = membrane[6];

    const auto ei = readDelimitedDoubles(
        window, IDC_EI_PARAMETERS, 4, "E/I-Parameter benötigen 4 Werte");
    config.excitatoryFraction = ei[0];
    config.excitatoryWeight = ei[1];
    config.inhibitoryWeight = ei[2];
    config.maximumWeight = ei[3];

    const auto adaptation = readDelimitedDoubles(
        window,
        IDC_ADAPTATION_PARAMETERS,
        2,
        "Adaptationsparameter benötigen 2 Werte");
    config.adaptiveThresholdIncrementMv = adaptation[0];
    config.adaptiveThresholdTauMs = adaptation[1];

    const auto stdp = readDelimitedDoubles(
        window, IDC_STDP_PARAMETERS, 4, "STDP benötigt 4 Werte");
    config.stdpLearningRate = stdp[0];
    config.stdpTauMs = stdp[1];
    config.stdpPotentiation = stdp[2];
    config.stdpDepression = stdp[3];

    config.localEligibilityEnabled =
        SendDlgItemMessageW(
            window,
            IDC_LOCAL_ELIGIBILITY_ENABLED,
            BM_GETCHECK,
            0,
            0)
        == BST_CHECKED;
    const auto localEligibility = readDelimitedDoubles(
        window,
        IDC_LOCAL_ELIGIBILITY_PARAMETERS,
        4,
        "Lokale Eligibility benötigt tau;gain;maximum;shift");
    config.localEligibilityTauMs = localEligibility[0];
    config.localEligibilityGain = localEligibility[1];
    config.localEligibilityMaximum = localEligibility[2];
    config.localEligibilityTimeShiftMs = localEligibility[3];

    const auto gate = readDelimitedDoubles(
        window, IDC_GATE_INTERNALS, 2, "Gate intern benötigt 2 Werte");
    config.gateInputScale = gate[0];
    config.randomGateAmplitude = gate[1];

    const auto delays = readDelimitedDoubles(
        window, IDC_AXON_DELAYS, 2, "Axonverzögerung benötigt 2 Werte");
    config.minimumAxonDelayMs = delays[0];
    config.maximumAxonDelayMs = delays[1];

    config.synapseModel =
        SendDlgItemMessageW(
            window, IDC_SYNAPSE_MODEL, CB_GETCURSEL, 0, 0) == 1
        ? agbnn::SynapseModel::ConductanceBased
        : agbnn::SynapseModel::CurrentBased;
    const auto conductance = readDelimitedDoubles(
        window,
        IDC_CONDUCTANCE_PARAMETERS,
        4,
        "AMPA/GABA benötigt 4 Werte");
    config.ampaReversalMv = conductance[0];
    config.gabaReversalMv = conductance[1];
    config.excitatoryConductanceScale = conductance[2];
    config.inhibitoryConductanceScale = conductance[3];

    config.dendriteEnabled =
        SendDlgItemMessageW(
            window, IDC_DENDRITE_ENABLED, BM_GETCHECK, 0, 0)
        == BST_CHECKED;
    const auto dendrite = readDelimitedDoubles(
        window,
        IDC_DENDRITE_PARAMETERS,
        3,
        "Dendrit benötigt 3 Werte");
    config.tauDendriteMs = dendrite[0];
    config.somaDendriteCoupling = dendrite[1];
    config.externalToDendriteFraction = dendrite[2];

    config.classSpecificOperatorsEnabled =
        SendDlgItemMessageW(
            window, IDC_CLASS_OPERATORS_ENABLED, BM_GETCHECK, 0, 0)
        == BST_CHECKED;
    std::wstringstream operators(readText(window, IDC_CLASS_OPERATORS));
    std::vector<agbnn::GateMode> modes;
    std::wstring token;
    while (std::getline(operators, token, L';')) {
        modes.push_back(parseGateMode(token));
    }
    if (modes.size() != 4) {
        throw std::invalid_argument(
            "Klassenoperatoren benötigen EE;EI;IE;II");
    }
    config.eeGateMode = modes[0];
    config.eiGateMode = modes[1];
    config.ieGateMode = modes[2];
    config.iiGateMode = modes[3];
}

agbnn::NetworkConfig readNetworkConfig(HWND window) {
    agbnn::NetworkConfig config;
    config.neuronCount = readInt(window, IDC_NEURONS, L"Neuronen");
    config.seed = static_cast<std::uint64_t>(
        readInt(window, IDC_SEED, L"Seed"));
    config.connectionProbability =
        readDouble(window, IDC_DENSITY, L"Verbindungsdichte");
    config.constantGate =
        readDouble(window, IDC_CONSTANT_GATE, L"Konstantes Gate");
    config.gateMode = selectedGate(window);
    config.gateTiming = selectedGateTiming(window);
    config.gatePerturbation = selectedGatePerturbation(window);
    config.emissionFeature = selectedEmissionFeature(window);
    readProjectionParameters(window, config);
    readAdvancedNetworkConfig(window, config);
    config.plasticityEnabled =
        SendDlgItemMessageW(window, IDC_PLASTICITY, BM_GETCHECK, 0, 0)
        == BST_CHECKED;
    config.validate();
    return config;
}

agbnn::ClassificationConfig readTaskConfig(HWND window) {
    agbnn::ClassificationConfig config;
    config.steps = readInt(window, IDC_STEPS, L"Schritte");
    config.samplesPerClass =
        readInt(window, IDC_SAMPLES, L"Samples/Klasse");
    config.folds = readInt(window, IDC_FOLDS, L"Folds");
    config.noiseStd = readDouble(window, IDC_NOISE, L"Rauschen");
    config.pulseCurrent =
        readDouble(window, IDC_PULSE, L"Pulshöhe");
    const auto taskInternals = readDelimitedDoubles(
        window, IDC_TASK_INTERNALS, 2, "Aufgabe intern benötigt 2 Werte");
    config.baselineCurrent = taskInternals[0];
    if (std::abs(taskInternals[1] - std::round(taskInternals[1])) > 1e-9) {
        throw std::invalid_argument("Zeitfenster muss ganzzahlig sein");
    }
    config.timeBins = static_cast<int>(std::lround(taskInternals[1]));
    const auto readout = readDelimitedDoubles(
        window, IDC_READOUT_PARAMETERS, 3, "Readout benötigt 3 Werte");
    config.learningRate = readout[0];
    if (std::abs(readout[1] - std::round(readout[1])) > 1e-9) {
        throw std::invalid_argument("Readout-Epochen muss ganzzahlig sein");
    }
    config.trainingEpochs = static_cast<int>(std::lround(readout[1]));
    config.l2 = readout[2];
    config.validate();
    return config;
}

std::string canonicalConfiguration(
    const agbnn::NetworkConfig& network,
    const agbnn::ClassificationConfig& task,
    int pattern) {
    std::ostringstream value;
    value << std::setprecision(17)
        << "schema=agbnn-ui-v2"
        << ";pattern=" << pattern
        << ";neurons=" << network.neuronCount
        << ";efraction=" << network.excitatoryFraction
        << ";density=" << network.connectionProbability
        << ";networkseed=" << network.seed
        << ";dt=" << network.dtMs
        << ";taum=" << network.tauMembraneMs
        << ";taus=" << network.tauSynapseMs
        << ";vrest=" << network.restingMv
        << ";vreset=" << network.resetMv
        << ";vthreshold=" << network.thresholdMv
        << ";refractory=" << network.refractoryMs
        << ";we=" << network.excitatoryWeight
        << ";wi=" << network.inhibitoryWeight
        << ";wmax=" << network.maximumWeight
        << ";adaptinc=" << network.adaptiveThresholdIncrementMv
        << ";adapttau=" << network.adaptiveThresholdTauMs
        << ";gatemode=" << static_cast<int>(network.gateMode)
        << ";gatetiming=" << static_cast<int>(network.gateTiming)
        << ";perturbation=" << static_cast<int>(network.gatePerturbation)
        << ";feature=" << static_cast<int>(network.emissionFeature)
        << ";aei=" << network.projectionEiWeight
        << ";aslope=" << network.projectionSlopeWeight
        << ";aovershoot=" << network.projectionOvershootWeight
        << ";aisi=" << network.projectionIsiWeight
        << ";slopescale=" << network.membraneSlopeScaleMvPerMs
        << ";overshootscale=" << network.thresholdOvershootScaleMv
        << ";isitau=" << network.isiTauMs
        << ";delaymin=" << network.minimumAxonDelayMs
        << ";delaymax=" << network.maximumAxonDelayMs
        << ";synapse=" << static_cast<int>(network.synapseModel)
        << ";eampa=" << network.ampaReversalMv
        << ";egaba=" << network.gabaReversalMv
        << ";gescale=" << network.excitatoryConductanceScale
        << ";giscale=" << network.inhibitoryConductanceScale
        << ";dendrite=" << network.dendriteEnabled
        << ";dendritetau=" << network.tauDendriteMs
        << ";somadendrite=" << network.somaDendriteCoupling
        << ";externaldendrite=" << network.externalToDendriteFraction
        << ";classoperators=" << network.classSpecificOperatorsEnabled
        << ";ee=" << static_cast<int>(network.eeGateMode)
        << ";ei=" << static_cast<int>(network.eiGateMode)
        << ";ie=" << static_cast<int>(network.ieGateMode)
        << ";ii=" << static_cast<int>(network.iiGateMode)
        << ";gateinputscale=" << network.gateInputScale
        << ";constantgate=" << network.constantGate
        << ";randomamplitude=" << network.randomGateAmplitude
        << ";plasticity=" << network.plasticityEnabled
        << ";stdplr=" << network.stdpLearningRate
        << ";stdptau=" << network.stdpTauMs
        << ";stdppot=" << network.stdpPotentiation
        << ";stdpdep=" << network.stdpDepression
        << ";localeligibility=" << network.localEligibilityEnabled
        << ";localeligibilitytau=" << network.localEligibilityTauMs
        << ";localeligibilitygain=" << network.localEligibilityGain
        << ";localeligibilitymax=" << network.localEligibilityMaximum
        << ";localeligibilityshift="
        << network.localEligibilityTimeShiftMs
        << ";localeligibilitytransform="
        << static_cast<int>(network.localEligibilityTransform)
        << ";localeligibilityscope="
        << static_cast<int>(network.localEligibilityScope)
        << ";samples=" << task.samplesPerClass
        << ";folds=" << task.folds
        << ";steps=" << task.steps
        << ";timebins=" << task.timeBins
        << ";baseline=" << task.baselineCurrent
        << ";pulse=" << task.pulseCurrent
        << ";noise=" << task.noiseStd
        << ";taskseed=" << task.seed
        << ";readoutlr=" << task.learningRate
        << ";epochs=" << task.trainingEpochs
        << ";l2=" << task.l2
        << ";randomvalues=";
    for (double randomValue : network.randomGateValues) {
        value << randomValue << ",";
    }
    return value.str();
}

std::wstring configurationHash(
    const agbnn::NetworkConfig& network,
    const agbnn::ClassificationConfig& task,
    int pattern = -1) {
    const std::string canonical =
        canonicalConfiguration(network, task, pattern);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : canonical) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    std::wostringstream result;
    result << std::uppercase << std::hex << std::setfill(L'0')
        << std::setw(16) << hash;
    return result.str();
}

std::wstring formatConfigurationSummary(
    const agbnn::NetworkConfig& network,
    const agbnn::ClassificationConfig& task,
    int pattern = -1) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(6)
        << L"KONFIGURATION (geparst)\r\n"
        << L"Hash: " << configurationHash(network, task, pattern) << L"\r\n"
        << L"Netz: N=" << network.neuronCount
        << L", E-Anteil=" << network.excitatoryFraction
        << L", Dichte=" << network.connectionProbability
        << L", Seed=" << network.seed << L"\r\n"
        << L"Membran: dt/tauM/tauS=" << network.dtMs << L"/"
        << network.tauMembraneMs << L"/" << network.tauSynapseMs
        << L" ms, Vrest/Vreset/Vth=" << network.restingMv << L"/"
        << network.resetMv << L"/" << network.thresholdMv
        << L" mV, refr=" << network.refractoryMs << L" ms\r\n"
        << L"Synapsen: wE/wI/wMax=" << network.excitatoryWeight << L"/"
        << network.inhibitoryWeight << L"/" << network.maximumWeight
        << L", Modell=" << agbnn::synapseModelName(network.synapseModel)
        << L", Delay=" << network.minimumAxonDelayMs << L"..."
        << network.maximumAxonDelayMs << L" ms\r\n"
        << L"AMPA/GABA: E=" << network.ampaReversalMv << L"/"
        << network.gabaReversalMv << L" mV, Skala="
        << network.excitatoryConductanceScale << L"/"
        << network.inhibitoryConductanceScale << L"\r\n"
        << L"Dendrit: " << (network.dendriteEnabled ? L"an" : L"aus")
        << L", tau/Kopplung/Input=" << network.tauDendriteMs << L"/"
        << network.somaDendriteCoupling << L"/"
        << network.externalToDendriteFraction << L"\r\n"
        << L"Lokale Synapsen-Eligibility: "
        << (network.localEligibilityEnabled ? L"an" : L"aus")
        << L", tau/Gain/Maximum="
        << network.localEligibilityTauMs << L"/"
        << network.localEligibilityGain << L"/"
        << network.localEligibilityMaximum
        << L", Shift=" << network.localEligibilityTimeShiftMs
        << L" ms\r\n"
        << L"Gate: " << agbnn::gateModeName(network.gateMode) << L", "
        << agbnn::gateTimingName(network.gateTiming) << L", "
        << agbnn::emissionFeatureName(network.emissionFeature)
        << L", Kontrolle="
        << agbnn::gatePerturbationName(network.gatePerturbation)
        << L", Skala/Zufall=" << network.gateInputScale << L"/"
        << network.randomGateAmplitude << L"\r\n"
        << L"Klassenoperatoren: "
        << (network.classSpecificOperatorsEnabled ? L"an" : L"aus")
        << L" [EE=" << agbnn::gateModeName(network.eeGateMode)
        << L", EI=" << agbnn::gateModeName(network.eiGateMode)
        << L", IE=" << agbnn::gateModeName(network.ieGateMode)
        << L", II=" << agbnn::gateModeName(network.iiGateMode) << L"]\r\n"
        << L"Aufgabe: Schritte/Bins=" << task.steps << L"/"
        << task.timeBins << L", Basis/Puls/Rauschen="
        << task.baselineCurrent << L"/" << task.pulseCurrent << L"/"
        << task.noiseStd << L", Samples/Folds=" << task.samplesPerClass
        << L"/" << task.folds << L"\r\n"
        << L"Readout: lr/Epochen/L2=" << task.learningRate << L"/"
        << task.trainingEpochs << L"/" << task.l2
        << L", STDP=" << (network.plasticityEnabled ? L"an" : L"aus")
        << L" (" << network.stdpLearningRate << L"/"
        << network.stdpTauMs << L"/" << network.stdpPotentiation
        << L"/" << network.stdpDepression << L")\r\n\r\n";
    return out.str();
}

std::wstring formatSingle(
    const agbnn::NetworkConfig& network,
    const agbnn::ClassificationConfig& task,
    int pattern,
    const agbnn::SimulationResult& result) {
    const auto& metrics = result.metrics;
    std::wostringstream out;
    out << std::fixed << std::setprecision(6);
    out << formatConfigurationSummary(network, task, pattern)
        << L"EINZELSIMULATION\r\n"
        << L"Muster: " << (pattern == 0 ? L"A (0-1-0-1)" : L"B (1-0-1-0)") << L"\r\n"
        << L"Gate: " << agbnn::gateModeName(network.gateMode) << L"\r\n"
        << L"Timing: " << agbnn::gateTimingName(network.gateTiming)
        << L" | Kontrolle: "
        << agbnn::gatePerturbationName(network.gatePerturbation) << L"\r\n"
        << L"Emissionsfeature: "
        << agbnn::emissionFeatureName(network.emissionFeature) << L"\r\n"
        << L"Projektion a(EI,V',O,ISI): "
        << network.projectionEiWeight << L", "
        << network.projectionSlopeWeight << L", "
        << network.projectionOvershootWeight << L", "
        << network.projectionIsiWeight << L"\r\n"
        << L"Skalen sV/sO/tauISI: "
        << network.membraneSlopeScaleMvPerMs << L" / "
        << network.thresholdOvershootScaleMv << L" / "
        << network.isiTauMs << L" ms\r\n"
        << L"Membran dt/tauM/tauS: " << network.dtMs << L" / "
        << network.tauMembraneMs << L" / "
        << network.tauSynapseMs << L" ms\r\n"
        << L"Potentiale Ruhe/Reset/Schwelle: "
        << network.restingMv << L" / " << network.resetMv << L" / "
        << network.thresholdMv << L" mV | Refraktär: "
        << network.refractoryMs << L" ms\r\n"
        << L"Synapsenmodell: "
        << agbnn::synapseModelName(network.synapseModel)
        << L" | Axonverzögerung: " << network.minimumAxonDelayMs
        << L"..." << network.maximumAxonDelayMs << L" ms\r\n"
        << L"Dendrit: " << (network.dendriteEnabled ? L"an" : L"aus")
        << L" | Klassenoperatoren: "
        << (network.classSpecificOperatorsEnabled ? L"an" : L"aus")
        << L" ["
        << agbnn::gateModeName(network.eeGateMode) << L","
        << agbnn::gateModeName(network.eiGateMode) << L","
        << agbnn::gateModeName(network.ieGateMode) << L","
        << agbnn::gateModeName(network.iiGateMode) << L"]\r\n"
        << L"Neuronen: " << network.neuronCount
        << L" | Schritte: " << task.steps
        << L" | Seed: " << network.seed << L"\r\n"
        << L"Pulse: " << task.pulseCurrent
        << L" | Rauschen σ: " << task.noiseStd
        << L" | Dichte: " << network.connectionProbability << L"\r\n\r\n"
        << L"Gesamtspikes: " << metrics.totalSpikes << L"\r\n"
        << L"Mittlere Feuerrate: " << metrics.meanFiringRateHz << L" Hz\r\n"
        << L"Spannungsenergie: " << metrics.normalizedVoltageEnergy << L"\r\n"
        << L"Dendritische Spannungsenergie: "
        << metrics.dendriticVoltageEnergy << L"\r\n"
        << L"Mittlere Axonverzögerung: "
        << metrics.meanAxonDelayMs << L" ms\r\n"
        << L"Synaptische Übertragungen: "
        << metrics.synapticTransmissionCount << L"\r\n"
        << L"Lokale Eligibility Synapsen/Mittel/Varianz/|max|: "
        << metrics.localEligibilitySynapseCount << L" / "
        << metrics.meanLocalEligibility << L" / "
        << metrics.localEligibilityVariance << L" / "
        << metrics.maximumAbsoluteLocalEligibility << L"\r\n"
        << L"Eligibility-Übertragungsfaktor Mittel/Varianz: "
        << metrics.meanEligibilityTransmissionFactor << L" / "
        << metrics.eligibilityTransmissionFactorVariance << L"\r\n"
        << L"Population-Spike-Count-Fano: "
        << metrics.populationSpikeCountFano << L"\r\n"
        << L"Mittlere paarweise Spike-Korrelation: "
        << metrics.meanPairwiseSpikeCorrelation << L"\r\n"
        << L"Binned coincidence rate: "
        << metrics.binnedCoincidenceRate << L"\r\n"
        << L"Gate-Mittel: " << metrics.meanGate << L"\r\n"
        << L"Gate-Varianz: " << metrics.gateVariance << L"\r\n"
        << L"Wirksames Gate (nur übertragene Spikes): "
        << metrics.effectiveGateMean << L"\r\n"
        << L"Wirksame Gate-Varianz: "
        << metrics.effectiveGateVariance << L"\r\n"
        << L"Wirksame Gate-Entropie: "
        << metrics.effectiveGateEntropyBits << L" bit\r\n"
        << L"Wirksamer Gate-Bereich: ["
        << metrics.effectiveGateMinimum << L", "
        << metrics.effectiveGateMaximum << L"]\r\n"
        << L"Spike-Ereignisse: " << metrics.spikeEventCount << L"\r\n"
        << L"Eventfeature Mittel/Varianz: "
        << metrics.eventFeatureMean << L" / "
        << metrics.eventFeatureVariance << L"\r\n"
        << L"Eventfeature-Bereich: ["
        << metrics.eventFeatureMinimum << L", "
        << metrics.eventFeatureMaximum << L"]\r\n"
        << L"Komponenten Mittel/Varianz:\r\n"
        << L"  E/I: " << metrics.eventEiBalanceMean << L" / "
        << metrics.eventEiBalanceVariance << L"\r\n"
        << L"  Membransteigung: " << metrics.eventMembraneSlopeMean
        << L" / " << metrics.eventMembraneSlopeVariance << L"\r\n"
        << L"  Schwellenüberschuss: "
        << metrics.eventThresholdOvershootMean << L" / "
        << metrics.eventThresholdOvershootVariance << L"\r\n"
        << L"  ISI-Zustand: " << metrics.eventIsiStateMean << L" / "
        << metrics.eventIsiStateVariance << L"\r\n"
        << L"Aktive Neuronen: " << metrics.activeFraction * 100.0 << L" %\r\n"
        << L"Alle Zustände endlich: " << (metrics.finite ? L"ja" : L"NEIN") << L"\r\n";
    return out.str();
}

std::wstring formatComparison(
    const agbnn::NetworkConfig& network,
    const agbnn::ClassificationConfig& task,
    const std::vector<agbnn::GateEvaluation>& results) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(4);
    out << formatConfigurationSummary(network, task)
        << L"STRATIFIZIERTE CROSS-VALIDATION\r\n"
        << L"Aufgabe: gleiche Assemblies, umgekehrte zeitliche Reihenfolge\r\n"
        << L"Neuronen: " << network.neuronCount
        << L" | Samples/Klasse: " << task.samplesPerClass
        << L" | Folds: " << task.folds
        << L" | Schritte: " << task.steps << L"\r\n"
        << L"Pulse: " << task.pulseCurrent
        << L" | Rauschen σ: " << task.noiseStd
        << L" | Timing: " << agbnn::gateTimingName(network.gateTiming)
        << L" | Kontrolle: "
        << agbnn::gatePerturbationName(network.gatePerturbation)
        << L" | Feature: "
        << agbnn::emissionFeatureName(network.emissionFeature)
        << L" | STDP: " << (network.plasticityEnabled ? L"an" : L"aus")
        << L"\r\n"
        << L"Projektion a(EI,V',O,ISI)="
        << network.projectionEiWeight << L","
        << network.projectionSlopeWeight << L","
        << network.projectionOvershootWeight << L","
        << network.projectionIsiWeight
        << L" | Skalen=" << network.membraneSlopeScaleMvPerMs << L","
        << network.thresholdOvershootScaleMv << L","
        << network.isiTauMs << L" ms\r\n"
        << L"Biophysik dt/tauM/tauS=" << network.dtMs << L","
        << network.tauMembraneMs << L"," << network.tauSynapseMs
        << L" | Synapse=" << agbnn::synapseModelName(network.synapseModel)
        << L" | Axondelay=" << network.minimumAxonDelayMs << L"..."
        << network.maximumAxonDelayMs << L" ms"
        << L" | Dendrit=" << (network.dendriteEnabled ? L"an" : L"aus")
        << L" | lokale Eligibility="
        << (network.localEligibilityEnabled ? L"an" : L"aus")
        << L"\r\n"
        << L"Konstante und Zufallsverteilung werden auf die wirksamen "
           L"Kernel-Events kalibriert.\r\n\r\n";
    for (const auto& item : results) {
        out << std::left << std::setw(15) << agbnn::gateModeName(item.gateMode)
            << L" Accuracy=" << item.meanAccuracy()
            << L" ± " << item.accuracyStddev()
            << L" | Balanced=" << item.meanBalancedAccuracy()
            << L" | Rate=" << item.meanFiringRateHz << L" Hz"
            << L" | Gate(global)=" << item.meanGate
            << L" | Gate(effektiv)=" << item.meanEffectiveGate
            << L" | Var(effektiv)="
            << item.meanEffectiveGateVariance
            << L" | H(effektiv)="
            << item.meanEffectiveGateEntropyBits << L" bit\r\n"
            << L"  Assembly-Separation=" << item.meanAssemblySeparation()
            << L" | Spikes/Sample=" << item.meanSpikesPerSample
            << L" | Spikes/korrekte Entscheidung="
            << item.spikesPerCorrectDecision << L"\r\n"
            << L"  Confusion [["
            << item.confusion[0][0] << L"," << item.confusion[0][1]
            << L"],[" << item.confusion[1][0] << L","
            << item.confusion[1][1] << L"]]\r\n";
    }
    out << L"\r\nInterpretation: Unterschiede gelten nur für diese synthetische Aufgabe.\r\n";
    return out.str();
}

void showError(HWND window, const std::exception& error) {
    std::wstring message = L"Eingabe oder Berechnung fehlgeschlagen:\n";
    const std::string narrow = error.what();
    message.append(narrow.begin(), narrow.end());
    MessageBoxW(window, message.c_str(), L"AG Bio Network", MB_ICONERROR);
}

void runSingle(HWND window) {
    try {
        const agbnn::NetworkConfig networkConfig = readNetworkConfig(window);
        agbnn::ClassificationConfig taskConfig = readTaskConfig(window);
        taskConfig.seed = networkConfig.seed;
        const int pattern = static_cast<int>(SendDlgItemMessageW(
            window, IDC_PATTERN, CB_GETCURSEL, 0, 0));
        const std::wstring progress =
            L"Simulation läuft ...\r\nKonfigurationshash: "
            + configurationHash(networkConfig, taskConfig, pattern);
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), progress.c_str());
        UpdateWindow(window);
        const auto stimulus = agbnn::makeTemporalStimulus(
            networkConfig.neuronCount,
            taskConfig.steps,
            taskConfig.timeBins,
            pattern,
            0,
            taskConfig.seed,
            taskConfig.baselineCurrent,
            taskConfig.pulseCurrent,
            taskConfig.noiseStd);
        agbnn::SpikingNetwork network(networkConfig);
        g_state.lastRun = network.run(stimulus);
        g_state.comparisons.clear();
        g_state.report = formatSingle(
            networkConfig, taskConfig, pattern, g_state.lastRun);
        g_state.view = UiState::View::Single;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), g_state.report.c_str());
        InvalidateRect(window, nullptr, TRUE);
    } catch (const std::exception& error) {
        showError(window, error);
    }
}

void runComparison(HWND window) {
    try {
        const agbnn::NetworkConfig networkConfig = readNetworkConfig(window);
        agbnn::ClassificationConfig taskConfig = readTaskConfig(window);
        taskConfig.seed = networkConfig.seed;
        const std::wstring progress =
            L"Cross-Validation läuft. Bitte warten ...\r\n"
            L"Konfigurationshash: "
            + configurationHash(networkConfig, taskConfig);
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), progress.c_str());
        UpdateWindow(window);
        const agbnn::TemporalClassifier classifier(
            networkConfig, taskConfig);
        g_state.comparisons = classifier.compareAll();
        g_state.report = formatComparison(
            networkConfig, taskConfig, g_state.comparisons);
        g_state.view = UiState::View::Comparison;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), g_state.report.c_str());
        InvalidateRect(window, nullptr, TRUE);
    } catch (const std::exception& error) {
        showError(window, error);
    }
}

std::vector<std::uint64_t> readResearchSeeds(HWND window) {
    std::wstringstream input(readText(window, IDC_RESEARCH_SEEDS));
    std::vector<std::uint64_t> seeds;
    std::wstring token;
    while (std::getline(input, token, L';')) {
        std::size_t consumed = 0;
        const unsigned long long value = std::stoull(token, &consumed);
        if (consumed != token.size()) {
            throw std::invalid_argument("Ungültiger Forschungs-Seed");
        }
        seeds.push_back(static_cast<std::uint64_t>(value));
    }
    if (seeds.size() < 2 || seeds.size() > 32) {
        throw std::invalid_argument(
            "Forschungs-Seeds: 2 bis 32 Werte erforderlich");
    }
    return seeds;
}

double vectorMean(const std::vector<double>& values) {
    return values.empty()
        ? 0.0
        : std::accumulate(values.begin(), values.end(), 0.0)
            / values.size();
}

double vectorStddev(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    const double average = vectorMean(values);
    double variance = 0.0;
    for (double value : values) {
        const double centered = value - average;
        variance += centered * centered;
    }
    return std::sqrt(variance / values.size());
}

double pairedPermutationPValue(const std::vector<double>& differences) {
    if (differences.empty()) {
        return 1.0;
    }
    const double observed = std::abs(vectorMean(differences));
    const std::size_t exactCount =
        differences.size() <= 20
            ? (static_cast<std::size_t>(1) << differences.size())
            : 65536;
    std::mt19937_64 random(0xA61B10ULL);
    std::uniform_int_distribution<int> sign(0, 1);
    std::size_t atLeastObserved = 0;
    for (std::size_t permutation = 0;
         permutation < exactCount;
         ++permutation) {
        double signedSum = 0.0;
        for (std::size_t index = 0;
             index < differences.size();
             ++index) {
            const bool positive =
                differences.size() <= 20
                    ? ((permutation >> index) & 1U) != 0
                    : sign(random) != 0;
            signedSum += positive
                ? differences[index]
                : -differences[index];
        }
        if (
            std::abs(signedSum / differences.size())
            >= observed - 1e-12) {
            ++atLeastObserved;
        }
    }
    return atLeastObserved / static_cast<double>(exactCount);
}

void runMultiSeed(HWND window) {
    try {
        const auto seeds = readResearchSeeds(window);
        const agbnn::NetworkConfig baseNetwork = readNetworkConfig(window);
        const agbnn::ClassificationConfig baseTask = readTaskConfig(window);
        const std::wstring runHash =
            configurationHash(baseNetwork, baseTask);
        const std::wstring parsedSummary =
            formatConfigurationSummary(baseNetwork, baseTask);
        const std::wstring progress =
            L"Mehrseed-Lauf und Signifikanztests laufen ...\r\n"
            L"Basiskonfigurationshash: " + runHash;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), progress.c_str());
        UpdateWindow(window);
        const std::array<agbnn::GateMode, 6> modes{
            agbnn::GateMode::Kernel,
            agbnn::GateMode::Constant,
            agbnn::GateMode::Disabled,
            agbnn::GateMode::Sign,
            agbnn::GateMode::Tanh,
            agbnn::GateMode::Random};
        std::array<std::vector<double>, 6> accuracies;
        std::array<std::vector<double>, 6> efficiencies;
        std::vector<double> ecologyAccuracies;

        for (std::uint64_t seed : seeds) {
            agbnn::NetworkConfig network = baseNetwork;
            network.seed = seed;
            agbnn::ClassificationConfig task = baseTask;
            task.seed = seed;
            const agbnn::TemporalClassifier classifier(network, task);
            const auto comparisons = classifier.compareAll();
            for (std::size_t index = 0;
                 index < comparisons.size();
                 ++index) {
                accuracies[index].push_back(
                    comparisons[index].meanAccuracy());
                efficiencies[index].push_back(
                    comparisons[index].spikesPerCorrectDecision);
            }
            if (network.classSpecificOperatorsEnabled) {
                ecologyAccuracies.push_back(
                    classifier.evaluate(network.gateMode, false)
                        .meanAccuracy());
            }
        }

        std::wostringstream out;
        out << std::fixed << std::setprecision(6);
        out << parsedSummary
            << L"MEHRSEED-FORSCHUNGSLAUF\r\n"
            << L"Basiskonfigurationshash: " << runHash << L"\r\n"
            << L"Seeds: ";
        for (std::size_t index = 0; index < seeds.size(); ++index) {
            if (index) out << L",";
            out << seeds[index];
        }
        out << L"\r\n"
            << L"Test: zweiseitige gepaarte Sign-Flip-Permutation "
               L"auf Seed-Accuracies\r\n\r\n";
        for (std::size_t modeIndex = 0;
             modeIndex < modes.size();
             ++modeIndex) {
            out << std::left << std::setw(15)
                << agbnn::gateModeName(modes[modeIndex])
                << L" Accuracy=" << vectorMean(accuracies[modeIndex])
                << L" ± " << vectorStddev(accuracies[modeIndex])
                << L" | Spikes/korrekt="
                << vectorMean(efficiencies[modeIndex]);
            if (modeIndex > 0) {
                std::vector<double> differences;
                for (std::size_t seedIndex = 0;
                     seedIndex < seeds.size();
                     ++seedIndex) {
                    differences.push_back(
                        accuracies[0][seedIndex]
                        - accuracies[modeIndex][seedIndex]);
                }
                out << L" | ΔKernel=" << vectorMean(differences)
                    << L" | p=" << pairedPermutationPValue(differences);
            }
            out << L"\r\n";
        }
        if (!ecologyAccuracies.empty()) {
            out << L"\r\nKonfigurierte EE/EI/IE/II-Ökologie: "
                << vectorMean(ecologyAccuracies) << L" ± "
                << vectorStddev(ecologyAccuracies) << L"\r\n";
        }
        out << L"\r\np-Werte sind explorativ und bei wenigen Seeds grob "
               L"aufgelöst; sie ersetzen keine externe Replikation.\r\n";
        g_state.report = out.str();
        g_state.view = UiState::View::None;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), g_state.report.c_str());
        InvalidateRect(window, nullptr, TRUE);
    } catch (const std::exception& error) {
        showError(window, error);
    }
}

void optimizeProjection(HWND window) {
    try {
        const auto seeds = readResearchSeeds(window);
        agbnn::NetworkConfig baseNetwork = readNetworkConfig(window);
        agbnn::ClassificationConfig baseTask = readTaskConfig(window);
        if (
            baseNetwork.gateTiming
                != agbnn::GateTiming::EmissionState
            || baseNetwork.emissionFeature
                != agbnn::EmissionFeature::FeatureProjection) {
            throw std::invalid_argument(
                "Optimierung benötigt EMISSION_STATE und 4-Feature-Projektion");
        }
        const std::wstring startHash =
            configurationHash(baseNetwork, baseTask);
        const std::wstring startSummary =
            formatConfigurationSummary(baseNetwork, baseTask);
        const std::wstring progress =
            L"Projektionsgewichte werden auf Trainings-Seeds optimiert ...\r\n"
            L"Startkonfigurationshash: " + startHash;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), progress.c_str());
        UpdateWindow(window);

        std::vector<std::array<double, 4>> candidates{
            {baseNetwork.projectionEiWeight,
             baseNetwork.projectionSlopeWeight,
             baseNetwork.projectionOvershootWeight,
             baseNetwork.projectionIsiWeight},
            {1.0, 0.0, 0.0, 0.0},
            {0.0, 1.0, 0.0, 0.0},
            {0.0, 0.0, 1.0, 0.0},
            {0.0, 0.0, 0.0, 1.0},
            {0.25, 0.25, 0.25, 0.25}};
        std::mt19937_64 random(0x0F71A12EULL);
        std::uniform_real_distribution<double> coefficient(-1.0, 1.0);
        for (int candidate = 0; candidate < 10; ++candidate) {
            std::array<double, 4> weights{};
            double norm = 0.0;
            for (double& value : weights) {
                value = coefficient(random);
                norm += std::abs(value);
            }
            for (double& value : weights) {
                value /= std::max(norm, 1e-12);
            }
            candidates.push_back(weights);
        }

        const std::uint64_t holdoutSeed = seeds.back();
        double bestAccuracy = -1.0;
        double bestEfficiency = std::numeric_limits<double>::infinity();
        std::array<double, 4> bestWeights = candidates.front();
        for (const auto& weights : candidates) {
            std::vector<double> trainingAccuracies;
            std::vector<double> trainingEfficiencies;
            for (std::size_t index = 0; index + 1 < seeds.size(); ++index) {
                agbnn::NetworkConfig network = baseNetwork;
                network.seed = seeds[index];
                network.projectionEiWeight = weights[0];
                network.projectionSlopeWeight = weights[1];
                network.projectionOvershootWeight = weights[2];
                network.projectionIsiWeight = weights[3];
                agbnn::ClassificationConfig task = baseTask;
                task.seed = seeds[index];
                const auto evaluation =
                    agbnn::TemporalClassifier(network, task)
                        .evaluate(agbnn::GateMode::Kernel);
                trainingAccuracies.push_back(evaluation.meanAccuracy());
                trainingEfficiencies.push_back(
                    evaluation.spikesPerCorrectDecision);
            }
            const double accuracy = vectorMean(trainingAccuracies);
            const double efficiency = vectorMean(trainingEfficiencies);
            if (
                accuracy > bestAccuracy + 1e-12
                || (
                    std::abs(accuracy - bestAccuracy) <= 1e-12
                    && efficiency < bestEfficiency)) {
                bestAccuracy = accuracy;
                bestEfficiency = efficiency;
                bestWeights = weights;
            }
        }

        baseNetwork.seed = holdoutSeed;
        baseNetwork.projectionEiWeight = bestWeights[0];
        baseNetwork.projectionSlopeWeight = bestWeights[1];
        baseNetwork.projectionOvershootWeight = bestWeights[2];
        baseNetwork.projectionIsiWeight = bestWeights[3];
        baseTask.seed = holdoutSeed;
        const auto holdoutResults =
            agbnn::TemporalClassifier(baseNetwork, baseTask).compareAll();

        std::wostringstream parameterText;
        parameterText << std::setprecision(12)
            << bestWeights[0] << L";" << bestWeights[1] << L";"
            << bestWeights[2] << L";" << bestWeights[3] << L";"
            << baseNetwork.membraneSlopeScaleMvPerMs << L";"
            << baseNetwork.thresholdOvershootScaleMv << L";"
            << baseNetwork.isiTauMs;
        SetWindowTextW(
            GetDlgItem(window, IDC_PROJECTION_PARAMETERS),
            parameterText.str().c_str());

        std::wostringstream out;
        out << std::fixed << std::setprecision(6);
        out << startSummary
            << L"PROJEKTIONSOPTIMIERUNG\r\n"
            << L"Startkonfigurationshash: " << startHash << L"\r\n"
            << L"Ergebniskonfigurationshash: "
            << configurationHash(baseNetwork, baseTask) << L"\r\n"
            << L"Trainings-Seeds: ";
        for (std::size_t index = 0; index + 1 < seeds.size(); ++index) {
            if (index) out << L",";
            out << seeds[index];
        }
        out << L" | unberührter Holdout-Seed: " << holdoutSeed << L"\r\n"
            << L"Kandidaten: " << candidates.size()
            << L" | Auswahl: maximale Trainings-Accuracy, "
               L"bei Gleichstand minimale Spikekosten\r\n"
            << L"Gewichte [E/I,Steigung,Überschuss,ISI] = ["
            << bestWeights[0] << L"," << bestWeights[1] << L","
            << bestWeights[2] << L"," << bestWeights[3] << L"]\r\n"
            << L"Training Accuracy=" << bestAccuracy
            << L" | Spikes/korrekt=" << bestEfficiency << L"\r\n\r\n"
            << L"HOLDOUT-ERGEBNISSE\r\n";
        for (const auto& result : holdoutResults) {
            out << std::left << std::setw(15)
                << agbnn::gateModeName(result.gateMode)
                << L" Accuracy=" << result.meanAccuracy()
                << L" | Spikes/korrekt="
                << result.spikesPerCorrectDecision << L"\r\n";
        }
        out << L"\r\nDie Holdout-Ergebnisse wurden nicht zur Auswahl "
               L"der Gewichte verwendet.\r\n";
        g_state.report = out.str();
        g_state.view = UiState::View::None;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), g_state.report.c_str());
        InvalidateRect(window, nullptr, TRUE);
    } catch (const std::exception& error) {
        showError(window, error);
    }
}

void runXorAblations(HWND window) {
    try {
        const auto eligibility = readDelimitedDoubles(
            window,
            IDC_ELIGIBILITY_TAUS,
            3,
            "Eligibility benötigt drei Zeitkonstanten");
        if (!std::all_of(
                eligibility.begin(),
                eligibility.end(),
                [](double value) {
                    return std::isfinite(value) && value > 0.0;
                })) {
            throw std::invalid_argument(
                "Eligibility-Zeitkonstanten müssen positiv sein");
        }
        const bool eligibilityEnabled =
            SendDlgItemMessageW(
                window, IDC_ELIGIBILITY_ENABLED, BM_GETCHECK, 0, 0)
            == BST_CHECKED;
        const bool productsEnabled =
            SendDlgItemMessageW(
                window, IDC_INTERACTION_PRODUCTS, BM_GETCHECK, 0, 0)
            == BST_CHECKED;
        const bool dendriteEnabled =
            SendDlgItemMessageW(
                window, IDC_DENDRITE_ENABLED, BM_GETCHECK, 0, 0)
            == BST_CHECKED;
        const auto localEligibility = readDelimitedDoubles(
            window,
            IDC_LOCAL_ELIGIBILITY_PARAMETERS,
            4,
            "Lokale Eligibility benötigt tau;gain;maximum;shift");
        if (
            localEligibility[0] <= 0.0
            || localEligibility[1] < 0.0
            || localEligibility[1] > 1.0
            || localEligibility[2] <= 0.0
            || localEligibility[3] <= 0.0
            || localEligibility[3] > 1000.0) {
            throw std::invalid_argument(
                "Lokale Eligibility benötigt tau>0, "
                "Gain in [0,1], Maximum>0 und Shift in (0,1000]");
        }
        const bool localEligibilityEnabled =
            SendDlgItemMessageW(
                window,
                IDC_LOCAL_ELIGIBILITY_ENABLED,
                BM_GETCHECK,
                0,
                0)
            == BST_CHECKED;
        wchar_t executablePath[MAX_PATH]{};
        if (
            GetModuleFileNameW(
                nullptr, executablePath, MAX_PATH) == 0) {
            throw std::runtime_error("UI-Pfad konnte nicht bestimmt werden");
        }
        const std::filesystem::path executable =
            std::filesystem::path(executablePath)
                .parent_path()
                / L"AGBioNetworkDelayedXor.exe";
        if (!std::filesystem::exists(executable)) {
            throw std::runtime_error(
                "AGBioNetworkDelayedXor.exe fehlt im UI-Ordner");
        }
        const std::filesystem::path outputDirectory =
            std::filesystem::path(executablePath)
                .parent_path()
                / L"xor_ui_ablation";
        std::filesystem::create_directories(outputDirectory);
        std::wostringstream tauText;
        tauText << std::setprecision(12)
            << eligibility[0] << L";" << eligibility[1] << L";"
            << eligibility[2];
        std::wostringstream command;
        command << L"\"" << executable.wstring() << L"\" \""
            << outputDirectory.wstring()
            << L"\" --memory-ablate \"" << tauText.str() << L"\" "
            << (eligibilityEnabled ? 1 : 0) << L" "
            << (productsEnabled ? 1 : 0) << L" "
            << (dendriteEnabled ? 1 : 0) << L" "
            << (localEligibilityEnabled ? 1 : 0) << L" \""
            << std::setprecision(12)
            << localEligibility[0] << L";"
            << localEligibility[1] << L";"
            << localEligibility[2] << L";"
            << localEligibility[3] << L"\"";
        std::wstring mutableCommand = command.str();
        mutableCommand.push_back(L'\0');
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT),
            L"Delayed-XOR-Einzelablationen laufen ...");
        UpdateWindow(window);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(
                nullptr,
                mutableCommand.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                executable.parent_path().c_str(),
                &startup,
                &process)) {
            throw std::runtime_error(
                "Delayed-XOR-Ablationsprozess konnte nicht gestartet werden");
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        if (exitCode != 0) {
            throw std::runtime_error(
                "Delayed-XOR-Ablationsprozess ist fehlgeschlagen");
        }
        const auto reportPath =
            outputDirectory / L"ABLATION_REPORT.md";
        std::ifstream input(reportPath, std::ios::binary);
        if (!input) {
            throw std::runtime_error(
                "Ablationsbericht wurde nicht erzeugt");
        }
        const std::string utf8(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const int required = MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8.data(),
            static_cast<int>(utf8.size()),
            nullptr,
            0);
        std::wstring report(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8.data(),
            static_cast<int>(utf8.size()),
            report.data(),
            required);
        g_state.report =
            L"DELAYED-XOR-ABLATIONEN\r\n"
            L"Eligibility: " + tauText.str() + L" ms"
            + L" | aktiv=" + (eligibilityEnabled ? L"ja" : L"nein")
            + L" | Produkte=" + (productsEnabled ? L"ja" : L"nein")
            + L" | Dendrit=" + (dendriteEnabled ? L"ja" : L"nein")
            + L" | lokale Synapsenspur="
            + (localEligibilityEnabled ? L"ja" : L"nein")
            + L"\r\nAusgabeordner: " + outputDirectory.wstring()
            + L"\r\n\r\n" + report;
        g_state.view = UiState::View::None;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), g_state.report.c_str());
        InvalidateRect(window, nullptr, TRUE);
    } catch (const std::exception& error) {
        showError(window, error);
    }
}

void runTraceEssential(HWND window) {
    try {
        wchar_t executablePath[MAX_PATH]{};
        if (
            GetModuleFileNameW(nullptr, executablePath, MAX_PATH)
            == 0) {
            throw std::runtime_error(
                "UI-Pfad konnte nicht bestimmt werden");
        }
        const std::filesystem::path executable =
            std::filesystem::path(executablePath).parent_path()
            / L"AGBioNetworkTraceEssential.exe";
        if (!std::filesystem::exists(executable)) {
            throw std::runtime_error(
                "AGBioNetworkTraceEssential.exe fehlt im UI-Ordner");
        }
        const std::filesystem::path outputDirectory =
            std::filesystem::path(executablePath).parent_path()
            / L"trace_essential_ui";
        std::filesystem::create_directories(outputDirectory);
        std::wostringstream command;
        command << L"\"" << executable.wstring() << L"\" \""
            << outputDirectory.wstring() << L"\" --full";
        std::wstring mutableCommand = command.str();
        mutableCommand.push_back(L'\0');
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT),
            L"Stufe 15 läuft: 125er Entwicklungssuche, Kontrollen "
            L"und eingefrorener Holdout ...");
        UpdateWindow(window);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(
                nullptr,
                mutableCommand.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                executable.parent_path().c_str(),
                &startup,
                &process)) {
            throw std::runtime_error(
                "Trace-essential Prozess konnte nicht gestartet werden");
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        if (exitCode != 0 && exitCode != 2) {
            throw std::runtime_error(
                "Trace-essential Prozess ist fehlgeschlagen");
        }
        const auto reportPath =
            outputDirectory / L"holdout" / L"REPORT.md";
        std::ifstream input(reportPath, std::ios::binary);
        if (!input) {
            throw std::runtime_error(
                "Trace-essential Bericht wurde nicht erzeugt");
        }
        const std::string utf8(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const int required = MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8.data(),
            static_cast<int>(utf8.size()),
            nullptr,
            0);
        std::wstring report(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            utf8.data(),
            static_cast<int>(utf8.size()),
            report.data(),
            required);
        g_state.report =
            L"FORSCHUNGSSTUFE 15\r\nAusgabeordner: "
            + outputDirectory.wstring() + L"\r\n\r\n" + report;
        g_state.view = UiState::View::None;
        SetWindowTextW(
            GetDlgItem(window, IDC_OUTPUT), g_state.report.c_str());
        InvalidateRect(window, nullptr, TRUE);
    } catch (const std::exception& error) {
        showError(window, error);
    }
}

void saveReport(HWND window) {
    if (g_state.report.empty()) {
        MessageBoxW(
            window,
            L"Zuerst eine Simulation oder Cross-Validation ausführen.",
            L"AG Bio Network",
            MB_ICONINFORMATION);
        return;
    }
    wchar_t path[MAX_PATH] = L"ag_bio_network_report.txt";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"Textbericht (*.txt)\0*.txt\0Alle Dateien\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"txt";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) {
        return;
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, g_state.report.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        g_state.report.c_str(),
        -1,
        utf8.data(),
        required,
        nullptr,
        nullptr);
    utf8.resize(static_cast<std::size_t>(required - 1));
    std::ofstream output(
        std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    output.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    if (!output) {
        MessageBoxW(window, L"Bericht konnte nicht gespeichert werden.", L"Fehler", MB_ICONERROR);
    }
}

void drawFrame(HDC dc, RECT rectangle, const wchar_t* title) {
    HBRUSH background = CreateSolidBrush(RGB(250, 252, 255));
    FillRect(dc, &rectangle, background);
    DeleteObject(background);
    HPEN border = CreatePen(PS_SOLID, 1, RGB(165, 175, 190));
    HPEN previous = static_cast<HPEN>(SelectObject(dc, border));
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rectangle.left, rectangle.top, rectangle.right, rectangle.bottom);
    SelectObject(dc, previous);
    DeleteObject(border);
    SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, rectangle.left + 10, rectangle.top + 7, title, lstrlenW(title));
}

void drawSingle(HDC dc, const RECT& area) {
    if (g_state.lastRun.spikes.empty()) {
        return;
    }
    RECT raster{area.left, area.top, area.right, area.top + 250};
    drawFrame(dc, raster, L"Spike-Raster");
    const int left = raster.left + 45;
    const int top = raster.top + 30;
    const int width = raster.right - left - 15;
    const int height = raster.bottom - top - 20;
    const int steps = static_cast<int>(g_state.lastRun.spikes.size());
    const int neurons = static_cast<int>(g_state.lastRun.spikes.front().size());
    HBRUSH spikeBrush = CreateSolidBrush(RGB(21, 97, 170));
    for (int step = 0; step < steps; ++step) {
        for (int neuron = 0; neuron < neurons; ++neuron) {
            if (!g_state.lastRun.spikes[static_cast<std::size_t>(step)]
                                        [static_cast<std::size_t>(neuron)]) {
                continue;
            }
            const int x = left + step * width / std::max(1, steps - 1);
            const int y = top + neuron * height / std::max(1, neurons - 1);
            RECT dot{x - 1, y - 1, x + 2, y + 2};
            FillRect(dc, &dot, spikeBrush);
        }
    }
    DeleteObject(spikeBrush);

    RECT trace{area.left, raster.bottom + 12, area.right, area.bottom};
    drawFrame(dc, trace, L"Membranspannung Neuron 0");
    const int traceLeft = trace.left + 45;
    const int traceTop = trace.top + 30;
    const int traceWidth = trace.right - traceLeft - 15;
    const int traceHeight = trace.bottom - traceTop - 20;
    HPEN voltagePen = CreatePen(PS_SOLID, 2, RGB(196, 68, 54));
    HPEN previous = static_cast<HPEN>(SelectObject(dc, voltagePen));
    for (int step = 0; step < steps; ++step) {
        const double voltage =
            g_state.lastRun.voltagesMv[static_cast<std::size_t>(step)][0];
        const double normalized = std::clamp((voltage + 75.0) / 30.0, 0.0, 1.0);
        const int x = traceLeft + step * traceWidth / std::max(1, steps - 1);
        const int y = traceTop + traceHeight
            - static_cast<int>(normalized * traceHeight);
        if (step == 0) {
            MoveToEx(dc, x, y, nullptr);
        } else {
            LineTo(dc, x, y);
        }
    }
    SelectObject(dc, previous);
    DeleteObject(voltagePen);
}

void drawComparison(HDC dc, const RECT& area) {
    if (g_state.comparisons.empty()) {
        return;
    }
    drawFrame(dc, area, L"Cross-Validation Accuracy");
    const int chartLeft = area.left + 70;
    const int chartTop = area.top + 35;
    const int chartBottom = area.bottom - 55;
    const int chartWidth = area.right - chartLeft - 25;
    const int chartHeight = chartBottom - chartTop;
    const int slot = chartWidth / static_cast<int>(g_state.comparisons.size());
    HBRUSH barBrush = CreateSolidBrush(RGB(47, 126, 121));
    SetBkMode(dc, TRANSPARENT);
    for (std::size_t index = 0; index < g_state.comparisons.size(); ++index) {
        const auto& item = g_state.comparisons[index];
        const int barHeight = static_cast<int>(
            std::clamp(item.meanAccuracy(), 0.0, 1.0) * chartHeight);
        const int x0 = chartLeft + static_cast<int>(index) * slot + 12;
        RECT bar{
            x0,
            chartBottom - barHeight,
            x0 + std::max(12, slot - 24),
            chartBottom};
        FillRect(dc, &bar, barBrush);
        std::wostringstream value;
        value << std::fixed << std::setprecision(3) << item.meanAccuracy();
        const std::wstring valueText = value.str();
        TextOutW(
            dc,
            x0,
            bar.top - 20,
            valueText.c_str(),
            static_cast<int>(valueText.size()));
        const wchar_t* name = agbnn::gateModeName(item.gateMode);
        TextOutW(
            dc,
            x0,
            chartBottom + 8,
            name,
            std::min(10, lstrlenW(name)));
    }
    DeleteObject(barBrush);
    HPEN axis = CreatePen(PS_SOLID, 1, RGB(90, 90, 90));
    HPEN previous = static_cast<HPEN>(SelectObject(dc, axis));
    MoveToEx(dc, chartLeft, chartTop, nullptr);
    LineTo(dc, chartLeft, chartBottom);
    LineTo(dc, area.right - 20, chartBottom);
    SelectObject(dc, previous);
    DeleteObject(axis);
}

void attachValueTooltip(
    HWND parent,
    HWND control,
    const wchar_t* description) {
    if (!g_state.tooltip || !control) {
        return;
    }
    g_state.tooltipDescriptions[control] = description;
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.hwnd = parent;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = LPSTR_TEXTCALLBACKW;
    SendMessageW(
        g_state.tooltip,
        TTM_ADDTOOLW,
        0,
        reinterpret_cast<LPARAM>(&tool));
}

void addValidationMarker(
    HWND window,
    int fieldId,
    int x,
    int y) {
    HWND marker = CreateWindowExW(
        0,
        L"STATIC",
        L"?",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        x,
        y + 3,
        24,
        22,
        window,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(
        marker,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_state.font),
        TRUE);
    g_state.validationMarkers[fieldId] = marker;
    g_state.markerValidity[marker] = false;
}

bool allFinite(const std::vector<double>& values) {
    return std::all_of(
        values.begin(),
        values.end(),
        [](double value) { return std::isfinite(value); });
}

bool validateMultiField(HWND window, int id) {
    try {
        switch (id) {
            case IDC_PROJECTION_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 7, "Projektion");
                return allFinite(values)
                    && values[4] > 0.0
                    && values[5] > 0.0
                    && values[6] > 0.0;
            }
            case IDC_MEMBRANE_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 7, "Membran");
                return allFinite(values)
                    && values[0] > 0.0
                    && values[1] > 0.0
                    && values[2] > 0.0
                    && values[5] > values[3]
                    && values[4] <= values[3]
                    && values[6] >= 0.0;
            }
            case IDC_EI_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 4, "E/I");
                return allFinite(values)
                    && values[0] > 0.0
                    && values[0] <= 1.0
                    && values[1] >= 0.0
                    && values[2] >= 0.0
                    && values[3] > 0.0;
            }
            case IDC_ADAPTATION_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 2, "Adaptation");
                return allFinite(values) && values[1] > 0.0;
            }
            case IDC_TASK_INTERNALS: {
                const auto values = readDelimitedDoubles(
                    window, id, 2, "Aufgabe");
                return allFinite(values)
                    && std::abs(values[1] - std::round(values[1])) <= 1e-9
                    && values[1] >= 2.0
                    && values[1] <= 32.0;
            }
            case IDC_READOUT_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 3, "Readout");
                return allFinite(values)
                    && values[0] > 0.0
                    && std::abs(values[1] - std::round(values[1])) <= 1e-9
                    && values[1] > 0.0
                    && values[2] >= 0.0;
            }
            case IDC_STDP_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 4, "STDP");
                return allFinite(values)
                    && values[0] >= 0.0
                    && values[1] > 0.0
                    && values[2] >= 0.0
                    && values[3] >= 0.0;
            }
            case IDC_GATE_INTERNALS: {
                const auto values = readDelimitedDoubles(
                    window, id, 2, "Gate");
                return allFinite(values)
                    && values[0] > 0.0
                    && values[1] >= 0.0
                    && values[1] <= 1.0;
            }
            case IDC_AXON_DELAYS: {
                const auto values = readDelimitedDoubles(
                    window, id, 2, "Axon");
                const auto membrane = readDelimitedDoubles(
                    window,
                    IDC_MEMBRANE_PARAMETERS,
                    7,
                    "Membran");
                return allFinite(values)
                    && values[0] >= membrane[0]
                    && values[1] >= values[0]
                    && values[1] <= 1000.0;
            }
            case IDC_CONDUCTANCE_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 4, "AMPA/GABA");
                return allFinite(values)
                    && values[2] >= 0.0
                    && values[3] >= 0.0;
            }
            case IDC_DENDRITE_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 3, "Dendrit");
                return allFinite(values)
                    && values[0] > 0.0
                    && values[1] >= 0.0
                    && values[2] >= 0.0
                    && values[2] <= 1.0;
            }
            case IDC_CLASS_OPERATORS: {
                std::wstringstream input(readText(window, id));
                std::wstring token;
                int count = 0;
                while (std::getline(input, token, L';')) {
                    (void)parseGateMode(token);
                    ++count;
                }
                return count == 4;
            }
            case IDC_RESEARCH_SEEDS:
                (void)readResearchSeeds(window);
                return true;
            case IDC_ELIGIBILITY_TAUS: {
                const auto values = readDelimitedDoubles(
                    window, id, 3, "Eligibility");
                return allFinite(values)
                    && std::all_of(
                        values.begin(),
                        values.end(),
                        [](double value) { return value > 0.0; });
            }
            case IDC_LOCAL_ELIGIBILITY_PARAMETERS: {
                const auto values = readDelimitedDoubles(
                    window, id, 4, "Lokale Eligibility");
                return allFinite(values)
                    && values[0] > 0.0
                    && values[1] >= 0.0
                    && values[1] <= 1.0
                    && values[2] > 0.0
                    && values[3] > 0.0
                    && values[3] <= 1000.0;
            }
            default:
                return true;
        }
    } catch (const std::exception&) {
        return false;
    }
}

void refreshValidationMarkers(HWND window) {
    for (const auto& entry : g_state.validationMarkers) {
        const bool valid = validateMultiField(window, entry.first);
        g_state.markerValidity[entry.second] = valid;
        SetWindowTextW(entry.second, valid ? L"\u2713" : L"!");
        InvalidateRect(entry.second, nullptr, TRUE);
    }
}

HWND addControl(
    HWND parent,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    int id) {
    HWND control = CreateWindowExW(
        lstrcmpW(className, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0,
        className,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_state.font), TRUE);
    return control;
}

HWND addInput(
    HWND window,
    const wchar_t* label,
    const wchar_t* value,
    int id,
    int y) {
    addControl(window, L"STATIC", label, 0, 18, y + 3, 155, 22, 0);
    return addControl(
        window,
        L"EDIT",
        value,
        ES_AUTOHSCROLL | ES_RIGHT,
        178,
        y,
        125,
        24,
        id);
}

HWND addInputAt(
    HWND window,
    const wchar_t* label,
    const wchar_t* value,
    int id,
    int x,
    int y,
    const wchar_t* tooltipDescription) {
    addControl(window, L"STATIC", label, 0, x, y + 3, 200, 22, 0);
    HWND edit = addControl(
        window,
        L"EDIT",
        value,
        ES_AUTOHSCROLL | ES_RIGHT,
        x + 205,
        y,
        200,
        24,
        id);
    addValidationMarker(window, id, x + 410, y);
    attachValueTooltip(window, edit, tooltipDescription);
    return edit;
}

void createControls(HWND window) {
    g_state.font = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_state.tooltip = CreateWindowExW(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASSW,
        nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(g_state.tooltip, TTM_SETMAXTIPWIDTH, 0, 720);
    SendMessageW(g_state.tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 12000);
    addControl(window, L"STATIC", L"Netzwerk und Aufgabe", SS_CENTER, 16, 14, 290, 24, 0);
    addControl(window, L"STATIC", L"Biophysik und Forschungsautomation", SS_CENTER, 334, 14, 440, 24, 0);
    addInput(window, L"Neuronen", L"16", IDC_NEURONS, 48);
    addInput(window, L"Schritte", L"120", IDC_STEPS, 78);
    addInput(window, L"Seed", L"38", IDC_SEED, 108);
    addInput(window, L"Rauschen σ", L"5.0", IDC_NOISE, 138);
    addInput(window, L"Pulshöhe", L"2.0", IDC_PULSE, 168);
    addInput(window, L"Verbindungsdichte", L"0.18", IDC_DENSITY, 198);
    addInput(
        window,
        L"Konstantes Gate",
        L"0.12831112128784755",
        IDC_CONSTANT_GATE,
        228);
    addInput(window, L"Samples/Klasse", L"24", IDC_SAMPLES, 258);
    addInput(window, L"CV-Folds", L"4", IDC_FOLDS, 288);

    addControl(window, L"STATIC", L"Gate-Modus", 0, 18, 321, 155, 22, 0);
    HWND gate = addControl(
        window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
        178, 318, 125, 200, IDC_GATE_MODE);
    for (agbnn::GateMode mode : {
             agbnn::GateMode::Kernel,
             agbnn::GateMode::Constant,
             agbnn::GateMode::Disabled,
             agbnn::GateMode::Sign,
             agbnn::GateMode::Tanh,
             agbnn::GateMode::Random}) {
        SendMessageW(gate, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(agbnn::gateModeName(mode)));
    }
    SendMessageW(gate, CB_SETCURSEL, 0, 0);

    addControl(window, L"STATIC", L"Gate-Timing", 0, 18, 353, 155, 22, 0);
    HWND timing = addControl(
        window, L"COMBOBOX", L"", CBS_DROPDOWNLIST,
        178, 350, 125, 100, IDC_GATE_TIMING);
    SendMessageW(timing, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"RESET_LOCKED"));
    SendMessageW(timing, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"EMISSION_STATE"));
    SendMessageW(timing, CB_SETCURSEL, 1, 0);

    addControl(window, L"STATIC", L"Timing-Kontrolle", 0, 18, 385, 155, 22, 0);
    HWND perturbation = addControl(
        window, L"COMBOBOX", L"", CBS_DROPDOWNLIST,
        178, 382, 125, 120, IDC_GATE_PERTURBATION);
    SendMessageW(perturbation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Keine"));
    SendMessageW(perturbation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Zeitverschoben"));
    SendMessageW(perturbation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"State-shuffled"));
    SendMessageW(perturbation, CB_SETCURSEL, 0, 0);

    addControl(window, L"STATIC", L"Emissionsfeature", 0, 18, 417, 155, 22, 0);
    HWND emissionFeature = addControl(
        window, L"COMBOBOX", L"", CBS_DROPDOWNLIST,
        178, 414, 125, 100, IDC_EMISSION_FEATURE);
    SendMessageW(
        emissionFeature,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"Pre-reset Spannung"));
    SendMessageW(
        emissionFeature,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"E/I-Balance"));
    SendMessageW(
        emissionFeature,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"4-Feature-Projektion"));
    SendMessageW(emissionFeature, CB_SETCURSEL, 2, 0);

    HWND projection = addInput(
        window,
        L"aEI;aV;aO;aISI;sV;sO;tISI",
        L"0.40;0.25;0.15;0.20;1.0;1.0;50.0",
        IDC_PROJECTION_PARAMETERS,
        446);
    addValidationMarker(window, IDC_PROJECTION_PARAMETERS, 306, 446);
    attachValueTooltip(
        window,
        projection,
        L"Vollständiger Projektionsvektor: Gewichte für E/I-Balance, "
        L"Membransteigung, Schwellenüberschuss und ISI; danach deren "
        L"Skalen sV, sO und tauISI. Semikolon als Trenner.");

    addControl(window, L"STATIC", L"Einzelmuster", 0, 18, 481, 155, 22, 0);
    HWND pattern = addControl(
        window, L"COMBOBOX", L"", CBS_DROPDOWNLIST,
        178, 478, 125, 100, IDC_PATTERN);
    SendMessageW(pattern, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"A: 0-1-0-1"));
    SendMessageW(pattern, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"B: 1-0-1-0"));
    SendMessageW(pattern, CB_SETCURSEL, 0, 0);

    HWND plasticity = addControl(
        window, L"BUTTON", L"Lokale STDP-Plastizität",
        BS_AUTOCHECKBOX, 18, 514, 285, 24, IDC_PLASTICITY);
    SendMessageW(plasticity, BM_SETCHECK, BST_CHECKED, 0);

    addControl(
        window, L"BUTTON", L"Einzelsimulation",
        BS_PUSHBUTTON, 18, 550, 137, 34, IDC_SINGLE);
    addControl(
        window, L"BUTTON", L"Alle Gates vergleichen",
        BS_DEFPUSHBUTTON, 166, 550, 137, 34, IDC_COMPARE);
    addControl(
        window, L"BUTTON", L"Bericht speichern ...",
        BS_PUSHBUTTON, 18, 594, 285, 32, IDC_SAVE);
    addControl(
        window,
        L"STATIC",
        L"Synthetisches Forschungsmodell – keine biologische Validierung.",
        SS_CENTER,
        18,
        650,
        285,
        42,
        0);

    constexpr int advancedX = 334;
    addInputAt(
        window,
        L"dt;tM;tS;Vrest;Vreset;Vth;refr",
        L"1;20;5;-65;-70;-50;2",
        IDC_MEMBRANE_PARAMETERS,
        advancedX,
        48,
        L"Zeitschritt, Membran- und Synapsenzeitkonstante, Ruhe-, Reset- "
        L"und Schwellenpotential sowie Refraktärzeit; Einheiten ms/mV.");
    addInputAt(
        window,
        L"E-Anteil;wE;wI;wMax",
        L"0.8;13;17;30",
        IDC_EI_PARAMETERS,
        advancedX,
        78,
        L"Exzitatorischer Neuronenanteil, positive E- und I-Beträge und "
        L"maximaler Betrag plastischer Synapsengewichte.");
    addInputAt(
        window,
        L"Adapt Δθ;tau",
        L"1.5;80",
        IDC_ADAPTATION_PARAMETERS,
        advancedX,
        108,
        L"Schwellenerhöhung pro Spike in mV und ihre Abklingzeit in ms.");
    addInputAt(
        window,
        L"Basisstrom;Zeitfenster",
        L"15.0;4",
        IDC_TASK_INTERNALS,
        advancedX,
        138,
        L"Externer Basisstrom der Aufgabe und ganzzahlige Zahl der "
        L"Zeitfenster, aus denen auch die Readout-Dimension entsteht.");
    addInputAt(
        window,
        L"Readout lr;Epochen;L2",
        L"0.18;350;0.002",
        IDC_READOUT_PARAMETERS,
        advancedX,
        168,
        L"Lernrate, ganzzahlige Trainingsepochen und L2-Regularisierung "
        L"des überwachten linearen Readouts.");
    addInputAt(
        window,
        L"STDP lr;tau;A+;A-",
        L"0.01;20;1.0;1.05",
        IDC_STDP_PARAMETERS,
        advancedX,
        198,
        L"Lernrate, Spurzeitkonstante, Potenzierungs- und "
        L"Depressionsfaktor der lokalen STDP.");
    addInputAt(
        window,
        L"Gate Skala;Zufallsamplitude",
        L"1.0;0.35",
        IDC_GATE_INTERNALS,
        advancedX,
        228,
        L"Eingangsskala des algorithmischen Operators und maximale "
        L"Amplitude der Zufallskontrolle.");
    addInputAt(
        window,
        L"Axon delay min;max ms",
        L"1;5",
        IDC_AXON_DELAYS,
        advancedX,
        258,
        L"Minimale und maximale individuelle Axonverzögerung in ms. "
        L"Die Werte werden auf ganzzahlige Simulationsschritte abgebildet.");

    addControl(window, L"STATIC", L"Synapsenmodell", 0, advancedX, 291, 180, 22, 0);
    HWND synapseModel = addControl(
        window,
        L"COMBOBOX",
        L"",
        CBS_DROPDOWNLIST,
        advancedX + 205,
        288,
        200,
        100,
        IDC_SYNAPSE_MODEL);
    SendMessageW(
        synapseModel,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"Strombasiert"));
    SendMessageW(
        synapseModel,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"AMPA/GABA"));
    SendMessageW(synapseModel, CB_SETCURSEL, 1, 0);
    addInputAt(
        window,
        L"E_AMPA;E_GABA;sE;sI",
        L"0;-75;0.02;0.02",
        IDC_CONDUCTANCE_PARAMETERS,
        advancedX,
        318,
        L"AMPA- und GABA-Umkehrpotential in mV sowie E- und "
        L"I-Leitwertskalierung.");

    HWND dendrite = addControl(
        window,
        L"BUTTON",
        L"Passives Dendritenkompartiment",
        BS_AUTOCHECKBOX,
        advancedX,
        350,
        305,
        24,
        IDC_DENDRITE_ENABLED);
    SendMessageW(dendrite, BM_SETCHECK, BST_CHECKED, 0);
    addInputAt(
        window,
        L"Dendrit tau;Kopplung;Inputanteil",
        L"30;0.20;0.0",
        IDC_DENDRITE_PARAMETERS,
        advancedX,
        380,
        L"Zeitkonstante des passiven Dendritenkompartiments, "
        L"Soma-Dendrit-Kopplung und Anteil externen Inputs am Dendriten.");

    HWND classOperators = addControl(
        window,
        L"BUTTON",
        L"Operatoren nach EE/EI/IE/II",
        BS_AUTOCHECKBOX,
        advancedX,
        412,
        305,
        24,
        IDC_CLASS_OPERATORS_ENABLED);
    SendMessageW(classOperators, BM_SETCHECK, BST_CHECKED, 0);
    addInputAt(
        window,
        L"EE;EI;IE;II",
        L"kernel;kernel;kernel;kernel",
        IDC_CLASS_OPERATORS,
        advancedX,
        442,
        L"Operator je Verbindungsklasse in der Reihenfolge EE;EI;IE;II. "
        L"Zulässig: kernel, constant, disabled, sign, tanh, random.");
    addInputAt(
        window,
        L"Forschungs-Seeds",
        L"11;23;38;53;71",
        IDC_RESEARCH_SEEDS,
        advancedX,
        472,
        L"Semikolongetrennte Seedliste für Mehrseed- und "
        L"Projektionsoptimierung; 2 bis 32 Seeds.");
    addControl(
        window,
        L"BUTTON",
        L"Mehrseed + Signifikanz",
        BS_PUSHBUTTON,
        advancedX,
        518,
        210,
        34,
        IDC_MULTI_SEED);
    addControl(
        window,
        L"BUTTON",
        L"Gewichte optimieren",
        BS_PUSHBUTTON,
        advancedX + 220,
        518,
        210,
        34,
        IDC_OPTIMIZE);
    addInputAt(
        window,
        L"Eligibility tau ms",
        L"50;100;200",
        IDC_ELIGIBILITY_TAUS,
        advancedX,
        562,
        L"Drei positive Zeitkonstanten der cue-gebundenen "
        L"Eligibility-Memory in Millisekunden.");
    HWND eligibilityEnabled = addControl(
        window,
        L"BUTTON",
        L"Eligibility-Memory aktiv",
        BS_AUTOCHECKBOX,
        advancedX,
        594,
        210,
        24,
        IDC_ELIGIBILITY_ENABLED);
    SendMessageW(
        eligibilityEnabled, BM_SETCHECK, BST_CHECKED, 0);
    HWND productsEnabled = addControl(
        window,
        L"BUTTON",
        L"Interaktionsprodukte aktiv",
        BS_AUTOCHECKBOX,
        advancedX + 220,
        594,
        210,
        24,
        IDC_INTERACTION_PRODUCTS);
    SendMessageW(productsEnabled, BM_SETCHECK, BST_CHECKED, 0);
    addControl(
        window,
        L"BUTTON",
        L"Delayed-XOR Einzelablationen",
        BS_PUSHBUTTON,
        advancedX,
        630,
        430,
        34,
        IDC_XOR_ABLATIONS);
    addInputAt(
        window,
        L"Synapsen-Eligibility",
        L"100;0.35;4;40",
        IDC_LOCAL_ELIGIBILITY_PARAMETERS,
        advancedX,
        670,
        L"tau ms;Übertragungs-Gain;Betragsmaximum;Zeitverschiebung "
        L"der lokalen synapsenspezifischen Eligibility-Spur.");
    HWND localEligibilityEnabled = addControl(
        window,
        L"BUTTON",
        L"Lokale Synapsen-Eligibility aktiv",
        BS_AUTOCHECKBOX,
        advancedX,
        702,
        430,
        24,
        IDC_LOCAL_ELIGIBILITY_ENABLED);
    SendMessageW(
        localEligibilityEnabled, BM_SETCHECK, BST_CHECKED, 0);
    addControl(
        window,
        L"BUTTON",
        L"Stufe 15: Trace-essential Memory",
        BS_PUSHBUTTON,
        advancedX,
        732,
        430,
        34,
        IDC_TRACE_ESSENTIAL);

    addControl(
        window,
        L"EDIT",
        L"Parameter einstellen und Simulation starten.",
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        kPanelWidth + 20,
        480,
        840,
        330,
        IDC_OUTPUT);
    refreshValidationMarkers(window);
}

LRESULT CALLBACK windowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            createControls(window);
            return 0;
        case WM_NOTIFY: {
            const auto* notification =
                reinterpret_cast<const NMHDR*>(lParam);
            if (
                notification
                && notification->code == TTN_GETDISPINFOW) {
                auto* tooltipInfo =
                    reinterpret_cast<NMTTDISPINFOW*>(lParam);
                const HWND control =
                    reinterpret_cast<HWND>(notification->idFrom);
                const auto found =
                    g_state.tooltipDescriptions.find(control);
                if (found != g_state.tooltipDescriptions.end()) {
                    const int length = GetWindowTextLengthW(control);
                    std::wstring current(
                        static_cast<std::size_t>(length + 1),
                        L'\0');
                    GetWindowTextW(control, current.data(), length + 1);
                    current.resize(static_cast<std::size_t>(length));
                    g_state.tooltipBuffer =
                        L"Aktueller vollständiger Wert:\r\n"
                        + current + L"\r\n\r\n" + found->second;
                    tooltipInfo->lpszText =
                        g_state.tooltipBuffer.data();
                }
                return 0;
            }
            break;
        }
        case WM_COMMAND:
            if (
                HIWORD(wParam) == EN_CHANGE
                && g_state.validationMarkers.contains(LOWORD(wParam))) {
                refreshValidationMarkers(window);
                return 0;
            }
            switch (LOWORD(wParam)) {
                case IDC_SINGLE:
                    runSingle(window);
                    return 0;
                case IDC_COMPARE:
                    runComparison(window);
                    return 0;
                case IDC_SAVE:
                    saveReport(window);
                    return 0;
                case IDC_MULTI_SEED:
                    runMultiSeed(window);
                    return 0;
                case IDC_OPTIMIZE:
                    optimizeProjection(window);
                    return 0;
                case IDC_XOR_ABLATIONS:
                    runXorAblations(window);
                    return 0;
                case IDC_TRACE_ESSENTIAL:
                    runTraceEssential(window);
                    return 0;
            }
            break;
        case WM_CTLCOLORSTATIC: {
            const HWND control = reinterpret_cast<HWND>(lParam);
            const auto marker = g_state.markerValidity.find(control);
            if (marker != g_state.markerValidity.end()) {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetTextColor(
                    dc,
                    marker->second
                        ? RGB(16, 128, 62)
                        : RGB(196, 45, 45));
                SetBkMode(dc, TRANSPARENT);
                return reinterpret_cast<LRESULT>(
                    GetStockObject(NULL_BRUSH));
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            SelectObject(dc, g_state.font);
            RECT area{kPanelWidth + 20, 20, kPanelWidth + 860, 460};
            if (g_state.view == UiState::View::Single) {
                drawSingle(dc, area);
            } else if (g_state.view == UiState::View::Comparison) {
                drawComparison(dc, area);
            } else {
                drawFrame(dc, area, L"Visualisierung");
                const wchar_t* hint =
                    L"Einzelsimulation: Spike-Raster und Spannung | Vergleich: Accuracy-Balken";
                TextOutW(dc, area.left + 25, area.top + 60, hint, lstrlenW(hint));
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            if (g_state.font) {
                DeleteObject(g_state.font);
                g_state.font = nullptr;
            }
            g_state.tooltip = nullptr;
            g_state.tooltipDescriptions.clear();
            g_state.validationMarkers.clear();
            g_state.markerValidity.clear();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand) {
    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_WIN95_CLASSES;
    if (!InitCommonControlsEx(&commonControls)) {
        return 1;
    }
    const wchar_t* className = L"AGBioNetworkWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass)) {
        return 1;
    }
    HWND window = CreateWindowExW(
        0,
        className,
        L"TATARUS – A Persistent Synthetic Nervous System | Research UI",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1690,
        860,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!window) {
        return 2;
    }
    ShowWindow(window, showCommand);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
