#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <linker_set.hpp>

#include "util.hpp"

namespace rg = std::ranges;

auto proj = [](auto p) { return p ? *p : 0; };

template <typename T>
bool check_linker_set_size(std::span<T const* const> sp, std::ptrdiff_t sz) {
  return std::ssize(sp) == sz || rg::count_if(sp, std::identity {}) == sz;
}

template <typename T, std::size_t N>
bool check_linker_set_contains(std::span<T const* const> sp,
    std::array<T, N> arr) {
  return rg::all_of(arr, [&](auto& el) { return rg::contains(sp, el, proj); });
}

TEST_CASE("LINKER_SET_ADD_UNIQUE") {
  auto sp {LINKER_SET_SPAN(add_unique)};
  REQUIRE(check_linker_set_size(sp, 4));
  REQUIRE(check_linker_set_contains(sp, std::array {1, 2, 3}));
  REQUIRE(rg::count_if(sp, [](auto i) { return i == 3; }, proj) == 2);
}

TEST_CASE("LINKER_SET_ADD") {
  auto sp {LINKER_SET_SPAN(add_id)};
  REQUIRE(check_linker_set_size(sp, 3));
  REQUIRE(check_linker_set_contains(sp, std::array {1, 2, 3}));
}

TEST_CASE("LINKER_SET_ADD_MEMBER") {
  auto sp {LINKER_SET_SPAN(add_member)};
  REQUIRE(check_linker_set_size(sp, 3));
  REQUIRE(check_linker_set_contains(sp, std::array {1, 2, 3}));
}

TEST_CASE("Mixed LINKER_SET_ADD*") {
  auto sp {LINKER_SET_SPAN(add_mixed)};
  REQUIRE(check_linker_set_size(sp, 4));
  REQUIRE(check_linker_set_contains(sp, std::array {1, 2, 3, 4}));
}

TEST_CASE("LINKER_SET_ADD_INDEX / MEMBER") {
  auto sp_mixed {LINKER_SET_SPAN(add_mixed)};
  auto sp_idx {LINKER_SET_SPAN(add_idx)};
  REQUIRE(check_linker_set_size(sp_idx, 3));
  for(auto ptr : sp_idx) {
    if(!ptr)
      continue;
    REQUIRE(ptr->v == *sp_mixed[ptr->idx]);
  }
}
