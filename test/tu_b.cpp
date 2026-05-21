#include <linker_set.hpp>

#include "util.hpp"

static int add_b = 2;
static int mutable_b = 20;
extern int add_c;
LINKER_SET_ADD_UNIQUE(add_unique, add_b)
LINKER_SET_ADD_UNIQUE(add_unique, add_c)

LINKER_SET_ADD(add_id, add_b)
LINKER_SET_ADD(add_id, add_c)
LINKER_SET_ADD(add_mutable, mutable_b)

IndexEntry mutable_idx_b {
    .v = mutable_b,
    .idx = LINKER_SET_INDEX(add_mutable, mutable_b),
};
LINKER_SET_ADD(add_mut_idx, mutable_idx_b)

AutoReg<TagTypeB> autoreg_b;
AutoReg<TagTypeC> autoreg_c_b;

LINKER_SET_ADD(add_mixed, add_b)

IndexEntry idx_b {
    .v = add_b,
    .idx = LINKER_SET_INDEX(add_mixed, add_b),
};
LINKER_SET_ADD(add_idx, idx_b)
