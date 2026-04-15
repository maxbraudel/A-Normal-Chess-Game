#pragma once

#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace MultiplayerPasswordUtils {

inline std::string hex64(std::uint64_t value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

inline std::uint64_t fnv1a64(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char ch : value) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

inline std::string generateSalt() {
    std::random_device randomDevice;
    std::mt19937_64 engine(randomDevice());
    return hex64(engine()) + hex64(engine());
}

inline std::string computePasswordDigest(const std::string& password, const std::string& salt) {
    const std::uint64_t first = fnv1a64("lan-auth|" + salt + "|" + password);
    const std::uint64_t second = fnv1a64("lan-auth|" + password + "|" + salt);
    return hex64(first) + hex64(second);
}

} // namespace MultiplayerPasswordUtils