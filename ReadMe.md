# linker_set

Header-only C++20 macros for defining "linker sets": arrays of pointers that the
linker gathers across translation units. This is handy for registries, allowing
discovery without a central list.

Supports COFF, ELF, and Mach-O. Requires GCC 14+, Clang 17+, or any
version of MSVC which supports C++20.

## Highlights
- No runtime registration or constructors required
- Coalesced and non-coalesced insertion modes
- Header-only, simple CMake targets
- Optional writable sections to resist constant merging/variable ICF

## Quick Example
```cpp
// registry.hpp
#include <linker_set.hpp>

struct Handler {
  const char* name;
  void (*fn)();
};

LINKER_SET_DECLARE(handlers, Handler)

// handlers_a.cpp
static Handler h1 {"one", &fn1};
LINKER_SET_ADD(handlers, h1)

// handlers_b.cpp
static Handler h2 {"two", &fn2};
LINKER_SET_ADD(handlers, h2)

// main.cpp
for (auto p : LINKER_SET_SPAN(handlers)) {
  if (!p) continue; // MSVC can emit null slots with incremental linking
  std::print("{}", p->name);
}
```

A slightly more involved example is available in the `example` folder, which
illustrates a more typical usage of collecting metadata about template
specializations used throughout a program.

It can be built from the top-level CMake project with
`-DLINKER_SET_BUILD_EXAMPLE=ON`.

## API
- `LINKER_SET_DECLARE(tag, T)` - declare symbols for accessing set `tag`
                                 containing pointers to type `T`
- `LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)` - always-unique entry
- `LINKER_SET_ADD_ID(tag, id, expr_lvalue)` - coalesce by `(tag, id)`
- `LINKER_SET_ADD(tag, variable)` - convenience form of `ADD_ID`
- `LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)` - class-scope form
- `LINKER_SET_ADD_MEMBER(tag, member)` - convenience form of `ADD_MEMBER_ID`
- `LINKER_SET_INDEX(tag, id)` - index of entry in set
- `LINKER_SET_INDEX_MEMBER(tag, id)` - class scope form
- `LINKER_SET_SPAN(tag)` - `std::span<T const* const>` view of entries

Notes:
- `expr_lvalue` must be an lvalue object (not a function).
- MSVC may emit null slots, especially with incremental linking; check for null.

## Build / Install
This repo builds provides a CMake interface library. There is a ready-made
vcpkg overlay port in `vcpkg/ports` for local use. Or just drop the
`src/linker_set.hpp` header directly into your project.

For a traditional CMake installation
```sh
git clone https://github.com/nickelpro/linker_set.git
cmake -S linker_set -B linker_set/build
cmake --install linker_set/build --prefix path/to/install/destination
```

The CMake targets can then be used:
```cmake
find_package(linker_set CONFIG REQUIRED)
target_link_libraries(app PRIVATE linker_set::linker_set)
```

For writable, ICF-resistant linker sets:
```cmake
target_link_libraries(app PRIVATE linker_set::linker_set_writable)
```

## Options
- `LS_LINKER_SET_WRITABLE=1` places linker sets in writable sections to defeat
  constant merging and variable ICF. In practice this is only necessary with
  `-fmerge-all-constants` on GCC.

## Tests
Tests require Catch2, it can be pulled it automatically with
`LINKER_SET_BOOTSTRAP_VCPKG`.
```sh
git clone https://github.com/nickelpro/linker_set.git
# Optionally use -DLINKER_SET_BOOTSTRAP_VCPKG=ON
cmake -S linker_set -B linker_set/build -DLINKER_SET_BUILD_TESTS=ON
cmake --build linker_set/build
ctest --test-dir linker_set/build
```

## Legal

This work is in the public domain, see `License` for dedication.
