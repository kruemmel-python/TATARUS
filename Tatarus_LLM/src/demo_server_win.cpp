#include "tatarus_llm/demo_server.hpp"

#include "tatarus_llm/cognitive_json.hpp"
#include "tatarus_llm/mini_json.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tatarus::llm {
namespace {
#ifdef _WIN32
constexpr std::size_t kMaximumRequestBytes = 65536;
constexpr std::size_t kMaximumAssetBytes = 2 * 1024 * 1024;

struct WinsockGuard {
    WinsockGuard() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
    }
    ~WinsockGuard() { WSACleanup(); }
};

struct SocketGuard {
    SOCKET value = INVALID_SOCKET;
    ~SocketGuard() { if (value != INVALID_SOCKET) closesocket(value); }
};

void sendAll(SOCKET socket, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int count = send(socket, data.data() + sent,
            static_cast<int>(std::min<std::size_t>(data.size() - sent, 0x7fffffff)), 0);
        if (count <= 0) throw std::runtime_error("HTTP response send failed");
        sent += static_cast<std::size_t>(count);
    }
}

std::string statusPhrase(int status) {
    if (status == 200) return "OK";
    if (status == 204) return "No Content";
    if (status == 400) return "Bad Request";
    if (status == 404) return "Not Found";
    if (status == 405) return "Method Not Allowed";
    if (status == 500) return "Internal Server Error";
    return "Error";
}

void responseText(SOCKET client, int status, std::string_view contentType,
                  const std::string& payload, bool cacheAsset = false) {
    std::ostringstream headers;
    headers << "HTTP/1.1 " << status << ' ' << statusPhrase(status)
            << "\r\nContent-Type: " << contentType
            << "\r\nContent-Length: " << payload.size()
            << "\r\nConnection: close"
            << "\r\nX-Content-Type-Options: nosniff"
            << "\r\nReferrer-Policy: no-referrer"
            << "\r\nCross-Origin-Resource-Policy: same-origin"
            << "\r\nContent-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'"
            << "\r\nCache-Control: " << (cacheAsset ? "no-cache" : "no-store")
            << "\r\n\r\n";
    sendAll(client, headers.str() + payload);
}

void responseJson(SOCKET client, int status, const json::Value& body) {
    responseText(client, status, "application/json; charset=utf-8", body.dump());
}

void responseEmpty(SOCKET client) {
    sendAll(client, "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n"
                    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type\r\n\r\n");
}

std::size_t contentLength(const std::string& headers) {
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto at = lower.find("content-length:");
    if (at == std::string::npos) return 0;
    const auto start = lower.find_first_of("0123456789", at);
    return start == std::string::npos ? 0 : std::stoull(lower.substr(start));
}

std::string loadAsset(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
        throw std::runtime_error("Web asset not found: " + path.string());
    if (std::filesystem::file_size(path) > kMaximumAssetBytes)
        throw std::runtime_error("Web asset exceeds size limit: " + path.string());
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read web asset: " + path.string());
    return std::string((std::istreambuf_iterator<char>(input)), {});
}

bool serveAsset(SOCKET client, const std::filesystem::path& root, const std::string& path) {
    if (root.empty()) return false;
    std::filesystem::path file;
    std::string_view contentType;
    if (path == "/" || path == "/index.html") {
        file = root / "index.html"; contentType = "text/html; charset=utf-8";
    } else if (path == "/app.js") {
        file = root / "app.js"; contentType = "text/javascript; charset=utf-8";
    } else if (path == "/styles.css") {
        file = root / "styles.css"; contentType = "text/css; charset=utf-8";
    } else if (path == "/favicon.svg") {
        file = root / "favicon.svg"; contentType = "image/svg+xml";
    } else {
        return false;
    }
    responseText(client, 200, contentType, loadAsset(file), true);
    return true;
}

json::Value memoryStatusJson(const TatarusPlannerHost& host) {
    return json::Value::Object{
        {"memory_owner", toString(host.memoryMode())},
        {"episodes", std::to_string(host.episodicMemorySize())},
        {"synapses", std::to_string(host.episodicSynapseCount())},
        {"last_reconstruction_spikes", std::to_string(host.episodicRecallSpikeCount())},
        {"last_reconstruction_failures", std::to_string(host.episodicRecallFailureCount())},
        {"plasticity_updates", std::to_string(host.episodicPlasticityUpdateCount())},
        {"state_hash", std::to_string(host.stateHash())}
    };
}

json::Value resultJson(const HostStepResult& result) {
    const auto& command = result.planner.command;
    return json::Value::Object{{"provider", result.planner.provider}, {"model", result.planner.model},
        {"planning_latency_ms", static_cast<double>(result.planner.latencyMs)},
        {"response", result.response.text}, {"language_model", result.response.model},
        {"language_latency_ms", static_cast<double>(result.response.latencyMs)},
        {"language_error", result.response.error},
        {"recalled_episodes", recalledEpisodesJson(result.recalledEpisodes)},
        {"episodic_reconstruction_spikes", std::to_string(result.reconstructionSpikes)},
        {"episodic_reconstruction_failures", std::to_string(result.reconstructionFailures)},
        {"episodic_plasticity_updates", std::to_string(result.plasticityUpdates)},
        {"episodic_memory_synapses", std::to_string(result.memorySynapses)},
        {"command", json::Value::Object{{"attention", command.attention}, {"motor_intent", command.motorIntent},
            {"intent_strength", command.intentStrength}, {"recall_cue", static_cast<double>(command.recallCue)},
            {"recall_strength", command.recallStrength}}},
        {"environment_reward", result.feedback.reward}, {"cognitive_state", cognitiveStateJson(result.after)}};
}
#endif
}

