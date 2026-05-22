#ifndef LINKER_SET_HPP
#define LINKER_SET_HPP

// linker_set.hpp
//
// 1) Declare a linker set with:
//    - LINKER_SET_DECLARE(tag, T)          : container for immutable values
//    - LINKER_SET_DECLARE_MUTABLE(tag, T)  : container for mutable values
// 2) Five insertion interfaces:
//    - LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)        : always-unique entry (no
//    coalescing)
//    - LINKER_SET_ADD_ID(tag, id, expr_lvalue)        : coalesced by (tag,id)
//    symbol identity
//    - LINKER_SET_ADD(tag, variable)                  : convenience macro;
//    same as LINKER_SET_ADD_ID(tag, variable, variable)
//    - LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue) : coalesced by (tag,id)
//    symbol identity; class-scope form
//    - LINKER_SET_ADD_MEMBER(tag, member)             : convenience macro;
//    same as LINKER_SET_ADD_MEMBER_ID(tag, member, member)
// 3) Iterate with: LINKER_SET_RANGE(tag)
//
// Optionally:
//   4) Find an index within the linker set with:
//      - LINKER_SET_INDEX(tag, id)        : namespace scope
//      - LINKER_SET_INDEX_MEMBER(tag, id) : class scope
//    Must be called in the same scope as the LINKER_SET_ADD*. Not compatible
//    with LINKER_SET_ADD_UNIQUE.
//
//   5) Index into the linker set with:
//      - LINKER_SET_GET(tag, index) : unchecked access
//      - LINKER_SET_AT(tag, index)  : checked access, throws std::out_of_range
//
// Build Options:
//   - LS_LINKER_SET_WRITABLE (default OFF) : Make linker sets writable for the
//   purpose of defeating constant merging and variable ICF optimizations.
//   Rarely necessary, only GCC with -fmerge-all-constants has been shown to
//   require it.
//
// Notes:
// - No metadata is stored; the identifier exists only for linker coalescing.
// - Set may contain holes on MSVC, especially with incremental linking.
// - ELF uses [[gnu::retain]] to survive --gc-sections.
// - Windows:
//     * clang-cl: uses [[gnu::used]] on COFF to keep entries.
//     * cl.exe: best-effort "touch" in CRT$XCU to resist /Zc:inline and
//     /OPT:REF.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>

// Silence dumb warning in paranoid consumers
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#endif

#if defined(_MSC_VER)
#define LS_PLATFORM_MSVC 1
#elif defined(__APPLE__)
#define LS_PLATFORM_APPLE 1
#else
#define LS_PLATFORM_ELF 1
#endif

#define LS_STR_(x) #x
#define LS_STR(x) LS_STR_(x)

#define LS_CAT_(a, b) a##b
#define LS_CAT2(a, b) LS_CAT_(a, b)
#define LS_CAT3(a, b, c) LS_CAT2(LS_CAT2(a, b), c)
#define LS_CAT4(a, b, c, d) LS_CAT2(LS_CAT3(a, b, c), d)
#define LS_CAT5(a, b, c, d, e) LS_CAT2(LS_CAT4(a, b, c, d), e)

//------------------------------------------------------------------------------
// Optional: make linker-set storage writable.
//
// Define LS_LINKER_SET_WRITABLE to defeat constant merging/variable ICF and
// ensure linker sets are placed in writable sections.
//
// Constant merging and variable ICF can be tricky under two cases:
//   * The direct case: two LINKER_SET_ADD_UNIQUE entries point to the same
//     object, which makes them direct candidates for constant merging.
//
//   * The indirect case: two constant objects with individual entries in the
//     linker set are merged, becoming identity-identical. The linker set
//     entries for these objects are now candidates for constant merging or
//     ICF themselves.
//
// Testing has only shown both to be possible under GCC with
// -fmerge-all-constants. No tested linker will perform the latter under any
// optimization conditions.
//------------------------------------------------------------------------------

