#pragma once
#include <string>
namespace cronicas {
struct HttpResponse {
    long status;
    std::string body;
};
class HttpClient {
   public:
    virtual ~HttpClient() = default;
    virtual HttpResponse get(const std::string& url,
                             const std::string& token) const = 0;
};
class CurlHttpClient final : public HttpClient {
   public:
    HttpResponse get(const std::string& url,
                     const std::string& token) const override;
};
}  // namespace cronicas
