#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "serialization.hpp"
#include "helpers.hpp"

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
    using Values = std::tuple<T...>;
    static constexpr size_t BinSize = []{  // size of byte array representing all values in internal tuple
        Values v;
        size_t size = 0;
        std::apply([&](auto&&... args) {((size += sizeof(args)), ...);}, v);
        return size;
    }();

    constexpr Dependances(T... args) : vals{std::make_tuple(args...)} {}
    constexpr Dependances() = default;

    constexpr bool operator==(const Dependances& deps) const
    {
        return this->vals == deps.vals;
    }

    const auto& get_vals() const { return vals; }

    std::array<std::byte, BinSize> serialize() const
    {
        std::array<std::byte, BinSize> bytes;
        auto to_byte_arr = [&, offset = 0]<typename Type>(Type&& val) mutable {
            new (bytes.data() + offset) std::remove_cvref_t<Type>(std::forward<Type>(val));
            offset += sizeof(Type);
        };
        std::apply([&](auto&&... args) {((to_byte_arr(std::forward<decltype(args)>(args))), ...);}, vals);
        return bytes;
    }

    static Dependances deserialize(std::span<std::byte, BinSize> bytes) {
        Dependances deps;
        auto from_byte_arr = [&, offset = 0](auto& val) mutable {
            std::memcpy(&val, bytes.data() + offset, sizeof(val));
            offset += sizeof(val);
        };
        std::apply([&](auto&&... args) {((from_byte_arr(args)), ...);}, deps.vals);
        return deps;
    }

private:
    Values vals;
};

/**
 * Cache class
 *
 * Implements caching for values using std::unordered_map
 * Provides serialization and file dump capabilities
 *
 * @tparam Key type of a key for internal std::unordered_map
 * @tparam Value type of cached values
 * @tparam Tag file tag string for identificaion
 */
template <typename Key, typename Value, StringLiteral Tag = "">
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
            if constexpr (std::string_view{Tag.value} != "")
            {
                res += "_" + std::string{Tag.value};
            }
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
        std::vector<std::byte> binary_data{};

        binary_data.reserve(storage.size() * (sizeof(Key) + sizeof(Value)));

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

    std::unordered_map<Key, Value> deserialize(std::span<std::byte> bytes, size_t cached_count) {
        std::unordered_map<Key, Value> map;
        map.reserve(cached_count);
        std::byte* ptr = bytes.data();
        for (size_t i = 0; i <= cached_count; i++, ptr += bytes.size() / cached_count)
        {
            auto key = Caching::deserialize<Key>(std::span<std::byte, Key::BinSize>{ptr, Key::BinSize});
            auto value = Caching::deserialize<Value>(std::span<std::byte, sizeof(Value)>{ptr + Key::BinSize, sizeof(Value)});

            map[key] = value;
        }
        return map;
    }

    /// @brief Restores data from an associated file
    void load_from_file()
    {
        static constexpr size_t key_val_size = Key::BinSize + sizeof(Value);

        std::ifstream file_dump{get_cache_file_name(), std::ios::binary | std::ios::ate};

        if (!file_dump.good())
        {
            return;
        }

        std::ifstream::pos_type len = file_dump.tellg();

        if (len <= 0)
        {
            return;
        }

        const size_t cached_count = len / key_val_size;

        std::vector<std::byte> data(len);

        file_dump.seekg(0, std::ios::beg);
        file_dump.read(reinterpret_cast<char*>(data.data()), len);

        file_dump.close();

        storage.reserve(data.size() / key_val_size);

        storage = this->deserialize(std::span{data.data(), data.size()}, cached_count);
    }

    /// @brief Dumps cache content to an associated file
    void dump_to_file()
    {
        std::fstream file_dump(get_cache_file_name(), std::ios::out | std::ios::binary);
        const auto data = this->serialize();
        file_dump.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::unordered_map<Key, Value> storage;
};

/**
 * ConcurrentCache class
 *
 * Child for Cache decorating methods enabling concurrent usage
 *
 * @tparam Key type of a key for internal std::unordered_map
 * @tparam Value type of cached values
 * @tparam Tag file tag string for identificaion
 */
template <typename Key, typename Value, StringLiteral Tag = "">
class ConcurrentCache : public Cache<Key, Value, Tag>
{
public:
    ConcurrentCache()
    {
        load_impl_ptr = &load_protected_impl;
    }

    /// @brief Obtaines value by provided key if present concurrently
    /// @param key key for value
    /// @return std::optional for value
    [[nodiscard]] std::optional<Value> load(const Key& key) const
    {
        return load_impl_ptr(*this, key);
    }

    /// @brief Obtaines value by provided key if present concurrently without lock
    /// @param key key for value
    /// @return std::optional for value
    [[nodiscard]] std::optional<Value> load_unprotected(const Key& key) const
    {
        return load_unprotected_impl(*this, key);
    }

    /// @brief Sets load implementation to optimize for stores availability
    /// @param can_store flag whether stores can occure
    void set_stores_availability(bool can_store) const
    {
        std::unique_lock lk{mtx};
        if (can_store)
        {
            load_impl_ptr = &load_protected_impl;
        }
        else
        {
            load_impl_ptr = &load_unprotected_impl;
        }
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

    /// @brief Saves value to the cache cuncurrently without lock
    /// @tparam V type for value. Required for perfect forwarding
    /// @param deps key for storing value
    /// @param value value to store at key
    template <typename V>
    void store_unprotected(const Key& deps, V&& value)
    {
        Cache<Key, Value>::store(deps, std::forward<V>(value));
    }

private:
    [[nodiscard]] static std::optional<Value> load_protected_impl(const ConcurrentCache& self, const Key& key)
    {
        std::shared_lock lk{self.mtx};
        return static_cast<const Cache<Key, Value>&>(self).load(key);
    }

    [[nodiscard]] static std::optional<Value> load_unprotected_impl(const ConcurrentCache& self, const Key& key)
    {
        return static_cast<const Cache<Key, Value>&>(self).load(key);
    }

    using load_impl_ptr_t = std::optional<Value>(*)(const ConcurrentCache& self, const Key&);

    mutable load_impl_ptr_t load_impl_ptr;
    mutable std::shared_mutex mtx;
};

}  // namespace Caching