#if LS_LINKER_SET_WRITABLE
#define LS_MSVC_SEC_PERMS read, write
#define LS_APPLE_SEG "__DATA"

#if LS_PLATFORM_ELF
#define LS_SLOT_CONST
#else
#define LS_SLOT_CONST const
#endif

#else
#define LS_MSVC_SEC_PERMS read
#define LS_APPLE_SEG "__DATA_CONST"
#define LS_SLOT_CONST const
#endif

//------------------------------------------------------------------------------
// Utility macros
//------------------------------------------------------------------------------

#if LS_PLATFORM_MSVC
#define LS_SLOT_TYPE(tag) decltype(ls_start_##tag)
#define LS_BEGIN(tag) (&ls_start_##tag + 1)
#define LS_END(tag) (&ls_end_##tag)
#elif LS_PLATFORM_APPLE
#define LS_SLOT_TYPE(tag) std::remove_extent_t<decltype(ls_start_##tag)>
#define LS_BEGIN(tag) (ls_start_##tag)
#define LS_END(tag) (ls_end_##tag)
#else // ELF
#define LS_SLOT_TYPE(tag) std::remove_extent_t<decltype(__start_ls_##tag)>
#define LS_BEGIN(tag) (__start_ls_##tag)
#define LS_END(tag) (__stop_ls_##tag)
#endif

// LS_SLOT_TYPE always returns the slot type as const, but the individual slot
// declarations might not actually be const if LS_LINKER_SET_WRITABLE=1. We
// still treat the slot type as const in all other contexts.
#define LS_SLOT_DECL(tag)                                                      \
  constinit std::remove_const_t<LS_SLOT_TYPE(tag)> LS_SLOT_CONST

#define LS_STATIC_CHECK(tag, expr_lvalue)                                      \
  static_assert(std::is_lvalue_reference_v<decltype((expr_lvalue))>,           \
      "linker-set entry must be an lvalue expression");                        \
  static_assert(                                                               \
      std::is_same_v<std::remove_cvref_t<decltype((expr_lvalue))>,             \
          std::remove_const_t<std::remove_pointer_t<LS_SLOT_TYPE(tag)>>>,      \
      "linker-set entry type must exactly match LINKER_SET_DECLARE(tag, T)");  \
  static_assert(                                                               \
      std::is_convertible_v<decltype(&(expr_lvalue)), LS_SLOT_TYPE(tag)>,      \
      "linker-set entry cv-qualification is incompatible with this set");

//------------------------------------------------------------------------------
// clang-cl supports [[gnu::used]], which both emits and retains sections on
// COFF; either by adding /INCLUDE to .drectve, or (for variables with internal
// linkage and an explicit section) not marking sections as COMDAT.
//
// MSVC-only (cl.exe) best-effort retention: make the definition have an
// observable side-effect so /Zc:inline is unable to discard it. Under current
// semantics this creates a stable GC root in CRT$XCU, but nominally MSVC could
// change this in the future.
//------------------------------------------------------------------------------

#if LS_PLATFORM_MSVC && !defined(__clang__)
#define LS_MSVC_USED
#define LS_MSVC_TOUCH(sym)                                                     \
  ((void) *reinterpret_cast<unsigned char const volatile*>(&(sym)), 0)
#define LS_MSVC_TOUCH_LOCAL(sym)                                               \
  namespace {                                                                  \
  int const LS_CAT2(ls_touch_u__, __COUNTER__) = LS_MSVC_TOUCH(sym);           \
  }
#define LS_MSVC_TOUCH_INTERNAL_PTR(tag, id)                                    \
  static inline int const LS_CAT5(ls_touch_, tag, __, id, _internal__ptr) =    \
      LS_MSVC_TOUCH(LS_CAT5(ls_id_, tag, __, id, _f)());
