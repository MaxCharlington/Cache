# Cache

Library to cache various values by keys capable of saving to file between program runs.
Cache has two implementations:
* Simple cache
* Concurrent cache

Underlying implementation uses std::unordered_map and std::mutex for cuncurrent implementation.

Library is written in pure C++20, built with g++-11. It also provides CMake interface.
