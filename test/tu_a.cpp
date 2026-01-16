#include <linker_set.hpp>

#include "util.hpp"

static int add_a = 1;
int add_c = 3;
LINKER_SET_ADD_UNIQUE(add_unique, add_a)
LINKER_SET_ADD_UNIQUE(add_unique, add_c)

LINKER_SET_ADD(add_id, add_a)
LINKER_SET_ADD(add_id, add_c)

AutoReg<TagTypeA> autoreg_a;
AutoReg<TagTypeC> autoreg_c_a;

MixedReg<TagTypeA> mixedreg_a;
LINKER_SET_ADD_UNIQUE(add_mixed, add_c)

int add_d = 4;
LINKER_SET_ADD(add_mixed, add_d)

IndexEntry idx_d {
    .v = add_d,
    .idx = LINKER_SET_INDEX(add_mixed, add_d),
};
LINKER_SET_ADD(add_idx, idx_d)