#else
#define LS_MSVC_USED [[gnu::used]]
#define LS_MSVC_TOUCH(sym)
#define LS_MSVC_TOUCH_LOCAL(sym)
#define LS_MSVC_TOUCH_INTERNAL_PTR(tag, id)
#endif

#define LS_MSVC_SEC(tag, subsec) LS_STR(LS_CAT4(ls$, tag, $, subsec))

#define LS_APPLE_SEC(tag) LS_APPLE_SEG ",ls_" #tag
#define LS_APPLE_STARTSYM(tag) "section$start$" LS_APPLE_SEG "$ls_" #tag
#define LS_APPLE_ENDSYM(tag) "section$end$" LS_APPLE_SEG "$ls_" #tag

// Most of this is straightforward inline ASM machinery, but the input
// constraints are tricky, see:
// https://maskray.me/blog/2024-01-30-raw-symbol-names-in-inline-assembly

#if LS_LINKER_SET_WRITABLE || defined(__pic__) || defined(__pie__)
#define LS_ELF_GCC_ASM_FLAGS "awR"
#else
#define LS_ELF_GCC_ASM_FLAGS "aR"
#endif

#if defined(__arm__)
#define LS_ELF_GCC_PROGBITS "%progbits"
#define LS_ELF_GCC_OBJECT "%object"
#define LS_ELF_GCC_INPUT_CONSTRAINT "US"

#else

#define LS_ELF_GCC_PROGBITS "@progbits"
#define LS_ELF_GCC_OBJECT "@object"

#if defined(__aarch64__)
#define LS_ELF_GCC_INPUT_CONSTRAINT "S"
#elif defined(__i386__) || defined(__x86_64__)
#define LS_ELF_GCC_INPUT_CONSTRAINT "Ws"
#else
#define LS_ELF_GCC_INPUT_CONSTRAINT "s"
#endif

#endif

// We could use the full GCC 15/16 support for top-level extended asm, the
// associated new input constraints ":" / "-s", and new generic operand
// modifier "cc"; but this comes with a host of other problems:
//   * In GCC 15, top-level extended asm did not support LTO at all
//   * "-s" is never supported inside functions, which makes it worthless for
//     the class / template member case.
//   * The reference created by top-level extended asm does not preserve
//     internal linkage variables, which are then culled by dead-code
//     elimination.
//
// So, where available, we use ":" for defining variables, "cc" for printing,
// and give up on the rest for now.

// clang-format off
#if __GNUC__ >= 15
#define LS_ELF_GCC_DEFINE_CONSTRAINT ":"
#define LS_ELF_GCC_SYMPRINT(N) LS_STR(LS_CAT2(%cc, N))
#else
#define LS_ELF_GCC_DEFINE_CONSTRAINT LS_ELF_GCC_INPUT_CONSTRAINT
#if defined(__i386__) || defined(__x86_64__)
#define LS_ELF_GCC_SYMPRINT(N) LS_STR(LS_CAT2(%p, N))
#else
#define LS_ELF_GCC_SYMPRINT(N) LS_STR(LS_CAT2(%c, N))
#endif
#endif
// clang-format on

//------------------------------------------------------------------------------
// Two problems with GCC
//
// First:
// In non-LTO builds, GCC will insanely group inline declarations into section
// groups based on their section name, not their symbol name. The group
// identifier is a random (first? last?) symbol from the section.
//
// This is pants-on-head compiler behavior and breaks all reasonable C++ code.
// See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=116184
//
// To work around this we create an otherwise pointless function to contain the
// necessary inline asm to define a variable.
//
//
// Second:
// GCC is a real pain in the ass about section flags matching on non-LTO builds.
// This creates a problem for linker sets which mix unique (ie ADD_UNIQUE) and
// non-unique entries inside the same TU.
//
// We could solve this by hijacking the [[gnu::section]] attribute and injecting
// our own section flags (followed by a line comment to reject GCC's own
// contribution). Once hijacked, we could use the GCC assembler extension
// "unique,<N>" to break out of the flag matching behavior.
//
// However, we need to write inline asm for the non-unique entries anyway, so
// we make those the unique section. We only need a single assembler section
// instance for all grouped variables, so we use "unique,0" for all non-unique
// entries.
//
// This is very confusing. The "unique" tells GCC to put the group flagged
// variables (variables which can be coalesced across TUs) into their own
// section instance, which is "unique" (read: distinct) instance from the one
// containing the non-group flagged variables which cannot be coalesced.
//
// This doesn't mean all group flagged variables are in the same physical
// section, as they have distinct section groups and will generate their own
// section header entries. It means the assembler views them as belonging to a
// single logical section, and all the constituent parts of that logical
// section will have matching flags. This sparks joy in the GCC assembler.
//
// Clang doesn't care about any of this and does the right thing.
//------------------------------------------------------------------------------

