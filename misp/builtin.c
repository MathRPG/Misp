#include <stdio.h>
#include <assert.h>

#include "misp.h"
#include "parsing.h"
#include "builtin.h"

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

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

#define CMP_FUNC(name, symb) \
    mval* builtin_ ## name (menv* e, mval* a) { return builtin_ord(e, a, #symb); }

mval* builtin_list(__attribute__((unused)) menv* e, mval* a)
{
	a->type = MVAL_QEXPR;
	return a;
}

mval* builtin_head(__attribute__((unused)) menv* e, mval* a)
{
	MASSERT_NUM("head", a, 1);
	MASSERT_TYPE("head", a, 0, MVAL_QEXPR);
	MASSERT_NON_EMPTY("head", a, 0);

	mval* v = mval_take(a, 0);

	while (v->count > 1)
	{
		mval_del(mval_pop(v, 1));
	}

	return v;
}

mval* builtin_tail(__attribute__((unused)) menv* e, mval* a)
{
	MASSERT_NUM("tail", a, 1);
	MASSERT_TYPE("tail", a, 0, MVAL_QEXPR);
	MASSERT_NON_EMPTY("tail", a, 0);

	mval* v = mval_take(a, 0);
	mval_del(mval_pop(v, 0));
	return v;
}

mval* builtin_eval(menv* e, mval* a)
{
	MASSERT_NUM("eval", a, 1);
	MASSERT_TYPE("eval", a, 0, MVAL_QEXPR);

	mval* x = mval_take(a, 0);
	x->type = MVAL_SEXPR;
	return mval_eval(e, x);
}

mval* builtin_join(__attribute__((unused)) menv* e, mval* a)
{
	for_range(i, 0, a->count)
	{
		MASSERT_TYPE("join", a, i, MVAL_QEXPR);
	}

	mval* x = mval_pop(a, 0);

	while (a->count > 0)
	{
		mval* y = mval_pop(a, 0);
		x = mval_join(x, y);
	}

	mval_del(a);
	return x;
}

