#include "nervous_system.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>

#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kEditBase = 1000;
constexpr int kNew = 2001;
constexpr int kRun = 2002;
constexpr int kDamage = 2003;
constexpr int kSave = 2004;
constexpr int kLoad = 2005;
constexpr int kExport = 2006;
constexpr int kLab = 2007;
constexpr int kConfirmEndGoal = 2008;
constexpr int kPersistentAITrial = 2009;
constexpr int kStage20To23 = 2010;
constexpr int kOutput = 3000;
constexpr int kToggleBase = 4000;

struct Field {
    const wchar_t* label;
    const wchar_t* value;
    HWND edit = nullptr;
};

std::vector<Field> gFields{
    {L"Populationen S;E;I;Kontext;Motor;Mod", L"16;40;12;16;8;4"},
    {L"Seed", L"7001"},
    {L"dt [ms]", L"1"},
    {L"Verbindungswahrscheinlichkeit", L"0.07"},
    {L"Ruhe;Reset;Schwelle [mV]", L"-65;-70;-50"},
    {L"Tau Soma;Dendrit [ms]", L"20;35"},
    {L"Soma-Dendrit-Kopplung", L"0.22"},
    {L"Tau AMPA;NMDA;GABA-A;GABA-B [ms]", L"5;80;10;120"},
    {L"Umkehr AMPA;NMDA;GABA-A;GABA-B [mV]", L"0;0;-75;-95"},
    {L"Refraktaerzeit [ms]", L"2"},
    {L"Adaptationshub;Tau [mV;ms]", L"1.2;100"},
    {L"Zielrate;Homeostase-Tau;Gain", L"8;2000;0.003"},
    {L"Eligibility-Tau;Transfer;Inkrement;Lernrate;Konsolidierung", L"400;1;8;0.0008;0.00002"},
    {L"Ressourcen-Tau;Facilitation-Tau;Release", L"180;120;0.18"},
    {L"Energieerholung;Spike-/Transferkosten", L"0.0015;0.025;0.0004"},
    {L"Strukturintervall;Prune Usage;Gewicht", L"500;0.0001;0.004"},
    {L"Neue Synapsen;Assemblies;Aehnlichkeit", L"8;64;0.68"},
    {L"Schritte je Fortsetzung", L"1000"},
    {L"Roher UTF-8-Textstrom", L"seek target; preserve energy"},
    {L"Basisstrom", L"12.5"},
    {L"Dopamin-Tau;Acetylcholin-Tau [ms]", L"250;180"},
    {L"Motorische Ratenskalierung [Hz]", L"20"}
};

const wchar_t* gToggleLabels[] = {
    L"Generated Operator", L"Eligibility-Memory", L"Kurzzeitplastizitaet",
    L"Langzeitplastizitaet", L"Homeostase", L"Strukturplastizitaet",
    L"Energiehaushalt"
};
HWND gToggles[7]{};
HWND gOutput = nullptr;
std::unique_ptr<agns::PersistentNervousSystem> gSystem;
std::unique_ptr<agns::ContinuousEnvironment> gEnvironment;

std::wstring readText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(length), L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    return text;
}

std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::vector<double> numbers(HWND control, std::size_t expected) {
    std::wstring text = readText(control);
    for (auto& ch : text) if (ch == L';') ch = L' ';
    std::wistringstream input(text);
    std::vector<double> result;
    double value = 0.0;
    while (input >> value) result.push_back(value);
    if (result.size() != expected) {
        throw std::invalid_argument("Feld besitzt nicht die erwartete Anzahl Zahlen.");
    }
    return result;
}

int integer(HWND control) {
    return static_cast<int>(numbers(control, 1).front());
}

