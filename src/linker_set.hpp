#ifndef LINKER_SET_HPP
#define LINKER_SET_HPP

// linker_set.hpp
//
// 1) Declare a linker set with LINKER_SET_DECLARE(tag, T).
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
// 3) Optionally, retrieve the index inside the span with:
//    - LINKER_SET_INDEX(tag, id)        : namespace scope
//    - LINKER_SET_INDEX_MEMBER(tag, id) : class scope
//    Must be called in the same scope as the LINKER_SET_ADD*. Not compatible
//    with LINKER_SET_ADD_UNIQUE.
// 4) Iterate with: LINKER_SET_SPAN(tag) -> std::span<T const* const>
//
// Build Options:
//   - LS_LINKER_SET_WRITABLE (default OFF) : Make linker sets writable for the
//   purpose of defeating constant folding and variable ICF optimizations.
//   Rarely necessary, only GCC with -fmerge-all-constants has been shown to
//   require it.
//
// Notes:
// - No metadata is stored; the identifier exists only for linker coalescing.
// - Set may contain holes on MSVC, especially with incremental linking. Users
//   of such platforms must check for null elements.
// - ELF uses [[gnu::retain]] to survive --gc-sections.
// - Windows:
//     * clang-cl: uses [[gnu::used]] on COFF to keep entries.
//     * cl.exe: best-effort "touch" in CRT$XCU to resist /Zc:inline and
//     /OPT:REF.

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

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
// Define LS_LINKER_SET_WRITABLE to defeat constant folding/variable ICF and
// ensure linker sets are placed in writable sections.
//
// Constant folding and variable ICF can be tricky under two cases:
//   * The direct case: two LINKER_SET_ADD_UNIQUE entries point to the same
//     object, which makes them direct candidates for constant folding.
//
//   * The indirect case: two constant objects with individual entries in the
//     linker set are merged, becoming identity-identical. The linker set
//     entries for these objects are now candidates for ICF themselves.
//
// Testing has only shown the former to be possible under GCC with
// -fmerge-all-constants. No tested linker will perform the latter under any
// optimization conditions.
//------------------------------------------------------------------------------
#if LS_LINKER_SET_WRITABLE
#define LS_MSVC_SEC_PERMS read, write
#define LS_APPLE_SEG "__DATA"
#define LS_ELF_CONST
#else
#define LS_MSVC_SEC_PERMS read
#define LS_APPLE_SEG "__DATA_CONST"
#define LS_ELF_CONST const
#endif

#define LS_ELF_SLOT_DECL constinit auto const* LS_ELF_CONST

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
#define LS_ELF_GCC_COMMENT "@"
#define LS_ELF_GCC_INPUT_CONSTRAINT "US"

#else

#define LS_ELF_GCC_PROGBITS "@progbits"
#define LS_ELF_GCC_OBJECT "@object"

#if defined(__aarch64__)
#define LS_ELF_GCC_COMMENT "//"
#define LS_ELF_GCC_INPUT_CONSTRAINT "S"

#else
#define LS_ELF_GCC_COMMENT "#"

#if defined(__i386__) || defined(__x86_64__)
#define LS_ELF_GCC_INPUT_CONSTRAINT "Ws"
#else
#define LS_ELF_GCC_INPUT_CONSTRAINT "s"
#endif

#endif

#endif

//------------------------------------------------------------------------------
// GCC is a real pain in the ass about section flags matching on non-LTO builds.
// This creates a problem for linker sets which mix unique (ie ADD_UNIQUE) and
// non-unique entries inside the same TU.
//
// We can solve this by hijacking the [[gnu::section]] attribute and injecting
// our own section flags. Once hijacked, we can use the GCC assembler extension
// "unique,<N>" to break out of the flag matching behavior.
//
// However, different GCC assembler backends have different syntax to perform
// this hijacking, because nothing in life can be easy.
//
// Clang doesn't care about any of this and does the right thing.
//------------------------------------------------------------------------------
#if defined(__GNUC__) && !defined(__clang__)

#define LS_ELF_UNIQUE_SEC(tag, n)                                              \
  "ls_" #tag ",\"" LS_ELF_GCC_ASM_FLAGS "\"," LS_ELF_GCC_PROGBITS              \
  ",unique," LS_STR(n) " " LS_ELF_GCC_COMMENT

#else
#define LS_ELF_UNIQUE_SEC(tag, n) "ls_" #tag
#endif

//------------------------------------------------------------------------------
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
// Once again, clang simply does the right thing.
//------------------------------------------------------------------------------

