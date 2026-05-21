# linker_set

Header-only C++20 macros for defining "linker sets": arrays of pointers that the
linker gathers across translation units. This is handy for registries, allowing
discovery without a central list.

Supports COFF, ELF, and Mach-O. Requires GCC 14+, Clang 17+, or any
version of MSVC which supports C++20.

## Highlights
- No runtime registration or constructors required
- Mutable and immutable sets
- Range-based iterator
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
for (const auto& p : LINKER_SET_RANGE(handlers)) {
  std::print("{}", p.name);
}
```

A slightly more involved example is available in the `example` folder, which
illustrates a more typical usage of collecting metadata about template
specializations used throughout a program.

It can be built from the top-level CMake project with
`-DLINKER_SET_BUILD_EXAMPLE=ON`.

## API

A linker set is identified by a `tag` and stores pointers to objects of one
type `T` gathered across translation units. The declaration controls whether
the set exposes immutable or mutable references when it is read.

### Declaration

```cpp
LINKER_SET_DECLARE(tag, T)
LINKER_SET_DECLARE_MUTABLE(tag, T)
```

- `LINKER_SET_DECLARE` exposes elements as `T const&`.
- `LINKER_SET_DECLARE_MUTABLE` exposes elements as `T&`.
- The same `tag` and `T` must be used consistently anywhere the set is
  declared or populated.

### Registration

```cpp
LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)
LINKER_SET_ADD_ID(tag, id, expr_lvalue)
LINKER_SET_ADD(tag, variable)
LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)
LINKER_SET_ADD_MEMBER(tag, member)
```

- `LINKER_SET_ADD_UNIQUE` always emits a distinct entry. If two entries point
  at the same object, both remain in the final set.
- `LINKER_SET_ADD_ID` coalesces by `(tag, id)` symbol identity, so repeating
  the same `id` across translation units produces one logical entry.
- `LINKER_SET_ADD` is shorthand for `LINKER_SET_ADD_ID(tag, variable,
  variable)` and is convenient for namespace-scope variables.
- `LINKER_SET_ADD_MEMBER_ID` is the class-scope form of `LINKER_SET_ADD_ID`.
  It is primarily useful for templates and inline static data members.
- `LINKER_SET_ADD_MEMBER` is shorthand for `LINKER_SET_ADD_MEMBER_ID(tag,
  member, member)`.

All registration macros require an lvalue expression whose object type matches
`T`, with cv-qualification compatible with the set declaration. Mutable sets
therefore require non-const objects.

### Iteration And Access

```cpp
LINKER_SET_RANGE(tag)
LINKER_SET_INDEX(tag, id)
LINKER_SET_INDEX_MEMBER(tag, id)
LINKER_SET_GET(tag, index)
LINKER_SET_AT(tag, index)
```

- `LINKER_SET_RANGE` returns a lightweight forward range over the set values.
  Use it in range-for loops or with standard range algorithms.
- `LINKER_SET_INDEX` returns the slot index for an entry added with
  `LINKER_SET_ADD_ID`. `LINKER_SET_INDEX_MEMBER` does the same for
  `LINKER_SET_ADD_MEMBER_ID`.
- `LINKER_SET_GET` performs unchecked indexed access.
- `LINKER_SET_AT` performs checked indexed access and throws
  `std::out_of_range` on an invalid index.

`LINKER_SET_INDEX*` must be used in the same scope as the corresponding
`LINKER_SET_ADD*_ID` invocation and is not compatible with
`LINKER_SET_ADD_UNIQUE`.

### Notes

- The `id` argument is only used for linker coalescing. No runtime metadata is
  stored for it.
- On MSVC, linker sets can contain null holes, especially with incremental
  linking. `LINKER_SET_RANGE` skips those holes while iterating.

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