agns::NervousSystemConfig readConfig() {
    agns::NervousSystemConfig c;
    auto v = numbers(gFields[0].edit, 6);
    c.sensoryNeurons = static_cast<int>(v[0]);
    c.excitatoryNeurons = static_cast<int>(v[1]);
    c.inhibitoryNeurons = static_cast<int>(v[2]);
    c.contextNeurons = static_cast<int>(v[3]);
    c.motorNeurons = static_cast<int>(v[4]);
    c.modulatoryNeurons = static_cast<int>(v[5]);
    c.seed = static_cast<std::uint64_t>(numbers(gFields[1].edit, 1)[0]);
    c.dtMs = numbers(gFields[2].edit, 1)[0];
    c.connectionProbability = numbers(gFields[3].edit, 1)[0];
    v = numbers(gFields[4].edit, 3);
    c.restingMv = v[0]; c.resetMv = v[1]; c.thresholdMv = v[2];
    v = numbers(gFields[5].edit, 2);
    c.tauSomaMs = v[0]; c.tauDendriteMs = v[1];
    c.somaDendriteCoupling = numbers(gFields[6].edit, 1)[0];
    c.baseCurrent = numbers(gFields[19].edit, 1)[0];
    v = numbers(gFields[20].edit, 2);
    c.dopamineTauMs = v[0]; c.acetylcholineTauMs = v[1];
    c.motorRateScaleHz = numbers(gFields[21].edit, 1)[0];
    v = numbers(gFields[7].edit, 4);
    c.tauAmpaMs = v[0]; c.tauNmdaMs = v[1]; c.tauGabaAMs = v[2]; c.tauGabaBMs = v[3];
    v = numbers(gFields[8].edit, 4);
    c.ampaReversalMv = v[0]; c.nmdaReversalMv = v[1];
    c.gabaAReversalMv = v[2]; c.gabaBReversalMv = v[3];
    c.refractoryMs = numbers(gFields[9].edit, 1)[0];
    v = numbers(gFields[10].edit, 2);
    c.adaptationIncrementMv = v[0]; c.adaptationTauMs = v[1];
    v = numbers(gFields[11].edit, 3);
    c.targetRateHz = v[0]; c.homeostasisTauMs = v[1]; c.homeostasisGain = v[2];
    v = numbers(gFields[12].edit, 5);
    c.eligibilityTauMs = v[0]; c.eligibilityTransmissionGain = v[1];
    c.eligibilityIncrement = v[2];
    c.learningRate = v[3]; c.consolidationRate = v[4];
    v = numbers(gFields[13].edit, 3);
    c.resourceRecoveryTauMs = v[0]; c.facilitationTauMs = v[1]; c.releaseProbability = v[2];
    v = numbers(gFields[14].edit, 3);
    c.energyRecoveryPerMs = v[0]; c.spikeEnergyCost = v[1]; c.transmissionEnergyCost = v[2];
    v = numbers(gFields[15].edit, 3);
    c.structuralIntervalMs = v[0]; c.pruneUsageThreshold = v[1]; c.pruneWeightThreshold = v[2];
    v = numbers(gFields[16].edit, 3);
    c.maximumNewSynapsesPerInterval = static_cast<int>(v[0]);
    c.maximumAssemblies = static_cast<int>(v[1]); c.assemblySimilarityThreshold = v[2];
    c.generatedOperatorEnabled = Button_GetCheck(gToggles[0]) == BST_CHECKED;
    c.eligibilityMemoryEnabled = Button_GetCheck(gToggles[1]) == BST_CHECKED;
    c.shortTermPlasticityEnabled = Button_GetCheck(gToggles[2]) == BST_CHECKED;
    c.longTermPlasticityEnabled = Button_GetCheck(gToggles[3]) == BST_CHECKED;
    c.homeostasisEnabled = Button_GetCheck(gToggles[4]) == BST_CHECKED;
    c.structuralPlasticityEnabled = Button_GetCheck(gToggles[5]) == BST_CHECKED;
    c.energyRegulationEnabled = Button_GetCheck(gToggles[6]) == BST_CHECKED;
    c.validate();
    return c;
}

void output(const std::wstring& text) {
    SetWindowTextW(gOutput, text.c_str());
}