// clang-format off
#define LS_ELF_GCC_ASM(tag, id, expr_lvalue)                                   \
  std::remove_const_t<LS_SLOT_TYPE(tag)> LS_SLOT_CONST                         \
      LS_CAT4(ls_id_, tag, __, id);                                            \
  [[gnu::used]]                                                                \
  inline void LS_CAT5(ls_id_, tag, __, id, _emit)() {                          \
    __asm__ __volatile__(                                                      \
      ".pushsection ls_" #tag ",\"" LS_ELF_GCC_ASM_FLAGS "G\""                 \
      "," LS_ELF_GCC_PROGBITS "," LS_ELF_GCC_SYMPRINT(0) ",comdat,unique,0\n"  \
      ".balign " LS_STR(__SIZEOF_POINTER__) "\n"                               \
      ".globl " LS_ELF_GCC_SYMPRINT(0) "\n"                                    \
      ".type " LS_ELF_GCC_SYMPRINT(0) "," LS_ELF_GCC_OBJECT "\n"               \
      ".size " LS_ELF_GCC_SYMPRINT(0) "," LS_STR(__SIZEOF_POINTER__) "\n"      \
      LS_ELF_GCC_SYMPRINT(0) ":\n"                                             \
      "  .dc.a " LS_ELF_GCC_SYMPRINT(1) "\n"                                   \
      ".popsection\n"                                                          \
      :: LS_ELF_GCC_DEFINE_CONSTRAINT (&(LS_CAT4(ls_id_, tag, __, id))),       \
         LS_ELF_GCC_INPUT_CONSTRAINT (&(expr_lvalue))                          \
    );                                                                         \
  }
// clang-format on


//------------------------------------------------------------------------------
// DECLARE
// Intened to be used in a header; required to be visible for all other macros
//------------------------------------------------------------------------------

#if LS_PLATFORM_MSVC
// Despite what informal documentation might imply, neither link.exe nor
// lld-link will fold read-only data across different input sections. The
// sentinels are always safe from ICF.

#define LINKER_SET_DECLARE_IMPL(tag, ptr)                                      \
  __pragma(section(LS_MSVC_SEC(tag, a), LS_MSVC_SEC_PERMS));                   \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  __pragma(section(LS_MSVC_SEC(tag, z), LS_MSVC_SEC_PERMS));                   \
  extern "C" {                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, a))) inline ptr const ls_start_##tag =  \
      nullptr;                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, z))) inline ptr const ls_end_##tag =    \
      nullptr;                                                                 \
  }                                                                            \
  constinit inline std::atomic<std::size_t> ls_size_cache_##tag =              \
      std::numeric_limits<std::size_t>::max();

#define LS_SIZE_CACHE(tag) (&ls_size_cache_##tag)

#elif LS_PLATFORM_APPLE

#define LINKER_SET_DECLARE_IMPL(tag, ptr)                                      \
  extern "C" {                                                                 \
  [[gnu::weak]]                                                                \
  extern ptr const ls_start_##tag[] __asm(LS_APPLE_STARTSYM(tag));             \
  [[gnu::weak]]                                                                \
  extern ptr const ls_end_##tag[] __asm(LS_APPLE_ENDSYM(tag));                 \
  }

