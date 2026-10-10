// Http.cpp — minimal local HTTP/1.1 server implementation.
#include "Http.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <atomic>
#include <csignal>
#include <poll.h>
#include <thread>

namespace anvil_lab {

namespace {
std::atomic<bool> gShutdown{false};
volatile std::sig_atomic_t gSignalStop = 0;
void signalStop(int) { gSignalStop = 1; }

constexpr size_t kMaxHeaderBytes = 32 * 1024;
constexpr size_t kMaxBodyBytes = 64 * 1024 * 1024; // annotated screenshots

bool hexVal(char c, unsigned& v) {
    if (c >= '0' && c <= '9') { v = unsigned(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { v = unsigned(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { v = unsigned(c - 'A' + 10); return true; }
    return false;
}

bool recvSome(int fd, std::vector<uint8_t>& buf) {
    uint8_t tmp[16384];
    const ssize_t n = ::recv(fd, tmp, sizeof tmp, 0);
    if (n <= 0) return false;
    buf.insert(buf.end(), tmp, tmp + n);
    return true;
}

bool sendAll(int fd, const uint8_t* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        const ssize_t n = ::send(fd, data + off, len - off, MSG_NOSIGNAL);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
}

const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default: return "Status";
    }
}

} // namespace

bool urlDecode(const std::string& in, std::string& out) {
    out.clear();
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '+') { out.push_back(' '); continue; }
        if (in[i] == '%' && i + 2 < in.size()) {
            unsigned hi = 0, lo = 0;
            if (hexVal(in[i + 1], hi) && hexVal(in[i + 2], lo)) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
            return false;
        }
        out.push_back(in[i]);
    }
    return true;
}

std::string urlEncode(const std::string& in) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : in) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out.push_back(static_cast<char>(c));
        else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