std::wstring metricsText(const agns::ClosedLoopResult& result) {
    const auto& m = gSystem->metrics();
    std::wostringstream out;
    out << std::fixed << std::setprecision(6)
        << L"Persistenter Zustand erfolgreich fortgesetzt.\r\n\r\n"
        << L"Globaler Schritt: " << m.step << L"\r\n"
        << L"Spikes gesamt: " << m.totalSpikes << L"\r\n"
        << L"Uebertragungen: " << m.totalTransmissions << L"\r\n"
        << L"Mittlere Rate: " << m.meanRateHz << L" Hz\r\n"
        << L"Mittlere Energie: " << m.meanEnergy << L"\r\n"
        << L"Dopamin / Acetylcholin: " << m.dopamine << L" / " << m.acetylcholine << L"\r\n"
        << L"Eligibility / Ressource: " << m.meanEligibility << L" / " << m.meanResource << L"\r\n"
        << L"Assemblies: " << m.assemblyCount << L" (aktiv " << m.activeAssembly << L")\r\n"
        << L"Aktive Synapsen: " << m.activeSynapses << L"\r\n"
        << L"Struktur Wachstum / Pruning: " << m.structuralGrowth << L" / " << m.structuralPruning << L"\r\n"
        << L"Umweltdistanz: " << result.finalDistance << L"\r\n"
        << L"Kumulative Belohnung: " << result.cumulativeReward << L"\r\n"
        << L"Dale-konform: " << (gSystem->dalePrincipleHolds() ? L"JA" : L"NEIN") << L"\r\n"
        << L"State Hash: " << result.stateHash;
    return out.str();
}

