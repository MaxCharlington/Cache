#pragma once

#include <cstdint>
#include <algorithm>

#include "serialization.hpp"

namespace Caching {

/**
 * Literal class type that wraps a constant expression string
 *
 * Uses implicit conversion to allow templates to accept constant strings
 */
template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }

    char value[N];
};

// Forward Declaration for std::hash
template<typename ...T>
class Dependances;

}  // namespace Caching

/// @brief Hash struct overload for Dependancies
template <typename... T>
struct std::hash<Caching::Dependances<T...>>
{
    constexpr std::size_t operator()(
        const Caching::Dependances<T...>& deps) const noexcept {
        size_t result = std::numeric_limits<size_t>::max();
        std::apply(
            [&result](auto&&... args) {
                ((result ^= std::hash<std::string>{}(std::to_string(args))),
                 ...);
            },
            deps.get_vals());
        return result;
    }
};
