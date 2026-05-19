#include <cstddef>

#include <linker_set.hpp>

#include "registry.hpp"
#include "vehicle.hpp"

struct VanEngine {
  static consteval std::size_t get_displacement() {
    return 2000;
  }

  static consteval std::size_t max_speed() {
    return 200;
  }
};


namespace {

auto make_minivan() {
  return Vehicle<VanEngine, 8, 80> {};
}

auto make_cargovan() {
  return Vehicle<VanEngine, 4, 120> {};
}

// Known van drivers
std::string Alice = "Alice";
LINKER_SET_ADD(drivers, Alice);

std::string Bob = "Bob";
LINKER_SET_ADD(drivers, Bob);

} // namespace