// clang-format off
#define LS_ELF_GCC_ASM(tag, id, expr_lvalue)                                   \
  std::remove_cvref_t<decltype(expr_lvalue)> const * LS_ELF_CONST              \
      LS_CAT4(ls_id_, tag, __, id);                                            \
  [[gnu::used]]                                                                \
  inline void LS_CAT5(ls_id_, tag, __, id, _emit)() {                          \
    __asm__ __volatile__(                                                      \
      ".pushsection ls_" #tag ",\"" LS_ELF_GCC_ASM_FLAGS "G\""                 \
      "," LS_ELF_GCC_PROGBITS ",%c0,comdat\n"                                  \
      ".balign " LS_STR(__SIZEOF_POINTER__) "\n"                               \
      ".globl %c0\n"                                                           \
      ".type %c0," LS_ELF_GCC_OBJECT "\n"                                      \
      ".size %c0," LS_STR(__SIZEOF_POINTER__) "\n"                             \
      "%c0:\n"                                                                 \
      "  .dc.a %c1\n"                                                          \
      ".popsection\n"                                                          \
      :: LS_ELF_GCC_INPUT_CONSTRAINT (&(LS_CAT4(ls_id_, tag, __, id))),        \
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
// lld-link will fold read-only data across different sections. The sentinels
// are always safe from ICF.

#define LINKER_SET_DECLARE(tag, T)                                             \
  __pragma(section(LS_MSVC_SEC(tag, a), LS_MSVC_SEC_PERMS));                   \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  __pragma(section(LS_MSVC_SEC(tag, z), LS_MSVC_SEC_PERMS));                   \
  extern "C" {                                                                 \
  __declspec(allocate(                                                         \
      LS_MSVC_SEC(tag, a))) inline T const* const ls_begin_##tag = nullptr;    \
  __declspec(allocate(                                                         \
      LS_MSVC_SEC(tag, z))) inline T const* const ls_end_##tag = nullptr;      \
  }

#elif LS_PLATFORM_APPLE

#define LINKER_SET_DECLARE(tag, T)                                             \
  extern "C" {                                                                 \
  [[gnu::weak]]                                                                \
  extern T const* const ls_start_##tag[] __asm(LS_APPLE_STARTSYM(tag));        \
  [[gnu::weak]]                                                                \
  extern T const* const ls_end_##tag[] __asm(LS_APPLE_ENDSYM(tag));            \
  }

#else // ELF

#define LINKER_SET_DECLARE(tag, T)                                             \
  extern "C" {                                                                 \
  [[gnu::weak]]                                                                \
  extern T const* const __start_ls_##tag[];                                    \
  [[gnu::weak]]                                                                \
  extern T const* const __stop_ls_##tag[];                                     \
  }

#endif

#if LS_PLATFORM_MSVC
#define LS_SET_SLOT_PTR_TYPE(tag) std::remove_cvref_t<decltype(ls_begin_##tag)>
#elif LS_PLATFORM_APPLE
#define LS_SET_SLOT_PTR_TYPE(tag)                                              \
  std::remove_cvref_t<std::remove_extent_t<decltype(ls_start_##tag)>>
#else // ELF
#define LS_SET_SLOT_PTR_TYPE(tag)                                              \
  std::remove_cvref_t<std::remove_extent_t<decltype(__start_ls_##tag)>>
#endif

#define LS_SET_OBJECT_TYPE(tag)                                                \
  std::remove_cv_t<std::remove_pointer_t<LS_SET_SLOT_PTR_TYPE(tag)>>

#define LS_EXPR_OBJECT_TYPE(expr_lvalue)                                       \
  std::remove_cvref_t<decltype((expr_lvalue))>

#define LS_STATIC_CHECK(tag, expr_lvalue)                                      \
  static_assert(std::is_lvalue_reference_v<decltype((expr_lvalue))>,           \
      "registry entry must be an lvalue expression");                          \
  static_assert(std::is_same_v<LS_EXPR_OBJECT_TYPE(expr_lvalue),               \
                    LS_SET_OBJECT_TYPE(tag)>,                                  \
      "registry entry type must exactly match LINKER_SET_DECLARE(tag, T)");

//------------------------------------------------------------------------------
// ADD_UNIQUE (non-coalescing; namespace-scope form)
//------------------------------------------------------------------------------
#if LS_PLATFORM_MSVC

#define LS_MSVC_ADD_IMPL(tag, name, expr_lvalue)                               \
  namespace {                                                                  \
  LS_MSVC_USED                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, m))) constinit auto const* const name = \
      &(expr_lvalue);                                                          \
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
  [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]]                            \
  constinit auto const* const LS_CAT2(ls_u__, __COUNTER__) = &(expr_lvalue);   \
  }

#else // ELF

#define LINKER_SET_ADD_UNIQUE_(tag, expr_lvalue, n)                            \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  namespace {                                                                  \
  [[gnu::used]] [[gnu::retain]] [[gnu::section(LS_ELF_UNIQUE_SEC(tag, n))]]    \
  LS_ELF_SLOT_DECL LS_CAT2(ls_u__, n) = &(expr_lvalue);                        \
  }

#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)                                \
  LINKER_SET_ADD_UNIQUE_(tag, expr_lvalue, __COUNTER__)

#endif

//------------------------------------------------------------------------------
// ADD_ID (coalesced by symbol identity (tag,id); namespace-scope form)
//------------------------------------------------------------------------------
#if LS_PLATFORM_MSVC

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  LS_MSVC_USED                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, m))) inline auto const* const LS_CAT4(  \
      ls_id_, tag, __, id) = &(expr_lvalue);                                   \
  LS_MSVC_TOUCH_LOCAL(LS_CAT4(ls_id_, tag, __, id))

