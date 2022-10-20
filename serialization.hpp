#pragma once

namespace Caching {

/// @brief  Serializer
/// @param val object supporting serialize method or trivial value to serialize
/// @return byte array representing passed value
constexpr auto serialize(auto val) // -> std::array<std::byte, SomeSize>
{
    if constexpr (requires { val.serialize(); })
    {
        return val.serialize();
    }
    else if constexpr (std::is_trivially_copyable_v<decltype(val)>)
    {
        return std::bit_cast<std::array<std::byte, sizeof(val)>>(val);
    }
    throw std::invalid_argument{"val is neither object of a class with method serealize nor of a trivial type"};
}

/// @brief  Deserealizer
/// @param bytes byte span of a size of S
/// @tparam T is type of obtained object
/// @tparam S size of provided span
/// @return object of type T
template<typename T, size_t S>
constexpr auto deserialize(std::span<std::byte, S> bytes) -> T
{
    if constexpr (requires { T::deserialize(bytes); })
    {
        return T::deserialize(bytes);
    }
    else if constexpr (std::is_trivial_v<T>)
    {
        T obj;
        std::memcpy(&obj, bytes.data(), bytes.size());
        return obj;
    }
    throw std::invalid_argument{"T is neither object of a class with method deserealize nor of a trivial type"};
}

}  // namespace Caching
