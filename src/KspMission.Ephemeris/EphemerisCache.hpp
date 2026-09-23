#pragma once
#include "Ephemeris.hpp"
#include <filesystem>

namespace ksp {
class EphemerisCacheError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
void save_ephemeris_cache(const Ephemeris& ephemeris,const Snapshot& snapshot,
    const Settings& settings,const std::filesystem::path& path);
Ephemeris load_ephemeris_cache(const std::filesystem::path& path,const Snapshot& snapshot,
    const Settings& settings);
}