mval* builtin_math_op(__attribute__((unused)) menv* e, mval* a, char* symbol)
{
	for_range(i, 0, a->count)
	{
		MASSERT_TYPE(symbol, a, i, MVAL_NUM);
	}

	mval* x = mval_pop(a, 0);

	if (a->count == 0 && strcmp(symbol, "-") == 0)
	{
		x->num = -x->num;
	}

#define OP(symb, body) \
    if(strcmp(symbol, #symb) == 0) { \
        if (0) {} else { body }   \
        x->num symb##= y->num;    \
    } 0

	while (a->count > 0)
	{
		mval* y = mval_pop(a, 0);

		OP(+, ;);
		OP(-, ;);
		OP(*, ;);
		OP(/, {
			if (y->num == 0)
			{
				mval_del(x);
				mval_del(y);
				x = mval_err("Zero Division Error");
				break;
			}
		});

		mval_del(y);
	}

	mval_del(a);
	return x;
}

mval* builtin_add(menv* e, mval* a)
{
	return builtin_math_op(e, a, "+");
}

mval* builtin_sub(menv* e, mval* a)
{
	return builtin_math_op(e, a, "-");
}

mval* builtin_mul(menv* e, mval* a)
{
	return builtin_math_op(e, a, "*");
}

mval* builtin_div(menv* e, mval* a)
{
	return builtin_math_op(e, a, "/");
}

mval* builtin_ord(__attribute__((unused)) menv* e, mval* a, const char* op)
{
	MASSERT_NUM(op, a, 2);
	MASSERT_TYPE(op, a, 0, MVAL_NUM);
	MASSERT_TYPE(op, a, 1, MVAL_NUM);

	long x = a->vals[0]->num, y = a->vals[1]->num;

	mval_del(a);

#define CMP_OPER(name, symb) \
    if (strcmp(op, #symb) == 0) {\
        return mval_num(x symb y); \
    }

	BUILTIN_CMP(CMP_OPER);

	return mval_err("Invalid Comparison Symbol '%s'", op);
}

BUILTIN_CMP(CMP_FUNC)

mval* builtin_cmp(__attribute__((unused)) menv* e, mval* a, const char* op)
{
	MASSERT_NUM(op, a, 2);

	mval* x = a->vals[0], * y = a->vals[1];
	int r = mval_eq(x, y);

	if (strcmp(op, "!=") == 0)
		r = !r;

	mval_del(a);
	return mval_num(r);
}

mval* builtin_eq(menv* e, mval* a)
{
	return builtin_cmp(e, a, "==");
}

mval* builtin_ne(menv* e, mval* a)
{
	return builtin_cmp(e, a, "!=");
}

static typeof(menv_def)* get_menv_func(const char* func_name)
{
	struct
	{
		const char* name;
		typeof(menv_def)* func;
	} scope_funcs[] = {
		{ "def", menv_def, },
		{ "=", menv_put, },
	};

	for_range(i, 0, len(scope_funcs))
	{
		if (strcmp(scope_funcs[i].name, func_name) == 0)
			return scope_funcs[i].func;
	}

	return NULL;
}

mval* builtin_var(menv* e, mval* a, char* func_name)
{
	MASSERT_TYPE(func_name, a, 0, MVAL_QEXPR);

	mval* syms = a->vals[0];

	for_range(i, 0, syms->count)
	{
		MASSERT(a, (syms->vals[i]->type == MVAL_SYMBOL),
			"Function '%s' cannot define non-symbol.\n"
			"Got %s, Expected %s.",
			func_name,
			mtype_name(syms->vals[i]->type),
			mtype_name(MVAL_SYMBOL));
	}

	MASSERT(a, (syms->count == a->count - 1),
		"Function '%s' passed with incorrect number of arguments.\n"
		"Got %i, Expected %i.",
		func_name, syms->count, a->count - 1);

	typeof(menv_def)* menv_func = get_menv_func(func_name);
	assert(menv_func);

	for_range(i, 0, syms->count)
	{
		menv_func(e, syms->vals[i], a->vals[i + 1]);
	}

	mval_del(a);
	return mval_sexpr();
}

mval* builtin_def(menv* e, mval* a)
{
	return builtin_var(e, a, "def");
}

mval* builtin_put(menv* e, mval* a)
{
	return builtin_var(e, a, "=");
}

mval* builtin_lambda(__attribute__((unused)) menv* e, mval* a)
{
	MASSERT_NUM("\\", a, 2);
	MASSERT_TYPE("\\", a, 0, MVAL_QEXPR);
	MASSERT_TYPE("\\", a, 1, MVAL_QEXPR);

	for_range(i, 0, a->vals[0]->count)
	{
		enum mval_type type = a->vals[0]->vals[i]->type;
		MASSERT(a, (type == MVAL_SYMBOL),
			"Cannot define non-symbol.\nGot %s, Expected %s.",
			mtype_name(type),
			mtype_name(MVAL_SYMBOL));
	}

	mval* args = mval_pop(a, 0);
	mval* body = mval_pop(a, 0);
	mval_del(a);

	return mval_lambda(args, body);
}

mval* builtin_if(menv* e, mval* a)
{
	MASSERT_NUM("if", a, 3);
	MASSERT_TYPE("if", a, 0, MVAL_NUM);
	MASSERT_TYPE("if", a, 1, MVAL_QEXPR);
	MASSERT_TYPE("if", a, 2, MVAL_QEXPR);

	a->vals[1]->type = MVAL_SEXPR;
	a->vals[2]->type = MVAL_SEXPR;

	long r = a->vals[0]->num;
	mval* x = mval_eval(e, mval_pop(a, r ? 1 : 2));

	mval_del(a);
	return x;
}

mval* builtin_print(__attribute__((unused)) menv* e, mval* a)
{
	for_range(i, 0, a->count)
	{
		mval_print(a->vals[i]);

		if (i != a->count - 1)
			putchar(' ');
	}

	putchar('\n');
	mval_del(a);

	return mval_sexpr();
}

mval* builtin_error(__attribute__((unused)) menv* e, mval* a)
{
	MASSERT_NUM("error", a, 1);
	MASSERT_TYPE("error", a, 0, MVAL_STRING);

	mval* err = mval_err("%s", a->vals[0]->str);

	mval_del(a);
	return err;
}

mval* builtin_load(menv* e, mval* a)
{
	MASSERT_NUM("load", a, 1);
	MASSERT_TYPE("load", a, 0, MVAL_STRING);

	mpc_result_t r;
	if (mpc_parse_contents(
		a->vals[0]->str,
		get_lang_parser(), &r))
	{
		mval* expr = mval_read(r.output);
		mpc_ast_delete(r.output);

		while (expr->count)
		{
			mval* x = mval_eval(e, mval_pop(expr, 0));
			if (x->type == MVAL_ERROR)
				mval_println(x);
			mval_del(x);
		}

		mval_del(expr);
		mval_del(a);

		return mval_sexpr();
	}
	else
	{
		char* err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);

		mval* err = mval_err("Could not load file %s", err_msg);
		free(err_msg);
		mval_del(a);

		return err;
	}
}