namespace {

// Parses one request from the buffer; returns 1 parsed, 0 need-more,
// -1 malformed. Consumes the parsed bytes.
int parseRequest(std::vector<uint8_t>& buf, HttpRequest& req) {
    const size_t headEnd = std::search(buf.begin(), buf.end(),
                                       reinterpret_cast<const uint8_t*>("\r\n\r\n"),
                                       reinterpret_cast<const uint8_t*>("\r\n\r\n") + 4)
        - buf.begin();
    const bool haveHead = headEnd + 4 <= buf.size();
    if (!haveHead) {
        if (buf.size() > kMaxHeaderBytes) return -1;
        return 0;
    }
    if (headEnd > kMaxHeaderBytes) return -1;
    std::string head(buf.begin(), buf.begin() + static_cast<long>(headEnd));
    size_t lineEnd = head.find("\r\n");
    const std::string reqLine = head.substr(0, lineEnd == std::string::npos
                                                    ? head.size() : lineEnd);
    size_t sp1 = reqLine.find(' ');
    size_t sp2 = reqLine.rfind(' ');
    if (sp1 == std::string::npos || sp2 == sp1) return -1;
    req.method = reqLine.substr(0, sp1);
    const std::string target = reqLine.substr(sp1 + 1, sp2 - sp1 - 1);
    const std::string version = reqLine.substr(sp2 + 1);
    if (version.rfind("HTTP/1.", 0) != 0) return -1;
    if (req.method != "GET" && req.method != "POST") return -1;

    // Query string split + strict decode.
    const size_t q = target.find('?');
    std::string rawPath = q == std::string::npos ? target : target.substr(0, q);
    if (!urlDecode(rawPath, req.path)) return -1;
    if (req.path.empty() || req.path[0] != '/') return -1;
    if (q != std::string::npos) {
        const std::string qs = target.substr(q + 1);
        size_t pos = 0;
        while (pos <= qs.size()) {
            const size_t amp = qs.find('&', pos);
            const std::string pair = qs.substr(
                pos, amp == std::string::npos ? std::string::npos : amp - pos);
            const size_t eq = pair.find('=');
            const std::string kRaw = pair.substr(0, eq);
            const std::string vRaw = eq == std::string::npos
                ? std::string() : pair.substr(eq + 1);
            std::string k, v;
            if (!urlDecode(kRaw, k) || !urlDecode(vRaw, v)) return -1;
            if (!k.empty()) req.query[k] = v;
            if (amp == std::string::npos) break;
            pos = amp + 1;
        }
    }

    // Content-Length body (the only body form accepted).
    size_t contentLength = 0;
    bool sawLength = false;
    size_t rest = lineEnd == std::string::npos ? head.size() : lineEnd + 2;
    while (rest < head.size()) {
        size_t eol = head.find("\r\n", rest);
        const std::string lineL = head.substr(
            rest, eol == std::string::npos ? std::string::npos : eol - rest);
        const size_t colon = lineL.find(':');
        if (colon != std::string::npos) {
            const std::string name = lineL.substr(0, colon);
            std::string val = lineL.substr(colon + 1);
            while (!val.empty() && val.front() == ' ') val.erase(0, 1);
            std::string lower = name;
            for (char& ch : lower)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (lower == "content-length") {
                if (sawLength || val.empty() || val.find_first_not_of("0123456789")
                        != std::string::npos) return -1;
                sawLength = true;
                size_t parsed = 0;
                for (char digit : val) {
                    const size_t d = static_cast<size_t>(digit - '0');
                    if (parsed > (kMaxBodyBytes - d) / 10) return -2;
                    parsed = parsed * 10 + d;
                }
                contentLength = parsed;
            } else if (lower == "transfer-encoding") {
                // No chunked support, especially no CL/TE ambiguity.
                return -1;
            }
        }
        if (eol == std::string::npos) break;
        rest = eol + 2;
    }
    const size_t total = headEnd + 4 + contentLength;
    if (buf.size() < total) return 0;
    req.body.assign(buf.begin() + static_cast<long>(headEnd + 4),
                    buf.begin() + static_cast<long>(total));
    buf.erase(buf.begin(), buf.begin() + static_cast<long>(total));
    return 1;
}

void handleConnection(int fd, const HttpHandler& handler) {
    std::vector<uint8_t> buf;
    // Serve up to a few requests per connection (simple keep-alive), then
    // close when the peer stops sending.
    for (int served = 0; served < 64; ++served) {
        HttpRequest req;
        int st = 0;
        try {
            while ((st = parseRequest(buf, req)) == 0) {
                if (!recvSome(fd, buf)) { ::close(fd); return; }
            }
        } catch (const std::exception&) { st = -1; }
        if (st < 0) {
            const std::string msg = st == -2
                ? "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
                : "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            sendAll(fd, reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
            break;
        }
        HttpResponse res;
        try {
            res = handler(req);
        } catch (const std::exception& e) {
            res.status = 500;
            res.contentType = "application/json";
            const std::string body = std::string("{\"error\":\"") + e.what() + "\"}";
            res.body.assign(body.begin(), body.end());
        }
        std::string head = "HTTP/1.1 " + std::to_string(res.status) + " "
            + statusText(res.status) + "\r\nContent-Type: " + res.contentType
            + "\r\nContent-Length: " + std::to_string(res.body.size());
        for (const std::string& h : res.headers) head += "\r\n" + h;
        head += "\r\nConnection: keep-alive\r\n\r\n";
        if (!sendAll(fd, reinterpret_cast<const uint8_t*>(head.data()), head.size())
            || !sendAll(fd, res.body.data(), res.body.size()))
            break;
    }
    ::close(fd);
}

} // namespace

bool httpServe(const std::string& bindAddr, uint16_t port,
               const HttpHandler& handler, const std::atomic<bool>*& shutdownFlag,
               std::string& err) {
    gShutdown.store(false);
    gSignalStop = 0;
    std::signal(SIGINT, signalStop);
    std::signal(SIGTERM, signalStop);
    shutdownFlag = &gShutdown;
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        err = "socket() failed";
        return false;
    }
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (bindAddr.empty() || bindAddr == "localhost")
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else if (::inet_pton(AF_INET, bindAddr.c_str(), &addr.sin_addr) != 1) {
        err = "invalid bind address '" + bindAddr + "' (IPv4 literal expected)";
        ::close(fd);
        return false;
    }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        err = "bind failed on " + bindAddr + ":" + std::to_string(port)
            + " (port already in use?)";
        ::close(fd);
        return false;
    }
    if (::listen(fd, 16) < 0) {
        err = "listen failed";
        ::close(fd);
        return false;
    }
    std::vector<std::thread> workers;
    while (!gShutdown.load() && !gSignalStop) {
        pollfd pfd{fd, POLLIN, 0};
        const int ready = ::poll(&pfd, 1, 200);
        if (ready <= 0 || !(pfd.revents & POLLIN)) continue;
        const int conn = ::accept(fd, nullptr, nullptr);
        if (conn < 0) continue;
        timeval timeout{1, 0};
        ::setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        ::setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
        workers.emplace_back(handleConnection, conn, handler);
    }
    ::close(fd);
    for (auto& worker : workers) if (worker.joinable()) worker.join();
    return true;
}

void requestServerShutdown() { gShutdown.store(true); }

} // namespace anvil_lab
