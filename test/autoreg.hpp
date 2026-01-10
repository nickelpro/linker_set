#include <linker_set.hpp>

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
};
