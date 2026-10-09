// Sha256.hpp — compact SHA-256 for input-file provenance (public-domain
// algorithm, FIPS 180-4). No external dependency.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

namespace anvil {

// Returns lowercase hex digest of the buffer.
std::string sha256Hex(const uint8_t* data, size_t len);

// Convenience: hash a file; returns false if unreadable.
bool sha256FileHex(const std::string& path, std::string& outHex);

} // namespace anvil