#define LS_SIZE_CACHE(tag) nullptr

#else // ELF

#define LINKER_SET_DECLARE_IMPL(tag, ptr)                                      \
  extern "C" {                                                                 \
  [[gnu::weak]]                                                                \
  extern ptr const __start_ls_##tag[];                                         \
  [[gnu::weak]]                                                                \
  extern ptr const __stop_ls_##tag[];                                          \
  }

#define LS_SIZE_CACHE(tag) nullptr

#endif

#define LINKER_SET_DECLARE(tag, T) LINKER_SET_DECLARE_IMPL(tag, T const*)
#define LINKER_SET_DECLARE_MUTABLE(tag, T) LINKER_SET_DECLARE_IMPL(tag, T*)

//------------------------------------------------------------------------------
// ADD_UNIQUE (non-coalescing; namespace-scope form)
//------------------------------------------------------------------------------

#if LS_PLATFORM_MSVC

#define LS_MSVC_ADD_IMPL(tag, name, expr_lvalue)                               \
  namespace {                                                                  \
  LS_MSVC_USED                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, m))) LS_SLOT_DECL(tag) name = &(        \
      expr_lvalue);                                                            \
  LS_MSVC_TOUCH_LOCAL(name)                                                    \
  }

#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  LS_MSVC_ADD_IMPL(tag, LS_CAT2(ls_u__, __COUNTER__), expr_lvalue)

#elif LS_PLATFORM_APPLE

#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  namespace {                                                                  \
  [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]] LS_SLOT_DECL(tag)          \
      LS_CAT2(ls_u__, __COUNTER__) = &(expr_lvalue);                           \
  }

#else // ELF

#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  namespace {                                                                  \
  [[gnu::used]] [[gnu::retain]] [[gnu::section("ls_" #tag)]] LS_SLOT_DECL(tag) \
      LS_CAT2(ls_u__, __COUNTER__) = &(expr_lvalue);                           \
  }

#endif

//------------------------------------------------------------------------------
// ADD_ID (coalesced by symbol identity (tag,id); namespace-scope form)
//------------------------------------------------------------------------------

#if LS_PLATFORM_MSVC

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  LS_MSVC_USED                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, m))) inline LS_SLOT_DECL(tag)           \
      LS_CAT4(ls_id_, tag, __, id) = &(expr_lvalue);                           \
  LS_MSVC_TOUCH_LOCAL(LS_CAT4(ls_id_, tag, __, id))

#elif LS_PLATFORM_APPLE

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]] inline LS_SLOT_DECL(tag)   \
      LS_CAT4(ls_id_, tag, __, id) = &(expr_lvalue);

#else // ELF

