// JsonWriter.hpp — minimal deterministic JSON writer (no external deps).
// Output is deterministic: insertion-ordered, fixed float formatting.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace anvil {

class JsonWriter {
public:
    JsonWriter();

    void beginObject();
    void endObject();
    void beginArray();
    void endArray();

    void key(std::string_view k);
    void value(std::string_view v);   // string value
    void value(int64_t v);
    void value(uint64_t v);
    void value(int v) { value(static_cast<int64_t>(v)); }
    void value(bool v);
    void value(double v);
    void null();

    // key + scalar helpers
    void kv(std::string_view k, std::string_view v) { key(k); value(v); }
    void kv(std::string_view k, const std::string& v) { key(k); value(v); }
    void kv(std::string_view k, int64_t v) { key(k); value(v); }
    void kv(std::string_view k, int v) { key(k); value(static_cast<int64_t>(v)); }
    void kv(std::string_view k, uint64_t v) { key(k); value(v); }
    void kv(std::string_view k, bool v) { key(k); value(v); }
    void kv(std::string_view k, double v) { key(k); value(v); }

    // begin an object/array as the value of key k
    void object(std::string_view k) { key(k); beginObject(); }
    void array(std::string_view k) { key(k); beginArray(); }

    std::string str() const;

private:
    enum class Ctx { TopLevel, Object, Array };
    struct Scope {
        Ctx ctx;
        bool needsComma = false;
    };
    void comma();
    void writeEscaped(std::string_view v);

    std::string out_;
    std::vector<Scope> stack_;
};

} // namespace anvil
