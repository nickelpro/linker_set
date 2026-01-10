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
// 3) Iterate with: LINKER_SET_SPAN(tag) -> std::span<T const* const>
//
// Build Options:
//   - LS_LINKER_SET_WRITABLE (default OFF) : Make linker sets writable for the
//   purpose of defeating variable ICF optimizations. Useful only for ensuring
//   unique ADD entries which point to identical objects.
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
// Define LS_LINKER_SET_WRITABLE to place the linker set in writable sections.
// This is useful if you want LINKER_SET_ADD_UNIQUE's "always-unique" slot
// semantics to survive toolchains that can package data into COMDATs and apply
// identical folding (e.g. MSVC /Gw /OPT:ICF, or LTO variable ICF).
//
// Note: You almost never want this on Apple, as no Mach-O backends support
// variable ICF to begin with.
//------------------------------------------------------------------------------
#if LS_LINKER_SET_WRITABLE
#define LS_MSVC_SEC_PERMS read, write
#define LS_APPLE_SEG "__DATA"
#define LS_ELF_SLOT_DECL constinit auto const*
#else
#define LS_MSVC_SEC_PERMS read
#define LS_APPLE_SEG "__DATA_CONST"
#define LS_ELF_SLOT_DECL constinit auto const* const
#endif

//------------------------------------------------------------------------------
// GCC binds the STB_GNU_UNIQUE flag to inline variable symbols and relies on
// COMDAT folding to prevent collisions. However, custom-named sections are
// never flagged for COMDAT folding under GCC, which causes symbol collisions.
// We need inline asm in order to define our own section groups and associated
// flags on GCC.
//
// clang uses weak symbols, no inline asm necessary.
//------------------------------------------------------------------------------
#if defined(__GNUC__)

#if __SIZEOF_POINTER__ == 8
#define LS_ASM_PTR_DIRECTIVE ".quad"
#else
#define LS_ASM_PTR_DIRECTIVE ".long"
#endif

#if LS_LINKER_SET_WRITABLE || defined(__PIC__)
#define LS_ASM_SECFLAGS "\"awRG\""
#else
#define LS_ASM_SECFLAGS "\"aRG\""
#endif

// Picking the right input constraint for symbols is a nearly impossible
// problem, see:
// https://maskray.me/blog/2024-01-30-raw-symbol-names-in-inline-assembly

#if __i386__ || __x86_64__
#define LS_ASM_INPUT_CONSTRAINT "Ws"
#elif __aarch64__
#define LS_ASM_INPUT_CONSTRAINT "S"
#elif __arm__
#define LS_ASM_INPUT_CONSTRAINT "US"
#else
#define LS_ASM_INPUT_CONSTRAINT "s"
#endif

#else
#define LS_ASM_PTR_DIRECTIVE
#define LS_ASM_SECFLAGS
#define LS_ASM_INPUT_CONSTRAINT
#endif

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
  ((void) *reinterpret_cast<unsigned char const volatile*>(sym), 0)
#define LS_MSVC_TOUCH_LOCAL(sym)                                               \
  namespace {                                                                  \
  int const LS_CAT2(ls_touch_u__, __COUNTER__) = LS_MSVC_TOUCH(&(sym));        \
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

//------------------------------------------------------------------------------
// DECLARE
// Intened to be used in a header; required to be visible for SPAN
//------------------------------------------------------------------------------
// Note: Despite what informal documentation might imply, neither link.exe nor
// lld-link will fold read-only data across different sections. The sentinels
// are always safe from ICF.
#if LS_PLATFORM_MSVC

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

#define LS_STATIC_CHECK(expr_lvalue)                                           \
  static_assert(std::is_lvalue_reference_v<decltype((expr_lvalue))>,           \
      "registry entry must be an lvalue expression");                          \
  static_assert(                                                               \
      std::is_object_v<std::remove_reference_t<decltype((expr_lvalue))>>,      \
      "registry entry must be an object (not a function)");

//------------------------------------------------------------------------------
// ADD_UNIQUE (non-coalescing; namespace-scope form)
//------------------------------------------------------------------------------
#if __INTELLISENSE__
// Always hide slots from intellisense, these declarations are never useful for
// completions
#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)
#elif LS_PLATFORM_MSVC

#define LS_MSVC_ADD_IMPL(tag, name, expr_lvalue)                               \
  namespace {                                                                  \
  LS_MSVC_USED                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, m))) constinit auto const* const name = \
      &(expr_lvalue);                                                          \
  LS_MSVC_TOUCH_LOCAL(name)                                                    \
  }

#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)                                \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  LS_MSVC_ADD_IMPL(tag, LS_CAT2(ls_u__, __COUNTER__), expr_lvalue)

#elif LS_PLATFORM_APPLE

#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)                                \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  namespace {                                                                  \
  [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]]                            \
  constinit auto const* const LS_CAT2(ls_u__, __COUNTER__) = &(expr_lvalue);   \
  }

#else // ELF

