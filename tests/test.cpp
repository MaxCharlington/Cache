#include <cassert>

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
}
