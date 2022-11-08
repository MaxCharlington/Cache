#include <cassert>
#include <complex>

#include "../cache.hpp"

int main() {
    using namespace Caching;

    { // Basic
        Cache<Dependances<int>, int> cache;
        cache.store({1}, 1);
        auto stored = cache.load({1});
        assert(stored.has_value() && stored.value() == 1);
    }

    { // From dump file
        Cache<Dependances<int>, int> cache;
        auto stored = cache.load({1});
        assert(stored.has_value() && stored.value() == 1);
    }

    { // Non trivial type
        // static_assert(not std::is_trivially_copyable_v<std::complex<double>>);
        // Cache<Dependances<double>, std::complex<double>> cache;
    }

    { // Tags
        Cache<Dependances<int>, int, "First"> cache1;
        Cache<Dependances<int>, int, "Second"> cache2;
    }

    { // Guarantee no stores
        ConcurrentCache<Dependances<int>, double> cache;
        cache.store({1}, 4.5);
        auto stored = cache.load({1});

        cache.set_stores_availability(false);

        stored = cache.load({1});
    }
}
