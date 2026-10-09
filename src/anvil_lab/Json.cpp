// Json.cpp — minimal strict JSON parser + deterministic writer.
#include "Json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace anvil_lab {

const JsonValue& JsonValue::at(const std::string& key) const {
    static const JsonValue nullValue;
    for (const auto& kv : obj)
        if (kv.first == key) return kv.second;
    return nullValue;
}

JsonValue& JsonValue::at(const std::string& key) {
    static JsonValue dummy;
    for (auto& kv : obj)
        if (kv.first == key) return kv.second;
    dummy = JsonValue();
    return dummy;
}

bool JsonValue::has(const std::string& key) const {
    for (const auto& kv : obj)
        if (kv.first == key) return true;
    return false;
}

void JsonValue::set(const std::string& key, JsonValue v) {
    for (auto& kv : obj) {
        if (kv.first == key) {
            kv.second = std::move(v);
            return;
        }
    }
    obj.emplace_back(key, std::move(v));
}

std::string JsonValue::asString(const std::string& dflt) const {
    return type == Type::String ? str : dflt;
}

double JsonValue::asNumber(double dflt) const {
    return type == Type::Number ? number : dflt;
}

int64_t JsonValue::asInt(int64_t dflt) const {
    return type == Type::Number ? static_cast<int64_t>(llround(number)) : dflt;
}

bool JsonValue::asBool(bool dflt) const {
    return type == Type::Bool ? boolean : dflt;
}

namespace {

struct Parser {
    const std::string& s;
    size_t p = 0;
    std::string err;

    explicit Parser(const std::string& text) : s(text) {}

    bool fail(const std::string& m) {
        if (err.empty())
            err = m + " at byte " + std::to_string(p);
        return false;
    }

    void ws() {
        while (p < s.size()
               && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r'))
            ++p;
    }

    bool literal(const char* lit) {
        const size_t n = std::char_traits<char>::length(lit);
        if (s.compare(p, n, lit) != 0) return false;
        p += n;
        return true;
    }

    bool parseValue(JsonValue& out) {
        ws();
        if (p >= s.size()) return fail("unexpected end of input");
        const char c = s[p];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') {
            out.type = JsonValue::Type::String;
            return parseString(out.str);
        }
        if (c == 't') {
            if (!literal("true")) return fail("bad literal");
            out = JsonValue::makeBool(true);
            return true;
        }
        if (c == 'f') {
            if (!literal("false")) return fail("bad literal");
            out = JsonValue::makeBool(false);
            return true;
        }
        if (c == 'n') {
            if (!literal("null")) return fail("bad literal");
            out = JsonValue::null();
            return true;
        }
        return parseNumber(out);
    }

    bool parseNumber(JsonValue& out) {
        const size_t start = p;
        if (p < s.size() && (s[p] == '-' || s[p] == '+')) ++p;
        bool digits = false, integral = true;
        while (p < s.size() && s[p] >= '0' && s[p] <= '9') { ++p; digits = true; }
        if (p < s.size() && s[p] == '.') {
            integral = false;
            ++p;
            while (p < s.size() && s[p] >= '0' && s[p] <= '9') { ++p; digits = true; }
        }
        if (!digits) return fail("invalid number");
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
            integral = false;
            ++p;
            if (p < s.size() && (s[p] == '-' || s[p] == '+')) ++p;
            bool expDigits = false;
            while (p < s.size() && s[p] >= '0' && s[p] <= '9') { ++p; expDigits = true; }
            if (!expDigits) return fail("invalid exponent");
        }
        const std::string tok = s.substr(start, p - start);
        errno = 0;
        char* end = nullptr;
        const double v = std::strtod(tok.c_str(), &end);
        if (errno == ERANGE || end != tok.c_str() + tok.size() || !std::isfinite(v))
            return fail("number out of range");
        out = JsonValue::makeNumber(v);
        if (integral) out = JsonValue::makeInt(static_cast<int64_t>(v));
        return true;
    }

