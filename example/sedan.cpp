#include <cstddef>

#include <linker_set.hpp>

#include "registry.hpp"
#include "vehicle.hpp"

struct SedanEngine {
  static consteval std::size_t get_displacement() {
    return 1400;
  }

  static consteval std::size_t max_speed() {
    return 240;
  }
};


namespace {

// Available sedans
auto make_twodoor() {
  return Vehicle<SedanEngine, 2, 60> {};
}

auto make_fourdoor() {
  return Vehicle<SedanEngine, 5, 80> {};
}

// Known sedan drivers
std::string Jimmy = "Jimmy";
LINKER_SET_ADD(drivers, Jimmy);

std::string Susan = "Susan";
LINKER_SET_ADD(drivers, Susan);

} // namespace
