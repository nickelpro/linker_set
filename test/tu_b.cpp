#include <linker_set.hpp>

#include "autoreg.hpp"

static int add_b = 2;
extern int add_c;
LINKER_SET_ADD_UNIQUE(add_unique, add_b)
LINKER_SET_ADD_UNIQUE(add_unique, add_c)

LINKER_SET_ADD(add_id, add_b)
LINKER_SET_ADD(add_id, add_c)

AutoReg<TagTypeB> autoreg_b;
AutoReg<TagTypeC> autoreg_c_b;

LINKER_SET_ADD(add_mixed, add_b);
