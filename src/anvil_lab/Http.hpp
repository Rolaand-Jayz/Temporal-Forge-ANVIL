// Http.hpp — minimal local HTTP/1.1 server for the ANVIL Visual Review Lab.
//
// Scope is deliberately narrow: a localhost single-user research tool, not a
// general web server. IPv4 TCP, GET/POST with bounded bodies, explicit
// Content-Length on every response, and a hard path-sandbox (no encoded
// traversal, no absolute escape). No TLS, no keep-alive requirement, no
// chunked upload parsing — anything outside that contract returns 400/501.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace anvil_lab {

struct HttpRequest {
    std::string method;
    std::string path;                 // decoded, must start with '/'
    std::map<std::string, std::string> query; // decoded query parameters
    std::vector<uint8_t> body;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::vector<uint8_t> body;
    // Optional extra headers (e.g. Cache-Control) as "Name: value" lines.
    std::vector<std::string> headers;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

// Blocks serving until shutdown is requested. Returns false when
// bind/listen fails (err filled).
bool httpServe(const std::string& bindAddr, uint16_t port,
               const HttpHandler& handler, const std::atomic<bool>*& shutdownFlag,
               std::string& err);

// Asks a running httpServe loop to stop (safe from any thread/handler).
void requestServerShutdown();

// URL-decodes %XX escapes and '+' as space. Invalid escapes make the whole
// request get rejected (handled by the parser, not silently decoded).
bool urlDecode(const std::string& in, std::string& out);

// Percent-encodes a path component for safe Location/Content-Disposition.
std::string urlEncode(const std::string& in);

} // namespace anvil_lab
