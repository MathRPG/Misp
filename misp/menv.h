#ifndef MENV_H
#define MENV_H

#include "misp.h"

menv* menv_new(void);
menv* menv_copy(menv* e);
mval* menv_get(menv* e, mval* k);

void menv_put(menv* e, mval* k, mval* v);
void menv_def(menv* e, mval* k, mval* v);

void menv_add_builtins(menv* e);

void menv_del(menv* e);

#endif /* MISP_H */