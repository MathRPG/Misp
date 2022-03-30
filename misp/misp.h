#pragma once

#include <stdlib.h>
#include <string.h>

#include "../mpc/mpc.h"

typedef struct MispValue mval;
typedef struct MispList mlist;
typedef struct MispOper moper;

struct MispList
{
	int count;
	mval** values;
} extern const EmptyMispList;

struct MispValue
{
	enum mval_type
	{
		MVAL_SEXPR,
		MVAL_QEXPR,
		MVAL_ERROR,
		MVAL_SYMBOL,
		MVAL_INT,
	} type;

	union
	{
		mlist exprs;
		char* error;
		char* symbol;
		long num;
	};
};

struct MispOper
{
	char* name;
	enum
	{
		MOPER_UNARY, MOPER_BINARY
	} type;

	union
	{
		mval* (* apply_unary)(mval*);
		mval* (* apply_binary)(mval*, mval*);
	};
};

#define MVAL_CTOR(name, type, arg, enum_, field_set) mval* mval_##name(type arg);

#define MVAL_CTOR_LIST\
    MVAL_CTOR(sexpr, void, , MVAL_SEXPR, EmptyMispList)\
    MVAL_CTOR(qexpr, void, , MVAL_QEXPR, EmptyMispList)\
    MVAL_CTOR(error, char*, m, MVAL_ERROR, .error = strdup(m))\
    MVAL_CTOR(symbol, char*, s, MVAL_SYMBOL, .symbol = strdup(s))\
    MVAL_CTOR(int, long, x, MVAL_INT, x)

MVAL_CTOR_LIST

#undef MVAL_CTOR

mval* mval_read(mpc_ast_t* t);

mval* mval_eval(mval* v);

void mval_print(mval*);
void mval_println(mval* v);

void mval_delete(mval* v);