#ifndef LINKER_SET_EXAMPLE_VEHICLE_SPEC
#define LINKER_SET_EXAMPLE_VEHICLE_SPEC

#include <cstddef>
#include <format>

struct VehicleSpec {
  std::size_t engine_displacement;
  std::size_t passenger_capacity;
  std::size_t top_speed;
};

template <>
struct std::formatter<VehicleSpec> {
  constexpr auto parse(std::format_parse_context& ctx) {
    auto it = ctx.begin();
    if(it != ctx.end() && *it != '}')
      throw std::format_error("invalid format for VehicleSpec");
    return it;
  }

  auto format(const VehicleSpec& vs, auto& ctx) const {
    auto out = std::format_to(ctx.out(),
        "Engine Displacement: {}\nPassenger Capacity: {}\nTop Speed: {}",
        vs.engine_displacement, vs.passenger_capacity, vs.top_speed);
    return out;
  }
};

#endif // LINKER_SET_EXAMPLE_VEHICLE_SPEC
