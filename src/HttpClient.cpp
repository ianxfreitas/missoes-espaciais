#include "cronicas/HttpClient.hpp"
#include <curl/curl.h>
#include <memory>
#include <stdexcept>
namespace cronicas {
namespace {
struct CurlRuntime {
    CurlRuntime() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("Falha ao iniciar libcurl");
    }
    ~CurlRuntime() { curl_global_cleanup(); }
};
struct Buffer {
    std::string data;
};
size_t receive(char* data, size_t size, size_t count, void* context) noexcept {
    auto& buffer = *static_cast<Buffer*>(context);
    constexpr size_t limit = 16 * 1024 * 1024;
    if (size && count > limit / size) return 0;
    const auto bytes = size * count;
    if (bytes > limit - buffer.data.size()) return 0;
    try {
        buffer.data.append(data, bytes);
        return bytes;
    } catch (...) {
        return 0;
    }
}
}  // namespace
HttpResponse CurlHttpClient::get(const std::string& url,
                                 const std::string& token) const {
    static CurlRuntime runtime;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(),
                                                             curl_easy_cleanup);
    if (!curl) throw std::runtime_error("Falha ao criar cliente HTTP");
    const std::string auth = "Authorization: Bearer " + token;
    curl_slist* raw = curl_slist_append(nullptr, auth.c_str());
    if (!raw) throw std::runtime_error("Falha ao criar cabeçalho HTTP");
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
        raw, curl_slist_free_all);
    Buffer buffer;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &buffer);
    // Não segue redirecionamentos: evita encaminhar credenciais a outro host.
    auto result = curl_easy_perform(curl.get());
    if (result != CURLE_OK)
        throw std::runtime_error(std::string("Falha de transporte HTTP: ") +
                                 curl_easy_strerror(result));
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    return {status, std::move(buffer.data)};
}
}  // namespace cronicas