#if defined(__clang__)

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  [[gnu::used]] [[gnu::retain]] [[gnu::section(                                \
      "ls_" #tag)]] inline LS_SLOT_DECL(tag) LS_CAT4(ls_id_, tag, __, id) =    \
      &(expr_lvalue);

#else // GCC

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  extern LS_ELF_GCC_ASM(tag, id, expr_lvalue)

#endif

#endif

//------------------------------------------------------------------------------
// ADD
// Convenience macro; suitable for top-level variables in headers. Same as
// ADD_ID, but uses the variable identifier as the id.
//------------------------------------------------------------------------------

#define LINKER_SET_ADD(tag, variable) LINKER_SET_ADD_ID(tag, variable, variable)

//------------------------------------------------------------------------------
// ADD_MEMBER_ID (coalesced by symbol identity (tag,id); class-scope form)
// Intended primarily for templates: one entry per specialization
//------------------------------------------------------------------------------

#if LS_PLATFORM_MSVC

#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  LS_MSVC_USED                                                                 \
  static auto& LS_CAT5(ls_id_, tag, __, id, _f)() {                            \
    LS_MSVC_USED                                                               \
    __declspec(allocate(LS_MSVC_SEC(tag, m))) static LS_SLOT_DECL(tag)         \
        ls_internal__ptr = &(expr_lvalue);                                     \
    return ls_internal__ptr;                                                   \
  }                                                                            \
  LS_MSVC_TOUCH_INTERNAL_PTR(tag, id)

#elif LS_PLATFORM_APPLE
// [[gnu::used]] does not constitute an ODR use on AppleClang, it doesn't
// instantiate the definition, so we pull the ol' static_assert trick. See
// D56928 (https://reviews.llvm.org/D56928), and subsequent revert.

#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  [[gnu::used]]                                                                \
  static auto& LS_CAT5(ls_id_, tag, __, id, _f)() {                            \
    [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]] static LS_SLOT_DECL(tag) \
        ls_internal__ptr = &(expr_lvalue);                                     \
    return ls_internal__ptr;                                                   \
  }                                                                            \
  static_assert(LS_CAT5(ls_id_, tag, __, id, _f));

#else // ELF

#if defined(__clang__)

#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  [[gnu::used]]                                                                \
  static auto& LS_CAT5(ls_id_, tag, __, id, _f)() {                            \
    [[gnu::used]] [[gnu::retain]] [[gnu::section(                              \
        "ls_" #tag)]] static LS_SLOT_DECL(tag) ls_internal__ptr =              \
        &(expr_lvalue);                                                        \
    return ls_internal__ptr;                                                   \
  }

#else // GCC

// clang-format off
#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  static LS_ELF_GCC_ASM(tag, id, expr_lvalue)                                  \
  static auto& LS_CAT5(ls_id_, tag, __, id, _f)() {                            \
    return LS_CAT4(ls_id_, tag, __, id);                                       \
  }
// clang-format on

#endif

#endif

//------------------------------------------------------------------------------
// ADD_MEMBER
// Convenience macro; suitable for top-level members. Same as ADD_MEMBER_ID,
// but uses the member identifier as the id.
//------------------------------------------------------------------------------

#define LINKER_SET_ADD_MEMBER(tag, member)                                     \
  LINKER_SET_ADD_MEMBER_ID(tag, member, member)

//------------------------------------------------------------------------------
// SPAN
// Very bad, no good, access to the linker set section as a span of pointers.
// Chock full of undefined behavior, works in practice, almost certainly not
// what you want. Use LINKER_SET_RANGE.
//
// If iterating, must null check for holes on COFF
//------------------------------------------------------------------------------

#define LINKER_SET_SPAN(tag)                                                   \
  ([]() noexcept -> std::span<LS_SLOT_TYPE(tag)> {                             \
    auto b = LS_BEGIN(tag);                                                    \
    auto e = LS_END(tag);                                                      \
    if(!b || e < b)                                                            \
      return {};                                                               \
    return {b, static_cast<std::size_t>(e - b)};                               \
  }())

//------------------------------------------------------------------------------
// INDEX
//------------------------------------------------------------------------------

#define LINKER_SET_INDEXER(tag)                                                \
  ([](LS_SLOT_TYPE(tag) & entry) noexcept -> std::size_t {                     \
    auto b = reinterpret_cast<std::uintptr_t>(LS_BEGIN(tag));                  \
    auto e = reinterpret_cast<std::uintptr_t>(&entry);                         \
    auto diff = e - b;                                                         \
    return static_cast<std::size_t>(diff / sizeof(entry));                     \
  })
#define LINKER_SET_INDEX(tag, id)                                              \
  LINKER_SET_INDEXER(tag)(LS_CAT4(ls_id_, tag, __, id))

#define LINKER_SET_INDEX_MEMBER(tag, id)                                       \
  LINKER_SET_INDEXER(tag)(LS_CAT5(ls_id_, tag, __, id, _f)())

//------------------------------------------------------------------------------
// GET / AT
//------------------------------------------------------------------------------

#define LINKER_SET_GET(tag, index)                                             \
  ([](std::size_t idx) noexcept -> decltype(auto) {                            \
    auto b = reinterpret_cast<std::uintptr_t>(LS_BEGIN(tag));                  \
    auto addr = b + idx * sizeof(LS_SLOT_TYPE(tag));                           \
    return **reinterpret_cast<LS_SLOT_TYPE(tag)*>(addr);                       \
  }(index))

#define LINKER_SET_AT(tag, index)                                              \
  ([](std::size_t idx) -> decltype(auto) {                                     \
    auto b = reinterpret_cast<std::uintptr_t>(LS_BEGIN(tag));                  \
    auto e = reinterpret_cast<std::uintptr_t>(LS_END(tag));                    \
    auto addr = b + idx * sizeof(LS_SLOT_TYPE(tag));                           \
    if(addr < b || addr >= e)                                                  \
      throw std::out_of_range("LINKER_SET_AT");                                \
    return **reinterpret_cast<LS_SLOT_TYPE(tag)*>(addr);                       \
  }(index))

//------------------------------------------------------------------------------
// RANGE
//------------------------------------------------------------------------------

namespace linker_set_detail {

template <typename SlotType, std::atomic<std::size_t>* SizeCache>
class range {
  std::uintptr_t first_ = 0;
  std::uintptr_t last_ = 0;

public:
  using element_type = std::remove_pointer_t<SlotType>;

  constexpr range(std::uintptr_t first, std::uintptr_t last) noexcept {
    if(!first || last < first)
      return;
    first_ = first;
    last_ = last;
  }

  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::remove_cv_t<element_type>;
    using reference = element_type&;
    using pointer = std::add_pointer_t<element_type>;

    constexpr iterator() noexcept = default;

    constexpr iterator(std::uintptr_t cur, std::uintptr_t last) noexcept
        : cur_ {cur}, last_ {last} {
      skip_null();
    }

    reference operator*() const noexcept {
      return **reinterpret_cast<SlotType*>(cur_);
    }

    pointer operator->() const noexcept {
      return *reinterpret_cast<SlotType*>(cur_);
    }

    iterator& operator++() noexcept {
      cur_ += sizeof(SlotType);
      skip_null();
      return *this;
    }

    iterator operator++(int) noexcept {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    friend constexpr bool operator==(iterator a, iterator b) noexcept {
      return a.cur_ == b.cur_;
    }

  private:
    std::uintptr_t cur_ = 0;
    std::uintptr_t last_ = 0;

    void skip_null() noexcept {
      if constexpr(SizeCache != nullptr) {
        while(cur_ < last_ && *reinterpret_cast<SlotType*>(cur_) == nullptr)
          cur_ += sizeof(SlotType);
      }
    }
  };

  constexpr iterator begin() const noexcept {
    return {first_, last_};
  }

  constexpr iterator end() const noexcept {
    return {last_, last_};
  }

  constexpr std::size_t size() const noexcept {
    if constexpr(SizeCache != nullptr) {
      auto cached = SizeCache->load(std::memory_order_relaxed);
      if(cached != std::numeric_limits<std::size_t>::max())
        return cached;

      auto dist = std::ranges::distance(begin(), end());
      SizeCache->store(dist, std::memory_order_relaxed);
      return dist;
    } else {
      return (last_ - first_) / sizeof(SlotType);
    }
  }
};

} // namespace linker_set_detail

#define LINKER_SET_RANGE(tag)                                                  \
  (::linker_set_detail::range<LS_SLOT_TYPE(tag), LS_SIZE_CACHE(tag)>(          \
      reinterpret_cast<std::uintptr_t>(LS_BEGIN(tag)),                         \
      reinterpret_cast<std::uintptr_t>(LS_END(tag))))

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // LINKER_SET_HPP
