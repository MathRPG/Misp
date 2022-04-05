#include "menv.h"
#include "macros.h"
#include "builtin.h"

menv* menv_new(void)
{
	menv* e = malloc(sizeof *e);
	*e = (menv){
		.par = NULL,
		.count = 0,
		.syms = NULL,
		.vals = NULL,
	};
	return e;
}

void menv_del(menv* e)
{
	for_range(i, 0, e->count)
	{
		free(e->syms[i]);
		mval_del(e->vals[i]);
	}

	free(e->syms);
	free(e->vals);
	free(e);
}

menv* menv_copy(menv* e)
{
	menv* n = malloc(sizeof *n);
	*n = (menv){
		.par = e->par,
		.count = e->count,
		.syms = malloc(sizeof(char*) * e->count),
		.vals = malloc(sizeof(mval*) * e->count),
	};

	for_range(i, 0, e->count)
	{
		n->syms[i] = strdup(e->syms[i]);
		n->vals[i] = mval_copy(e->vals[i]);
	}

	return n;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
mval* menv_get(menv* e, mval* k)
{
	for_range(i, 0, e->count)
	{
		if (strcmp(e->syms[i], k->sym) == 0)
			return mval_copy(e->vals[i]);
	}

	if (e->par != NULL)
		return menv_get(e->par, k);

	return mval_err("Unbound Symbol '%s'", k->sym);
}
#pragma clang diagnostic pop

void menv_put(menv* e, mval* k, mval* v)
{
	for_range(i, 0, e->count)
	{
		if (strcmp(e->syms[i], k->sym) == 0)
		{
			mval_del(e->vals[i]);
			e->vals[i] = mval_copy(v);
			return;
		}
	}

	e->count++;

	e->vals = realloc(e->vals, sizeof(mval*) * e->count);
	e->vals[e->count - 1] = mval_copy(v);

	e->syms = realloc(e->syms, sizeof(char*) * e->count);
	e->syms[e->count - 1] = strdup(k->sym);
}

void menv_def(menv* e, mval* k, mval* v)
{
	while (e->par != NULL)
		e = e->par;
	menv_put(e, k, v);
}

void menv_add_builtin(menv* e, char* name, mbuiltin func)
{
	mval* k = mval_sym(name);
	mval* v = mval_func(func);
	menv_put(e, k, v);
	mval_del(k);
	mval_del(v);
}

void menv_add_builtins(menv* e)
{
	struct
	{
		char* name;
		mbuiltin func;
	} const static builtins[] = {
		{ "def", builtin_def, },
		{ "=", builtin_put, },
		{ "\\", builtin_lambda, },
		{ "if", builtin_if, },
		{ "print", builtin_print, },
		{ "error", builtin_error, },
		{ "load", builtin_load, },
		// List Functions
		{ "list", builtin_list, },
		{ "head", builtin_head, },
		{ "tail", builtin_tail, },
		{ "eval", builtin_eval, },
		{ "join", builtin_join, },
		// Math Functions
		{ "+", builtin_add, },
		{ "-", builtin_sub, },
		{ "*", builtin_mul, },
		{ "/", builtin_div, },
		// Comparison Functions
		{ ">", builtin_gt, },
		{ "<", builtin_lt, },
		{ ">=", builtin_ge, },
		{ "<=", builtin_le, },
		{ "==", builtin_eq, },
		{ "!=", builtin_ne, },
	};

	for_range(i, 0, len(builtins))
	{
		menv_add_builtin(e, builtins[i].name, builtins[i].func);
	}
}