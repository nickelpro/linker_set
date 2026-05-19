#include <print>

#include "registry.hpp"

//------------------------------------------------------------------------------
// Linker Set Example
//
// This example is of a trivial program which prints some metadata collected
// from two other translation units, sedan.cpp and van.cpp. These files contain
// functions which specialize the Vehicle template from vehicle.hpp.
//
// The goal of the program is to collect metadata about all the possible
// vehicles which might be used across all the translation units in any
// context. This metadata is recorded as VehicleSpec objects (from
// vehicle_spec.hpp).
//
// It shouldn't be necessary to actually call, or indeed, anywhere in the life
// of the program actually construct one of these template specializations.
// Their existence in the source should be enough to collect metadata about
// them.
//
// As a secondary goal, we also record some strings of driver names, which are
// global variables with internal linkage. We would like to get access to these
// in the main function as well.
//------------------------------------------------------------------------------

int main() {

  std::println("Specs of available vehicles:");
  for(auto const& vh : LINKER_SET_SPAN(vehicle_specs)) {
    if(!vh)
      continue;
    std::println("{}\n", *vh);
  }

  std::println("Known drivers:");
  for(auto const& dv : LINKER_SET_SPAN(drivers)) {
    if(!dv)
      continue;
    std::println("{}", *dv);
  }
}