std::filesystem::path chooseFile(HWND owner, bool save) {
    wchar_t buffer[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"AG Nervous Snapshot (*.agns)\0*.agns\0Alle Dateien\0*.*\0";
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"agns";
    dialog.Flags = OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    const BOOL ok = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    return ok ? std::filesystem::path(buffer) : std::filesystem::path{};
}

void showError(HWND window, const std::exception& error) {
    const std::string text = error.what();
    const std::wstring wide(text.begin(), text.end());
    MessageBoxW(window, wide.c_str(), L"AGNervousSystemLab", MB_ICONERROR);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    try {
        if (message == WM_CREATE) {
            const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            for (std::size_t i = 0; i < gFields.size(); ++i) {
                const int column = i < 10 ? 0 : 1;
                const int row = column == 0 ? static_cast<int>(i) : static_cast<int>(i - 10);
                const int x = 12 + column * 475;
                HWND label = CreateWindowW(L"STATIC", gFields[i].label, WS_CHILD | WS_VISIBLE,
                    x, 12 + row * 52, 225, 18, window, nullptr, nullptr, nullptr);
                gFields[i].edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", gFields[i].value,
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x + 230, 8 + row * 52, 225, 25,
                    window, reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(kEditBase + static_cast<int>(i))), nullptr, nullptr);
                SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                SendMessageW(gFields[i].edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            for (int i = 0; i < 7; ++i) {
                gToggles[i] = CreateWindowW(L"BUTTON", gToggleLabels[i],
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    965, 12 + i * 30, 240, 24, window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToggleBase + i)), nullptr, nullptr);
                Button_SetCheck(gToggles[i], BST_CHECKED);
                SendMessageW(gToggles[i], WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            const wchar_t* names[] = {L"Neues System", L"Fortsetzen", L"Schaden 10/15%",
                L"Snapshot speichern", L"Snapshot laden", L"State JSON",
                L"Evolutionslabor", L"Endziel bestaetigen",
                L"KI-Kopplung testen", L"Stufen 20-23"};
            const int ids[] = {
                kNew, kRun, kDamage, kSave, kLoad, kExport, kLab,
                kConfirmEndGoal, kPersistentAITrial, kStage20To23};
            for (int i = 0; i < 10; ++i) {
                HWND button = CreateWindowW(L"BUTTON", names[i], WS_CHILD | WS_VISIBLE,
                    965 + (i % 2) * 125, 205 + (i / 2) * 36, 118, 29, window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ids[i])), nullptr, nullptr);
                SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
            gOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                L"Parameter einstellen, 'Neues System' erzeugen und mit 'Fortsetzen' ohne Reset weiterrechnen.",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                12, 650, 1193, 145, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOutput)), nullptr, nullptr);
            SendMessageW(gOutput, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return 0;
        }
        if (message == WM_COMMAND) {
            const int id = LOWORD(wParam);
            if (id == kNew) {
                auto config = readConfig();
                gSystem = std::make_unique<agns::PersistentNervousSystem>(config);
                gEnvironment = std::make_unique<agns::ContinuousEnvironment>(config.seed + 1);
                const auto text = readText(gFields[18].edit);
                gEnvironment->injectTextUtf8(utf8(text));
                output(L"Neues persistentes Nervensystem erzeugt. Es wurde noch nicht fortgesetzt.");
            } else if (id == kRun) {
                if (!gSystem) SendMessageW(window, WM_COMMAND, kNew, 0);
                const auto result = agns::runClosedLoop(*gSystem, *gEnvironment, integer(gFields[17].edit));
                output(metricsText(result));
            } else if (id == kDamage && gSystem) {
                gSystem->applyDamage(0.10, 0.15, gSystem->metrics().step + 991);
                output(L"10 % Neuronen und 15 % Synapsen wurden deaktiviert. Mit 'Fortsetzen' Regeneration testen.");
            } else if (id == kSave && gSystem) {
                const auto path = chooseFile(window, true);
                if (!path.empty()) gSystem->saveSnapshot(path);
            } else if (id == kLoad) {
                const auto path = chooseFile(window, false);
                if (!path.empty()) {
                    auto config = readConfig();
                    gSystem = std::make_unique<agns::PersistentNervousSystem>(config);
                    gSystem->loadSnapshot(path);
                    gEnvironment = std::make_unique<agns::ContinuousEnvironment>(config.seed + 1);
                    output(L"Snapshot vollständig geladen. Die nächste Fortsetzung nutzt den gespeicherten neuronalen Zustand.");
                }
            } else if (id == kExport && gSystem) {
                auto path = chooseFile(window, true);
                if (!path.empty()) {
                    path.replace_extension(L".json");
                    gSystem->writeStateJson(path);
                }
            } else if (id == kLab) {
                const auto executable = std::filesystem::path(L"AGNervousSystemLab.exe");
                ShellExecuteW(window, L"open", executable.c_str(), L"nervous_system_lab_results",
                    nullptr, SW_SHOWNORMAL);
            } else if (id == kConfirmEndGoal) {
                const auto executable = std::filesystem::path(L"AGRepresentationResearch.exe");
                ShellExecuteW(
                    window,
                    L"open",
                    executable.c_str(),
                    L"stage18_ui_confirmation --confirm",
                    nullptr, SW_SHOWNORMAL);
                output(
                    L"Der eingefrorene Mehrseed-Endziellauf wurde gestartet. "
                    L"Er schreibt STAGE18_CONFIRMATION.md, JSON, CSV und "
                    L"Snapshots nach stage18_ui_confirmation.");
            } else if (id == kPersistentAITrial) {
                const auto executable =
                    std::filesystem::path(L"AGPersistentAITrial.exe");
                ShellExecuteW(
                    window,
                    L"open",
                    executable.c_str(),
                    L"stage19_ui_trial --confirm",
                    nullptr,
                    SW_SHOWNORMAL);
                output(
                    L"Der bestaetigte Persistent-AI-Kopplungslauf wurde "
                    L"gestartet. Er vergleicht die gekoppelte KI mit "
                    L"Ohne-Eligibility- und Ohne-Nervensystem-Kontrollen.");
            } else if (id == kStage20To23) {
                const auto executable =
                    std::filesystem::path(L"AGStage20To23.exe");
                ShellExecuteW(
                    window,
                    L"open",
                    executable.c_str(),
                    L"stage20_23_ui",
                    nullptr,
                    SW_SHOWNORMAL);
                output(
                    L"Die Gesamtpipeline fuer offene Lebenswelt, "
                    L"Mehrskalen-Gedaechtnis, Skalierung und "
                    L"Replikationspaket wurde gestartet.");
            }
            return 0;
        }
        if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception& error) {
        showError(window, error);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    const wchar_t* className = L"AGNervousSystemResearchLabClass";
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) return 1;
    HWND window = CreateWindowW(className, L"TATARUS – A Persistent Synthetic Nervous System",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1240, 850,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