#define LINKER_SET_ADD_UNIQUE(tag, expr_lvalue)                                \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  namespace {                                                                  \
  [[gnu::used]] [[gnu::retain]] [[gnu::section("ls_" #tag)]]                   \
  LS_ELF_SLOT_DECL LS_CAT2(ls_u__, __COUNTER__) = &(expr_lvalue);              \
  }

#endif

//------------------------------------------------------------------------------
// ADD_ID (coalesced by symbol identity (tag,id); namespace-scope form)
//------------------------------------------------------------------------------
#if __INTELLISENSE__
#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)
#elif LS_PLATFORM_MSVC

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  LS_MSVC_USED                                                                 \
  __declspec(allocate(LS_MSVC_SEC(tag, m))) inline auto const* const LS_CAT4(  \
      ls_id_, tag, __, id) = &(expr_lvalue);                                   \
  LS_MSVC_TOUCH_LOCAL(LS_CAT4(ls_id_, tag, __, id))

#elif LS_PLATFORM_APPLE

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]]                            \
  inline constinit auto const* const LS_CAT4(ls_id_, tag, __, id) =            \
      &(expr_lvalue);

#else // ELF

#if defined(__clang__)

#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  [[gnu::used]] [[gnu::retain]] [[gnu::section("ls_" #tag)]]                   \
  inline LS_ELF_SLOT_DECL LS_CAT4(ls_id_, tag, __, id) = &(expr_lvalue);


#else // GCC
// Note: GCC 15 implements top-level extended asm, but not with LTO streaming;
// for this to work under -flto we need to wrap it in a function

// clang-format off
#define LINKER_SET_ADD_ID(tag, id, expr_lvalue)                                \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  [[gnu::used]]                                                                \
  inline void LS_CAT5(ls_id_, tag, __, id, _f) () {                            \
    __asm__ volatile (                                                         \
      ".pushsection ls_" #tag "," LS_ASM_SECFLAGS ",@progbits,"                \
      "ls_id_" #tag "__" #id ",comdat\n"                                       \
      ".balign " LS_STR(__SIZEOF_POINTER__) "\n"                               \
      "  " LS_ASM_PTR_DIRECTIVE " %c0\n"                                       \
      ".popsection\n"                                                          \
      :: LS_ASM_INPUT_CONSTRAINT (&(expr_lvalue))                              \
    );                                                                         \
  }
// clang-format on

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
#if __INTELLISENSE__
#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)
#elif LS_PLATFORM_MSVC

#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  __pragma(section(LS_MSVC_SEC(tag, m), LS_MSVC_SEC_PERMS));                   \
  LS_MSVC_USED                                                                 \
  static auto LS_CAT5(ls_id_, tag, __, id, _f)() {                             \
    LS_MSVC_USED __declspec(allocate(LS_MSVC_SEC(tag,                          \
        m))) static constinit auto const* const ls_internal__ptr =             \
        &(expr_lvalue);                                                        \
    return &ls_internal__ptr;                                                  \
  }                                                                            \
  LS_MSVC_TOUCH_INTERNAL_PTR(tag, id)

#elif LS_PLATFORM_APPLE

// Bafflingly, [[gnu::used]] is not enough to force instantiation on AppleClang,
// so we pull the ol' static_assert trick; which is enough for some reason.
#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  [[gnu::used]]                                                                \
  static void LS_CAT5(ls_id_, tag, __, id, _f)() {                             \
    [[gnu::used]] [[gnu::section(LS_APPLE_SEC(tag))]]                          \
    static constinit auto const* const ls_internal__ptr = &(expr_lvalue);      \
  }                                                                            \
  static_assert(LS_CAT5(ls_id_, tag, __, id, _f));

#else // ELF

#if defined(__clang__)

#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  [[gnu::used]]                                                                \
  static void LS_CAT5(ls_id_, tag, __, id, _f)() {                             \
    [[gnu::used]] [[gnu::retain]] [[gnu::section("ls_" #tag)]]                 \
    static LS_ELF_SLOT_DECL ls_internal__ptr = &(expr_lvalue);                 \
  }

#else // GCC

// clang-format off
#define LINKER_SET_ADD_MEMBER_ID(tag, id, expr_lvalue)                         \
  LS_STATIC_CHECK(expr_lvalue)                                                 \
  [[gnu::used]]                                                                \
  static void LS_CAT5(ls_id_, tag, __, id, _f)() {                             \
    __asm__ __volatile__(                                                      \
      ".pushsection ls_" #tag "," LS_ASM_SECFLAGS ",@progbits,%c0,comdat\n"    \
      ".balign " LS_STR(__SIZEOF_POINTER__) "\n"                               \
      "  " LS_ASM_PTR_DIRECTIVE " %c1\n"                                       \
      ".popsection\n"                                                          \
      :: LS_ASM_INPUT_CONSTRAINT (LS_CAT5(ls_id_, tag, __, id, _f)),           \
         LS_ASM_INPUT_CONSTRAINT (&(expr_lvalue))                              \
    );                                                                         \
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

#endif // LINKER_SET_HPP
