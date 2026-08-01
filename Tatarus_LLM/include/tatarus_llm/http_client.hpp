#pragma once

#include <map>
#include <memory>
#include <string>

namespace tatarus::llm {

struct HttpResponse { int status = 0; std::string body; };

class HttpClient {
public:
    virtual ~HttpClient() = default;
    virtual HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) = 0;
    virtual HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {}) = 0;
};

std::shared_ptr<HttpClient> makeDefaultHttpClient(int timeoutMs = 120000);

}  // namespace tatarus::llm
