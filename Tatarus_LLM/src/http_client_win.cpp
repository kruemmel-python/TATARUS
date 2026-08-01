#include "tatarus_llm/http_client.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <stdexcept>
#include <vector>

namespace tatarus::llm {
namespace {
#ifdef _WIN32
std::wstring wide(const std::string& value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) throw std::runtime_error("Invalid UTF-8 string");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}
struct Handle {
    HINTERNET value = nullptr;
    ~Handle() { if (value) WinHttpCloseHandle(value); }
    Handle() = default; explicit Handle(HINTERNET v) : value(v) {}
    Handle(const Handle&) = delete; Handle& operator=(const Handle&) = delete;
};

class WinHttpClient final : public HttpClient {
public:
    explicit WinHttpClient(int timeoutMs) : timeoutMs_(timeoutMs) {}
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers) override { return request("GET", url, {}, headers); }
    HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers) override { return request("POST", url, body, headers); }
private:
    int timeoutMs_;
    HttpResponse request(const std::string& method, const std::string& url, const std::string& body,
                         const std::map<std::string, std::string>& headers) {
        const std::wstring wurl = wide(url);
        URL_COMPONENTS components{}; components.dwStructSize = sizeof(components);
        components.dwSchemeLength = static_cast<DWORD>(-1); components.dwHostNameLength = static_cast<DWORD>(-1);
        components.dwUrlPathLength = static_cast<DWORD>(-1); components.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &components)) throw std::runtime_error("Invalid HTTP URL: " + url);
        const std::wstring host(components.lpszHostName, components.dwHostNameLength);
        std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
        if (components.dwExtraInfoLength) path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
        Handle session(WinHttpOpen(L"TATARUS-LLM/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
        if (!session.value) throw std::runtime_error("WinHttpOpen failed");
        WinHttpSetTimeouts(session.value, timeoutMs_, timeoutMs_, timeoutMs_, timeoutMs_);
        Handle connection(WinHttpConnect(session.value, host.c_str(), components.nPort, 0));
        if (!connection.value) throw std::runtime_error("Cannot connect to " + url);
        const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        const std::wstring wmethod = wide(method);
        Handle req(WinHttpOpenRequest(connection.value, wmethod.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
        if (!req.value) throw std::runtime_error("WinHttpOpenRequest failed");
        std::wstring allHeaders;
        for (const auto& [key, value] : headers) allHeaders += wide(key + ": " + value + "\r\n");
        if (method == "POST" && !headers.contains("Content-Type")) allHeaders += L"Content-Type: application/json\r\n";
        const BOOL sent = WinHttpSendRequest(req.value, allHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : allHeaders.c_str(),
            allHeaders.empty() ? 0 : static_cast<DWORD>(-1), body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
        if (!sent || !WinHttpReceiveResponse(req.value, nullptr)) throw std::runtime_error("HTTP request failed: " + url);
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(req.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
        std::string response;
        while (true) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(req.value, &available)) throw std::runtime_error("HTTP read failed");
            if (available == 0) break;
            const std::size_t offset = response.size(); response.resize(offset + available);
            DWORD read = 0;
            if (!WinHttpReadData(req.value, response.data() + offset, available, &read)) throw std::runtime_error("HTTP read failed");
            response.resize(offset + read);
        }
        return {static_cast<int>(status), std::move(response)};
    }
};
#endif
}

std::shared_ptr<HttpClient> makeDefaultHttpClient(int timeoutMs) {
#ifdef _WIN32
    return std::make_shared<WinHttpClient>(timeoutMs);
#else
    (void)timeoutMs;
    throw std::runtime_error("The bundled HTTP client currently supports Windows/WinHTTP");
#endif
}

}  // namespace tatarus::llm
