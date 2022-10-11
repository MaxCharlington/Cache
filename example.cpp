#include "cache.hpp"

using namespace Caching;

int main()
{
    double val_to_calculate;


    Dependances deps{1, 4.6};

    Cache<decltype(deps), double> cache{};
    cache.store(deps, 1.5);

    auto stored = cache.load(deps);
    if (stored.has_value())
    {
        val_to_calculate = stored.value();
    }
    else
    {
        val_to_calculate = 1 + 2;  // Heavy calculation
    }


    ConcurrentCache<decltype(deps), double> conc_cache{};  // Mutex protected version
    conc_cache.store(deps, 1.5);

    stored = conc_cache.load(deps);
    if (stored.has_value())
    {
        val_to_calculate = stored.value();
    }
    else
    {
        val_to_calculate = 1 + 2;  // Heavy calculation
    }
}
