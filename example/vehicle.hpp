#ifndef LINKER_SET_EXAMPLE_VEHICLE
#define LINKER_SET_EXAMPLE_VEHICLE

#include <cstddef>

#include <linker_set.hpp>

#include "registry.hpp"
#include "vehicle_spec.hpp"

template <typename EngineProperties, std::size_t NumPassenger,
    std::size_t MaxDrag>
struct Vehicle {
  inline static VehicleSpec vh_spec = {
      .engine_displacement = EngineProperties::get_displacement(),
      .passenger_capacity = NumPassenger,
      .top_speed = EngineProperties::max_speed() - MaxDrag,
  };

  LINKER_SET_ADD_MEMBER(vehicle_specs, vh_spec)
};

#endif
