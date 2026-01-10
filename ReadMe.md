# linker_set

Header-only C++ macros for defining "linker sets": arrays of pointers that the
linker gathers across translation units. This is handy for registries and
plugin-style discovery without a central list.

Supports MSVC (COFF), ELF, and Mach-O. Requires GCC 14+, Clang 17+, or any
version of MSVC which supports C++20.

## Highlights
- No runtime registration or constructors required
- Coalesced and non-coalesced insertion modes
- Header-only, simple CMake targets
- Optional writable sections to resist variable ICF

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
LINKER_SET_ADD_UNIQUE(handlers, h2)

// main.cpp
for (auto p : LINKER_SET_SPAN(handlers)) {
  if (!p) continue; // MSVC can emit null slots with incremental linking
  std::printf("%s\n", p->name);
}
```

## API
- `LINKER_SET_DECLARE(tag, T)` - declare a linker set for type `T`
- `LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)` - always-unique entry
- `LINKER_SET_ADD_ID(tag, id, expr_lvalue)` - coalesce by `(tag, id)`
- `LINKER_SET_ADD(tag, variable)` - convenience form of `ADD_ID`
- `LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)` - class-scope form
- `LINKER_SET_ADD_MEMBER(tag, member)` - convenience form of `ADD_MEMBER_ID`
- `LINKER_SET_SPAN(tag)` - `std::span<T const* const>` view of entries

Notes:
- `expr_lvalue` must be an lvalue object (not a function).
- MSVC may emit null slots, especially with incremental linking; check for null.

## Build / Install
This repo builds provides a CMake interface library. There is a ready-made
vcpkg overlay port in `vcpkg/ports` for local use. Or just drop the
`src/linker_set.hpp` header directly into your project.

```cmake
find_package(linker_set CONFIG REQUIRED)
target_link_libraries(app PRIVATE linker_set::linker_set)
```

For writable, ICF-resistant linker sets:
```cmake
target_link_libraries(app PRIVATE linker_set::linker_set_writeable)
```

## Options
- `LS_LINKER_SET_WRITABLE=1` places linker sets in writable sections to defeat
  variable ICF (useful with LTO or MSVC /Gw /OPT:ICF).

## Tests
```sh
cmake -S . -B build -DLINKER_SET_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```
