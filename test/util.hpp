#include <linker_set.hpp>

struct IndexEntry {
  int v;
  unsigned long long idx;
};

LINKER_SET_DECLARE(add_unique, int)
LINKER_SET_DECLARE(add_id, int)
LINKER_SET_DECLARE(add_member, int)
LINKER_SET_DECLARE(add_mixed, int)
LINKER_SET_DECLARE(add_idx, IndexEntry)


struct TagTypeA {
  int v = 1;
};
struct TagTypeB {
  int v = 2;
};
struct TagTypeC {
  int v = 3;
};

template <typename T>
struct AutoReg {
  static inline int t = T().v;
  LINKER_SET_ADD_MEMBER(add_member, t)
};

template <typename T>
struct MixedReg {
  static inline int t = T().v;
  LINKER_SET_ADD_MEMBER(add_mixed, t)

  static inline IndexEntry ie {
      .v = t,
      .idx = LINKER_SET_INDEX_MEMBER(add_mixed, t),
  };
  LINKER_SET_ADD_MEMBER(add_idx, ie)
};
