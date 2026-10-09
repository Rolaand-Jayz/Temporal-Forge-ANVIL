// Json.hpp — minimal JSON value model, parser, and deterministic writer.
//
// ANVIL's JsonWriter is serialization-only (the runner never reads JSON).
// The Lab must PARSE run manifests, experiment definitions, the baseline
// identity, the candidate catalog, and roster files, so this header adds a
// strict recursive-descent parser. Objects preserve insertion order so
// regenerated catalog files are byte-stable.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace anvil_lab {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    static JsonValue null() { return {}; }
    static JsonValue makeBool(bool v) { JsonValue j; j.type = Type::Bool; j.boolean = v; return j; }
    static JsonValue makeNumber(double v) { JsonValue j; j.type = Type::Number; j.number = v; return j; }
    static JsonValue makeInt(int64_t v) { JsonValue j; j.type = Type::Number; j.number = static_cast<double>(v); j.integral = true; return j; }
    static JsonValue makeString(std::string v) { JsonValue j; j.type = Type::String; j.str = std::move(v); return j; }
    static JsonValue makeArray() { JsonValue j; j.type = Type::Array; return j; }
    static JsonValue makeObject() { JsonValue j; j.type = Type::Object; return j; }

    bool isNull() const { return type == Type::Null; }
    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }
    bool isBool() const { return type == Type::Bool; }

    // Object access: returns Null when the key is absent.
    const JsonValue& at(const std::string& key) const;
    bool has(const std::string& key) const;
    void set(const std::string& key, JsonValue v); // replaces or appends

    std::string asString(const std::string& dflt = "") const;
    double asNumber(double dflt = 0.0) const;
    int64_t asInt(int64_t dflt = 0) const;
    bool asBool(bool dflt = false) const;

private:
    // Printed without a decimal point when the value is integral; keeps
    // regenerated files byte-stable.
    bool integral = false;
};

// Strict parse: the whole input must be exactly one JSON value (optional
// surrounding whitespace). Returns false and fills err otherwise.
bool jsonParse(const std::string& text, JsonValue& out, std::string& err);

// Compact deterministic serialization (2-space indent objects/arrays).
std::string jsonDump(const JsonValue& v);

// Escapes a raw string as a JSON string literal.
std::string jsonStringEscape(const std::string& s);

// Reads and parses a file (fails closed with err on I/O or parse error).
bool jsonReadFile(const std::string& path, JsonValue& out, std::string& err);
bool jsonWriteFile(const std::string& path, const JsonValue& v, std::string& err);

} // namespace anvil_lab
