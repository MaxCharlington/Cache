#include "cache.hpp"

int main() {
    Dependances deps{1, 4.6};

    Cache<decltype(deps), double> cache{};
    cache.store(deps, 1.5);

    auto stored = cache.load(deps);
    if (stored.has_value()) {
        double val = stored.value();
    }
}
