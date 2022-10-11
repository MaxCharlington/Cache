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

template <typename... T>
class Dependances {
public:
    constexpr Dependances(T... args) : vals{std::make_tuple(args...)} {}
    constexpr Dependances() = default;

    constexpr bool operator==(const Dependances& deps) const {
        return this->vals == deps.vals;
    }

    std::array<std::byte, sizeof(std::tuple<T...>)> serialize() const {
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

template <typename... T>
struct std::hash<Dependances<T...>> {
    constexpr std::size_t operator()(
        const Dependances<T...>& deps) const noexcept {
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

constexpr auto serialize(auto val) -> std::array<std::byte, sizeof(val)> {
    if constexpr (requires { val.serialize(); }) {
        return val.serialize();
    } else {
        return std::bit_cast<std::array<std::byte, sizeof(val)>>(val);
    }
}

template <typename Key, typename Value>
class Cache {
public:
    Cache()
        : cache_file_name{[] {
              using namespace std::string_literals;
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
          }()} {
        // load from file
    }

    ~Cache() {
        std::ofstream ofp(cache_file_name, std::ios::out | std::ios::binary);
        const auto data = this->serialize();
        ofp.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    const std::string& get_cache_file_name() const { return cache_file_name; }

    std::optional<Value> load(const Key& key) const {
        std::shared_lock lk{mtx};
        if (storage.find(key) != storage.end()) {
            return storage.at(key);
        }
        return std::nullopt;
    }

    template <typename V>  // Forwarding reference for value
    void store(const Key& deps, V&& value) {
        std::unique_lock lk{mtx};
        storage[deps] = std::forward<V>(value);
    }

    std::vector<std::byte> serialize() const {
        std::shared_lock lk{mtx};
        std::vector<std::byte> binary_data{storage.size() *
                                           (sizeof(Key) + sizeof(Value))};
        for (const auto& [key, value] : storage) {
            auto bin_key = ::serialize(key);
            std::ranges::for_each(
                bin_key, [&](auto byte) { binary_data.push_back(byte); });
            auto bin_value = ::serialize(value);
            std::ranges::for_each(
                bin_value, [&](auto byte) { binary_data.push_back(byte); });
        }
        return binary_data;
    }

private:
    mutable std::shared_mutex mtx;
    const std::string cache_file_name;
    std::unordered_map<Key, Value> storage;
};
