#ifndef MISP_H
#define MISP_H

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

mval* mval_sexpr(void);
mval* mval_func(mbuiltin f);
mval* mval_lambda(mval* args, mval* body);
mval* mval_err(const char* fmt, ...);
mval* mval_sym(char* s);
mval* mval_num(long x);

const char* mtype_name(enum mval_type t);

mval* mval_copy(mval* v);
mval* mval_take(mval* v, int i);
mval* mval_pop(mval* v, int i);
mval* mval_join(mval* x, mval* y);
int mval_eq(mval* x, mval* y);

mval* mval_read(mpc_ast_t* t);
mval* mval_eval(menv* e, mval* v);

void mval_print(mval*);
void mval_println(mval* v);

void mval_del(mval* v);

void load_file(menv* e, const char* filename);

#endif /* MISP_H */