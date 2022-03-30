#ifndef MISP_H
#define MISP_H

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
} const EmptyMispList = {
	.count = 0, .values = NULL,
};

struct MispValue
{
	enum mval_type
	{
		MVAL_SEXPR,
		MVAL_ERROR,
		MVAL_SYMBOL,
		MVAL_INT,
	} type;

	union
	{
		mlist sexpr;
		char* error;
		char* symbol;
		long num;
	};
};

struct MispOper
{
	char* name;
	void (* apply)(mval*, mval*);
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-macro-parentheses"
#define MVAL_CTOR(name, type, arg, enum_, field_set) \
    mval* mval_ ## name (type arg) {             \
        mval* v = malloc(sizeof *v);        \
        *v = (mval) {(enum_), field_set,};   \
        return v;                             \
    }
#pragma clang diagnostic pop

MVAL_CTOR(sexpr, void, , MVAL_SEXPR, EmptyMispList)
MVAL_CTOR(error, char*, m, MVAL_ERROR, .error = strdup(m))
MVAL_CTOR(symbol, char*, s, MVAL_SYMBOL, .symbol = strdup(s))
MVAL_CTOR(int, long, x, MVAL_INT, x)

mval* mval_read(mpc_ast_t* t);

mval* mval_eval(mval* v);

void mval_print(mval*);
void mval_println(mval* v);

void mval_delete(mval* v);

#endif //MISP_H
