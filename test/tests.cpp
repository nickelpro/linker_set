#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <linker_set.hpp>

#include "util.hpp"

namespace rg = std::ranges;

static_assert(
    std::is_same_v<decltype(LINKER_SET_GET(add_mixed, 0)), int const&>);
static_assert(
    std::is_same_v<decltype(LINKER_SET_AT(add_mixed, 0)), int const&>);
static_assert(
    std::is_same_v<decltype(LINKER_SET_SPAN(add_mixed)[0]), int const* const&>);
static_assert(std::is_same_v<decltype(LINKER_SET_GET(add_mutable, 0)), int&>);
static_assert(std::is_same_v<decltype(LINKER_SET_AT(add_mutable, 0)), int&>);
static_assert(
    std::is_same_v<decltype(LINKER_SET_SPAN(add_mutable)[0]), int* const&>);

template <typename R, typename T, std::size_t N>
bool check_linker_set_contains(R ls, std::array<T, N> arr) {
  return rg::all_of(arr, [&](auto& el) { return rg::contains(ls, el); });
}

TEST_CASE("LINKER_SET_ADD_UNIQUE") {
  auto ls {LINKER_SET_RANGE(add_unique)};
  REQUIRE(rg::size(ls) == 4);
  REQUIRE(check_linker_set_contains(ls, std::array {1, 2, 3}));
  REQUIRE(rg::count_if(ls, [](auto i) { return i == 3; }) == 2);
}

TEST_CASE("LINKER_SET_ADD") {
  auto ls {LINKER_SET_RANGE(add_id)};
  REQUIRE(rg::size(ls) == 3);
  REQUIRE(check_linker_set_contains(ls, std::array {1, 2, 3}));
}

TEST_CASE("LINKER_SET_ADD_MEMBER") {
  auto ls {LINKER_SET_RANGE(add_member)};
  REQUIRE(rg::size(ls) == 3);
  REQUIRE(check_linker_set_contains(ls, std::array {1, 2, 3}));
}

TEST_CASE("LINKER_SET_DECLARE_MUTABLE") {
  auto ls {LINKER_SET_RANGE(add_mutable)};
  auto ls_idx {LINKER_SET_RANGE(add_mut_idx)};
  REQUIRE(rg::size(ls) == 2);
  REQUIRE(rg::size(ls_idx) == 2);
  REQUIRE(check_linker_set_contains(ls, std::array {10, 20}));

  for(auto& value : ls)
    value += 100;

  REQUIRE(check_linker_set_contains(ls, std::array {110, 120}));

  for(auto& value : ls)
    value -= 100;

  for(auto const& entry : ls_idx) {
    auto& value = LINKER_SET_GET(add_mutable, entry.idx);
    auto& checked_value = LINKER_SET_AT(add_mutable, entry.idx);

    REQUIRE(value == entry.v);
    REQUIRE(&value == &checked_value);

    auto original = value;
    checked_value += 7;
    REQUIRE(value == original + 7);
    checked_value = original;
  }

  REQUIRE(check_linker_set_contains(ls, std::array {10, 20}));
}

TEST_CASE("Mixed LINKER_SET_ADD*") {
  auto ls {LINKER_SET_RANGE(add_mixed)};
  REQUIRE(rg::size(ls) == 4);
  REQUIRE(check_linker_set_contains(ls, std::array {1, 2, 3, 4}));
}

TEST_CASE("LINKER_SET_ADD_INDEX / MEMBER") {
  auto ls_idx {LINKER_SET_RANGE(add_idx)};
  REQUIRE(rg::size(ls_idx) == 3);
  REQUIRE(rg::all_of(ls_idx, [](const auto& el) {
    return el.v == LINKER_SET_GET(add_mixed, el.idx);
  }));
}

TEST_CASE("LINKER_SET_GET / AT") {
  auto ls_idx {LINKER_SET_RANGE(add_idx)};

  for(auto const& entry : ls_idx) {
    REQUIRE(LINKER_SET_GET(add_mixed, entry.idx) == entry.v);
    REQUIRE(LINKER_SET_AT(add_mixed, entry.idx) == entry.v);
    REQUIRE(&LINKER_SET_GET(add_mixed, entry.idx) ==
        &LINKER_SET_AT(add_mixed, entry.idx));
  }

  REQUIRE_THROWS_AS(
      LINKER_SET_AT(add_mixed, std::numeric_limits<std::size_t>::max()),
      std::out_of_range);
}