#elif LS_PLATFORM_APPLE

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]]                            \
  inline constinit auto const* const LS_CAT4(ls_id_, tag, __, id) =            \
      &(expr_lvalue);

#else // ELF

#if defined(__clang__)

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  [[gnu::used]] [[gnu::retain]] [[gnu::section("ls_" #tag)]]                   \
  inline LS_ELF_SLOT_DECL LS_CAT4(ls_id_, tag, __, id) = &(expr_lvalue);

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
    LS_MSVC_USED __declspec(allocate(LS_MSVC_SEC(tag,                          \
        m))) static constinit auto const* const ls_internal__ptr =             \
        &(expr_lvalue);                                                        \
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
    [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]]                          \
    static constinit auto const* const ls_internal__ptr = &(expr_lvalue);      \
    return ls_internal__ptr;                                                   \
  }                                                                            \
  static_assert(LS_CAT5(ls_id_, tag, __, id, _f));

#else // ELF

#if defined(__clang__)

#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(tag, expr_lvalue)                                            \
  [[gnu::used]]                                                                \
  static auto& LS_CAT5(ls_id_, tag, __, id, _f)() {                            \
    [[gnu::used]] [[gnu::retain]] [[gnu::section("ls_" #tag)]]                 \
    static LS_ELF_SLOT_DECL ls_internal__ptr = &(expr_lvalue);                 \
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
//------------------------------------------------------------------------------
#if LS_PLATFORM_MSVC

#define LINKER_SET_SPAN(tag)                                                   \
  ([]() noexcept -> std::span<decltype(ls_begin_##tag)> {                      \
    auto b = &ls_begin_##tag + 1;                                              \
    auto e = &ls_end_##tag;                                                    \
    if(e < b)                                                                  \
      return {};                                                               \
    return {b, static_cast<std::size_t>(e - b)};                               \
  }())

#elif LS_PLATFORM_APPLE

#define LINKER_SET_SPAN(tag)                                                   \
  ([]() noexcept                                                               \
          -> std::span<std::remove_extent_t<decltype(ls_start_##tag)>> {       \
    auto b = ls_start_##tag;                                                   \
    auto e = ls_end_##tag;                                                     \
    if(!b || !e || e < b)                                                      \
      return {};                                                               \
    return {b, static_cast<std::size_t>(e - b)};                               \
  }())

#else // ELF

#define LINKER_SET_SPAN(tag)                                                   \
  ([]() noexcept                                                               \
          -> std::span<std::remove_extent_t<decltype(__start_ls_##tag)>> {     \
    auto b = __start_ls_##tag;                                                 \
    auto e = __stop_ls_##tag;                                                  \
    if(!b || !e || e < b)                                                      \
      return {};                                                               \
    return {b, static_cast<std::size_t>(e - b)};                               \
  }())

#endif

//------------------------------------------------------------------------------
// INDEX
//------------------------------------------------------------------------------

#if LS_PLATFORM_MSVC

#define LINKER_SET_INDEXER(tag)                                                \
  ([](decltype(ls_begin_##tag)& entry) noexcept -> std::size_t {               \
    auto b = reinterpret_cast<std::uintptr_t>(&ls_begin_##tag + 1);            \
    auto e = reinterpret_cast<std::uintptr_t>(&entry);                         \
    auto diff = e - b;                                                         \
    return static_cast<std::size_t>(diff / sizeof(entry));                     \
  })

#elif LS_PLATFORM_APPLE

#define LINKER_SET_INDEXER(tag)                                                \
  ([](std::remove_extent_t<decltype(ls_start_##tag)>& entry) noexcept          \
          -> std::size_t {                                                     \
    auto b = reinterpret_cast<std::uintptr_t>(ls_start_##tag);                 \
    auto e = reinterpret_cast<std::uintptr_t>(&entry);                         \
    auto diff = e - b;                                                         \
    return static_cast<std::size_t>(diff / sizeof(entry));                     \
  })

#else // ELF

#define LINKER_SET_INDEXER(tag)                                                \
  ([](std::remove_extent_t<decltype(__start_ls_##tag)>& entry) noexcept        \
          -> std::size_t {                                                     \
    auto b = reinterpret_cast<std::uintptr_t>(__start_ls_##tag);               \
    auto e = reinterpret_cast<std::uintptr_t>(&entry);                         \
    auto diff = e - b;                                                         \
    return static_cast<std::size_t>(diff / sizeof(entry));                     \
  })

#endif

#define LINKER_SET_INDEX(tag, id)                                              \
  LINKER_SET_INDEXER(tag)(LS_CAT4(ls_id_, tag, __, id))

#define LINKER_SET_INDEX_MEMBER(tag, id)                                       \
  LINKER_SET_INDEXER(tag)(LS_CAT5(ls_id_, tag, __, id, _f)())

#endif // LINKER_SET_HPP
