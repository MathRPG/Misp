#pragma once

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

#define MASSERT(args, cond, fmt, ...) \
    if (!(cond)) {                    \
    mval* err = mval_err(fmt, ##__VA_ARGS__); \
    mval_del((args)); return err; }

#define MASSERT_TYPE(func, args, index, expect) \
    MASSERT(args, (args)->vals[index]->type == (expect), \
        "Function '%s' passed with incorrect type for argument %i.\n" \
        "Got %s, Expected %s.",                       \
        func, index, mtype_name((args)->vals[index]->type), mtype_name(expect))

#define MASSERT_NUM(func, args, num) \
    MASSERT(args, (args)->count == (num),   \
        "Function '%s' passed with incorrect number of arguments.\n" \
        "Got %i, Expected %i.",            \
        func, (args)->count, num)

#define MASSERT_NON_EMPTY(func, args, index) \
    MASSERT(args, (args)->vals[index]->count != 0, \
        "Function '%s' passed with {} for argument %i.", \
        func, index)