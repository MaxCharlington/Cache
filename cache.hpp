#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
// #include <iostream>
#include <fstream>
#include <mutex>
#include <shared_mutex>

namespace Caching {

/**
 * Dependancy class
 *
 * Implements convenient way to create key for Cache and ConcurrentCache classes
 * Internally uses std::tuple adding serialization capability
 */
template <typename... T>
class Dependances
{
public:
    constexpr Dependances(T... args) : vals{std::make_tuple(args...)} {}
    constexpr Dependances() = default;

    constexpr bool operator==(const Dependances& deps) const
    {
        return this->vals == deps.vals;
    }

    std::array<std::byte, sizeof(std::tuple<T...>)> serialize() const
    {
        std::array<std::byte, sizeof(std::tuple<T...>)> result;
        for (size_t i = 0; i < sizeof(std::tuple<T...>); i++) {
            result[i] = *(reinterpret_cast<const std::byte*>(&vals) + i);
        }
        return result;
    }

    const auto& get_vals() const { return vals; }

private:
    const std::tuple<T...> vals;
};

/// @brief  Serializer
/// @param val object supporting serialize method or trivial value to serialize
/// @return byte array representing passed value
constexpr auto serialize(auto val) -> std::array<std::byte, sizeof(val)>
{
    if constexpr (requires { val.serialize(); })
    {
        return val.serialize();
    }
    else
    {
        return std::bit_cast<std::array<std::byte, sizeof(val)>>(val);
    }
}

/**
 * Cache class
 *
 * Implements caching for values using std::unordered_map
 * Provides serialization and file dump capabilities
 *
 * @tparam Key type of a key for internal std::unordered_map
 * @tparam Value type of cached values
 */
template <typename Key, typename Value>
class Cache
{
public:
    Cache()
    {
        load_from_file();
    }

    ~Cache()
    {
        dump_to_file();
    }

    /// @brief Getter for file name for cache dump
    /// @return name of an associated file
    static const std::string& get_cache_file_name()
    {
        static std::string cache_file_name = []{
            using namespace std::string_literals;
            // Calculating of an associated file name based on types of key and value
            std::string res = "_cache";
            std::apply(
                [&](auto&&... args) {
                    ((res += ("_"s + typeid(decltype(args)).name())), ...);
                },
                Key{}.get_vals());
            res += "__";
            res += typeid(Value).name();
            res += ".bin";
            return res;
        }();
        return cache_file_name;
    }

    /// @brief Obtaines value by provided key if present
    /// @param key key for value
    /// @return std::optional for value
    [[nodiscard]] std::optional<Value> load(const Key& key) const
    {
        if (storage.find(key) != storage.end()) {
            return storage.at(key);
        }
        return std::nullopt;
    }

    /// @brief Saves value to the cache
    /// @tparam V type for value. Required for perfect forwarding
    /// @param deps key for storing value
    /// @param value value to store at key
    template <typename V>
    void store(const Key& deps, V&& value)
    {
        storage[deps] = std::forward<V>(value);
    }

protected:
    std::vector<std::byte> serialize() const
    {
        std::vector<std::byte> binary_data{storage.size() *
                                           (sizeof(Key) + sizeof(Value))};
        for (const auto& [key, value] : storage)
        {
            auto bin_key = Caching::serialize(key);
            std::ranges::for_each(
                bin_key, [&](auto byte) { binary_data.push_back(byte); });
            auto bin_value = Caching::serialize(value);
            std::ranges::for_each(
                bin_value, [&](auto byte) { binary_data.push_back(byte); });
        }
        return binary_data;
    }

    /// @brief Restores data from an associated file
    void load_from_file()
    {
        // Unimplemented
    }

    /// @brief Dumps cache content to an associated file
    void dump_to_file()
    {
        std::ofstream ofp(get_cache_file_name(), std::ios::out | std::ios::binary);
        const auto data = this->serialize();
        ofp.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::unordered_map<Key, Value> storage;
};

/**
 * ConcurrentCache class
 *
 * Child for Cache decorating methods enabling concurrent usage
 * Implements same methods as Cache class
 *
 * @tparam Key type of a key for internal std::unordered_map
 * @tparam Value type of cached values
 */
template <typename Key, typename Value>
class ConcurrentCache : public Cache<Key, Value>
{
public:
    /// @brief Obtaines value by provided key if present concurrently
    /// @param key key for value
    /// @return std::optional for value
    [[nodiscard]] std::optional<Value> load(const Key& key) const
    {
        std::shared_lock lk{mtx};
        return Cache<Key, Value>::load(key);
    }

    /// @brief Saves value to the cache cuncurrently
    /// @tparam V type for value. Required for perfect forwarding
    /// @param deps key for storing value
    /// @param value value to store at key
    template <typename V>
    void store(const Key& deps, V&& value)
    {
        std::unique_lock lk{mtx};
        Cache<Key, Value>::store(deps, std::forward<V>(value));
    }

private:
    std::vector<std::byte> serialize() const
    {
        std::shared_lock lk{mtx};
        return Cache<Key, Value>::serialize();
    }

    mutable std::shared_mutex mtx;
};

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
