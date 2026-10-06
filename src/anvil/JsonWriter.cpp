// JsonWriter.cpp — minimal deterministic JSON writer.
#include "JsonWriter.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace anvil {

JsonWriter::JsonWriter() {
    stack_.push_back({Ctx::TopLevel, false});
}

void JsonWriter::comma() {
    auto& top = stack_.back();
    if (top.needsComma) out_ += ',';
    top.needsComma = true;
}

void JsonWriter::beginObject() {
    comma();
    out_ += '{';
    stack_.push_back({Ctx::Object, false});
}

void JsonWriter::endObject() {
    out_ += '}';
    stack_.pop_back();
}

void JsonWriter::beginArray() {
    comma();
    out_ += '[';
    stack_.push_back({Ctx::Array, false});
}

void JsonWriter::endArray() {
    out_ += ']';
    stack_.pop_back();
}

void JsonWriter::key(std::string_view k) {
    comma();
    writeEscaped(k);
    out_ += ':';
    // A key consumes the object's pending comma; the value still needs one.
    stack_.back().needsComma = false;
}

void JsonWriter::writeEscaped(std::string_view v) {
    out_ += '"';
    for (char c : v) {
        switch (c) {
            case '"': out_ += "\\\""; break;
            case '\\': out_ += "\\\\"; break;
            case '\n': out_ += "\\n"; break;
            case '\r': out_ += "\\r"; break;
            case '\t': out_ += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out_ += buf;
                } else {
                    out_ += c;
                }
        }
    }
    out_ += '"';
}

void JsonWriter::value(std::string_view v) {
    comma();
    writeEscaped(v);
}

void JsonWriter::value(int64_t v) {
    comma();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    out_ += buf;
}

void JsonWriter::value(uint64_t v) {
    comma();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(v));
    out_ += buf;
}

void JsonWriter::value(bool v) {
    comma();
    out_ += v ? "true" : "false";
}

void JsonWriter::value(double v) {
    comma();
    if (std::isfinite(v)) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%.9g", v);
        out_ += buf;
    } else {
        null(); // JSON has no NaN/Inf; explicit null beats fabricated numbers
    }
}

void JsonWriter::null() {
    comma();
    out_ += "null";
}

std::string JsonWriter::str() const {
    return out_;
}

} // namespace anvil