    bool parseHex(unsigned& v) {
        if (p >= s.size()) return fail("truncated \\u escape");
        const char c = s[p];
        unsigned d = 0;
        if (c >= '0' && c <= '9') d = unsigned(c - '0');
        else if (c >= 'a' && c <= 'f') d = unsigned(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = unsigned(c - 'A' + 10);
        else return fail("invalid \\u escape");
        v = d;
        ++p;
        return true;
    }

    void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out.push_back(char(cp));
        else if (cp < 0x800) {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (cp >> 18)));
            out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }

    bool parseString(std::string& out) {
        out.clear();
        if (p >= s.size() || s[p] != '"') return fail("expected string");
        ++p;
        while (true) {
            if (p >= s.size()) return fail("unterminated string");
            const char c = s[p++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20)
                return fail("raw control character in string");
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (p >= s.size()) return fail("truncated escape");
            const char e = s[p++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        unsigned d = 0;
                        if (!parseHex(d)) return false;
                        cp = (cp << 4) | d;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF && p + 1 < s.size()
                        && s[p] == '\\' && s[p + 1] == 'u') {
                        p += 2;
                        unsigned lo = 0;
                        for (int i = 0; i < 4; ++i) {
                            unsigned d = 0;
                            if (!parseHex(d)) return false;
                            lo = (lo << 4) | d;
                        }
                        if (lo >= 0xDC00 && lo <= 0xDFFF)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        else
                            appendUtf8(out, cp), cp = lo;
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("invalid escape");
            }
        }
    }

    bool parseArray(JsonValue& out) {
        out = JsonValue::makeArray();
        ++p; // '['
        ws();
        if (p < s.size() && s[p] == ']') { ++p; return true; }
        while (true) {
            JsonValue v;
            if (!parseValue(v)) return false;
            out.arr.push_back(std::move(v));
            ws();
            if (p >= s.size()) return fail("unterminated array");
            if (s[p] == ',') { ++p; continue; }
            if (s[p] == ']') { ++p; return true; }
            return fail("expected ',' or ']'");
        }
    }

    bool parseObject(JsonValue& out) {
        out = JsonValue::makeObject();
        ++p; // '{'
        ws();
        if (p < s.size() && s[p] == '}') { ++p; return true; }
        while (true) {
            ws();
            std::string key;
            if (!parseString(key)) return false;
            ws();
            if (p >= s.size() || s[p] != ':') return fail("expected ':'");
            ++p;
            JsonValue v;
            if (!parseValue(v)) return false;
            out.obj.emplace_back(std::move(key), std::move(v));
            ws();
            if (p >= s.size()) return fail("unterminated object");
            if (s[p] == ',') { ++p; continue; }
            if (s[p] == '}') { ++p; return true; }
            return fail("expected ',' or '}'");
        }
    }
};

void dumpNumber(std::string& out, const JsonValue& v) {
    char buf[64];
    const double d = v.number;
    if (std::floor(d) == d && std::fabs(d) < 9.0e15) {
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(d));
    } else {
        std::snprintf(buf, sizeof buf, "%.10g", d);
    }
    out += buf;
}

} // namespace

std::string jsonStringEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

namespace {

void dumpValue(std::string& out, const JsonValue& v, int depth) {
    const std::string pad(depth * 2, ' ');
    const std::string padIn((depth + 1) * 2, ' ');
    switch (v.type) {
        case JsonValue::Type::Null: out += "null"; break;
        case JsonValue::Type::Bool: out += v.boolean ? "true" : "false"; break;
        case JsonValue::Type::Number: dumpNumber(out, v); break;
        case JsonValue::Type::String:
            out += '"';
            out += jsonStringEscape(v.str);
            out += '"';
            break;
        case JsonValue::Type::Array: {
            if (v.arr.empty()) {
                out += "[]";
                return;
            }
            out += "[\n";
            for (size_t i = 0; i < v.arr.size(); ++i) {
                out += padIn;
                dumpValue(out, v.arr[i], depth + 1);
                out += (i + 1 < v.arr.size()) ? ",\n" : "\n";
            }
            out += pad + "]";
            break;
        }
        case JsonValue::Type::Object: {
            if (v.obj.empty()) {
                out += "{}";
                return;
            }
            out += "{\n";
            for (size_t i = 0; i < v.obj.size(); ++i) {
                out += padIn + "\"" + jsonStringEscape(v.obj[i].first) + "\": ";
                dumpValue(out, v.obj[i].second, depth + 1);
                out += (i + 1 < v.obj.size()) ? ",\n" : "\n";
            }
            out += pad + "}";
            break;
        }
    }
}

} // namespace

std::string jsonDump(const JsonValue& v) {
    std::string out;
    dumpValue(out, v, 0);
    return out;
}

bool jsonParse(const std::string& text, JsonValue& out, std::string& err) {
    Parser p(text);
    if (!p.parseValue(out)) {
        err = p.err.empty() ? "parse error" : p.err;
        return false;
    }
    p.ws();
    if (p.p != text.size()) {
        err = "trailing content after JSON value at byte " + std::to_string(p.p);
        return false;
    }
    return true;
}

bool jsonReadFile(const std::string& path, JsonValue& out, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        err = "cannot open '" + path + "'";
        return false;
    }
    std::string text;
    char buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) {
        err = "read error on '" + path + "'";
        return false;
    }
    return jsonParse(text, out, err);
}

bool jsonWriteFile(const std::string& path, const JsonValue& v, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        err = "cannot open '" + path + "' for writing";
        return false;
    }
    const std::string text = jsonDump(v) + "\n";
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    if (!ok) err = "short write on '" + path + "'";
    return ok;
}

} // namespace anvil_lab
