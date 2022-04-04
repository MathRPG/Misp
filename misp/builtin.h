#pragma once

#define BUILTIN_CMP(F) \
    F(gt, >)                  \
    F(lt, <)                  \
    F(ge, >=)                 \
    F(le, <=)

mval* builtin_def(menv* e, mval* a);
mval* builtin_put(menv* e, mval* a);
mval* builtin_lambda(menv* e, mval* a);
mval* builtin_if(menv* e, mval* a);
mval* builtin_print(__attribute__((unused)) menv* e, mval* a);
mval* builtin_error(__attribute__((unused)) menv* e, mval* a);
mval* builtin_load(menv* e, mval* a);

mval* builtin_list(menv* e, mval* a);
mval* builtin_head(menv* e, mval* a);
mval* builtin_tail(menv* e, mval* a);
mval* builtin_eval(menv* e, mval* a);
mval* builtin_join(menv* e, mval* a);

mval* builtin_add(menv* e, mval* a);
mval* builtin_sub(menv* e, mval* a);
mval* builtin_mul(menv* e, mval* a);
mval* builtin_div(menv* e, mval* a);

#define FUNC_HEADER(name, symb) \
    mval* builtin_##name(menv* e, mval* a);

BUILTIN_CMP(FUNC_HEADER)

mval* builtin_eq(menv* e, mval* a);
mval* builtin_ne(menv* e, mval* a);