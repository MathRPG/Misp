#pragma once

#include <stdlib.h>
#include <string.h>

#include "../mpc/mpc.h"

typedef struct MispValue mval;
typedef struct MispEnv menv;

typedef mval* (* mbuiltin)(menv*, mval*);

struct MispValue
{
	enum mval_type
	{
		MVAL_SEXPR,
		MVAL_QEXPR,
		MVAL_FUNC,
		MVAL_ERROR,
		MVAL_SYMBOL,
		MVAL_STRING,
		MVAL_NUM,
	} type;

	union
	{
		struct
		{
			int count;
			mval** vals;
		};

		struct
		{
			mbuiltin func;
			menv* env;
			mval* args;
			mval* body;
		};

		char* err;
		char* sym;
		char* str;
		long num;
	};
};

struct MispEnv
{
	menv* par;
	int count;
	char** syms;
	mval** vals;
};

menv* menv_new(void);
void menv_del(menv* e);

void menv_add_builtins(menv* e);

mval* mval_read(mpc_ast_t* t);

mval* mval_eval(menv* e, mval* v);

void mval_print(mval*);
void mval_println(mval* v);

void mval_del(mval* v);