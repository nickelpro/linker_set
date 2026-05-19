#ifndef LINKER_SET_EXAMPLE_REGISTRY
#define LINKER_SET_EXAMPLE_REGISTRY

#include <string>

#include <linker_set.hpp>

#include "vehicle_spec.hpp"

LINKER_SET_DECLARE(vehicle_specs, VehicleSpec)
LINKER_SET_DECLARE(drivers, std::string)

#endif // LINKER_SET_EXAMPLE_REGISTRY