void runDemoServer(TatarusPlannerHost& host, unsigned short port,
                   const std::filesystem::path& snapshotDirectory, bool autosave,
                   const std::filesystem::path& webRoot) {
#ifdef _WIN32
    WinsockGuard winsock;
    SocketGuard server{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (server.value == INVALID_SOCKET) throw std::runtime_error("Cannot create web server socket");
    BOOL reuse = TRUE;
    setsockopt(server.value, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (bind(server.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(server.value, 8) == SOCKET_ERROR)
        throw std::runtime_error("Cannot bind TATARUS Web UI to 127.0.0.1:" + std::to_string(port));
    std::cout << "TATARUS Web UI: http://127.0.0.1:" << port << " (Ctrl+C beendet)\n";
    if (webRoot.empty()) std::cout << "Hinweis: kein Web-Asset-Verzeichnis gefunden; nur die JSON-API ist aktiv.\n";

    while (true) {
        SocketGuard client{accept(server.value, nullptr, nullptr)};
        if (client.value == INVALID_SOCKET) continue;
        try {
            std::string request;
            char buffer[4096];
            std::size_t headerEnd = std::string::npos;
            while (request.size() < kMaximumRequestBytes &&
                   (headerEnd = request.find("\r\n\r\n")) == std::string::npos) {
                const int count = recv(client.value, buffer, sizeof(buffer), 0);
                if (count <= 0) break;
                request.append(buffer, count);
            }
            if (headerEnd == std::string::npos) {
                responseJson(client.value, 400, json::Value::Object{{"error", "invalid HTTP request"}});
                continue;
            }
            const std::size_t length = contentLength(request.substr(0, headerEnd));
            if (length > kMaximumRequestBytes) {
                responseJson(client.value, 400, json::Value::Object{{"error", "request body too large"}});
                continue;
            }
            while (request.size() - (headerEnd + 4) < length) {
                const int count = recv(client.value, buffer, sizeof(buffer), 0);
                if (count <= 0) break;
                request.append(buffer, count);
            }
            const auto firstSpace = request.find(' ');
            const auto secondSpace = request.find(' ', firstSpace + 1);
            if (firstSpace == std::string::npos || secondSpace == std::string::npos)
                throw std::runtime_error("Invalid HTTP request line");
            const std::string method = request.substr(0, firstSpace);
            std::string path = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            if (const auto query = path.find('?'); query != std::string::npos) path.resize(query);

            if (method == "OPTIONS") {
                responseEmpty(client.value);
            } else if (method == "GET" && serveAsset(client.value, webRoot, path)) {
                // Static response already sent.
            } else if (method == "GET" && path == "/health") {
                responseJson(client.value, 200, json::Value::Object{
                    {"status", "ok"}, {"provider", host.provider().providerName()},
                    {"model", host.provider().currentModel()}, {"memory_owner", toString(host.memoryMode())},
                    {"web_ui", !webRoot.empty()}});
            } else if (method == "GET" && path == "/v1/state") {
                responseJson(client.value, 200, cognitiveStateJson(host.state()));
            } else if (method == "GET" && path == "/v1/memory") {
                responseJson(client.value, 200, memoryStatusJson(host));
            } else if (method == "POST" && path == "/v1/step") {
                const auto body = json::Value::parse(request.substr(headerEnd + 4, length));
                if (!body.isObject() || body.object().size() != 1 || !body.contains("user_input"))
                    throw std::runtime_error("Body must contain exactly user_input");
                const auto& input = json::requiredString(body, "user_input");
                if (input.empty() || input.size() > kMaximumRequestBytes)
                    throw std::runtime_error("user_input must contain 1 to 65536 UTF-8 bytes");
                const auto result = host.step(input);
                if (autosave) host.save(snapshotDirectory);
                responseJson(client.value, 200, resultJson(result));
            } else if (method == "POST" && path == "/v1/save") {
                host.save(snapshotDirectory);
                responseJson(client.value, 200, json::Value::Object{{"status", "saved"}, {"path", snapshotDirectory.string()}});
            } else if (method == "POST" && path == "/v1/load") {
                host.load(snapshotDirectory);
                responseJson(client.value, 200, json::Value::Object{{"status", "loaded"}, {"path", snapshotDirectory.string()}});
            } else if (method != "GET" && method != "POST") {
                responseJson(client.value, 405, json::Value::Object{{"error", "method not allowed"}});
            } else {
                responseJson(client.value, 404, json::Value::Object{{"error", "unknown endpoint"}});
            }
        } catch (const std::exception& error) {
            try {
                responseJson(client.value, 500, json::Value::Object{{"error", error.what()}});
            } catch (...) {
                // A disconnected browser must not terminate the persistent host.
            }
        }
    }
#else
    (void)host; (void)port; (void)snapshotDirectory; (void)autosave; (void)webRoot;
    throw std::runtime_error("TATARUS Web UI currently requires Windows/Winsock");
#endif
}

}  // namespace tatarus::llm
