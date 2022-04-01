#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "bugprone-macro-parentheses"
#pragma ide diagnostic ignored "DanglingPointer"

// TODO: double, more ops, refactor

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include "misp.h"
#include "parsing.h"

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

mval* mval_sexpr(void)
{
	mval* v = malloc(sizeof *v);
	v->type = (MVAL_SEXPR);
	v->count = 0;
	v->vals = NULL;
	return v;
}

mval* mval_qexpr(void)
{
	mval* v = malloc(sizeof *v);
	v->type = (MVAL_QEXPR);
	v->count = 0;
	v->vals = NULL;
	return v;
}

mval* mval_func(mbuiltin f)
{
	mval* v = malloc(sizeof *v);
	v->type = (MVAL_FUNC);
	v->func = f;
	return v;
}

mval* mval_lambda(mval* args, mval* body)
{
	mval* v = malloc(sizeof *v);
	*v = (mval){
		.type = MVAL_FUNC,
		.func = NULL,
		.env = menv_new(),
		.args = args,
		.body = body,
	};
	return v;
}

#define ERR_MAX_SIZE 512

__attribute__((format(printf, 1, 2)))
mval* mval_err(char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	// TODO: vsnprintf can tell how many characters it needs

	mval* v = malloc(sizeof *v);
	*v = (mval){ MVAL_ERROR, .err = malloc(ERR_MAX_SIZE), };
	vsnprintf(v->err, ERR_MAX_SIZE - 1, fmt, va);

	v->err = realloc(v->err, strlen(v->err) + 1);

	va_end(va);
	return v;
}

mval* mval_sym(char* s)
{
	mval* v = malloc(sizeof *v);
	v->type = (MVAL_SYMBOL);
	v->sym = strdup(s);
	return v;
}

mval* mval_str(const char* s)
{
	mval* v = malloc(sizeof *v);
	v->type = (MVAL_STRING);
	v->str = strdup(s);
	return v;
}

mval* mval_num(long x)
{
	mval* v = malloc(sizeof *v);
	v->type = (MVAL_NUM);
	v->num = x;
	return v;
}

void mval_del(mval* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
	case MVAL_QEXPR:
	{
		for_range(i, 0, v->count)
		{
			mval_del(v->vals[i]);
		}

		free(v->vals);
		break;
	}

	case MVAL_FUNC:
		if (v->func == NULL)
		{
			menv_del(v->env);
			mval_del(v->args);
			mval_del(v->body);
		}
		break;

	case MVAL_ERROR:
		free(v->err);
		break;

	case MVAL_SYMBOL:
		free(v->sym);
		break;

	case MVAL_STRING:
		free(v->str);
		break;

	case MVAL_NUM:
		break;
	}

	free(v);
}

menv* menv_copy(menv* e);

mval* mval_copy(mval* v)
{
	// TODO: Study possibility of memcpy

	mval* x = malloc(sizeof *x);
	x->type = v->type;

	switch (v->type)
	{
	case MVAL_SEXPR:
	case MVAL_QEXPR:
	{
		x->count = v->count;
		x->vals = malloc(sizeof(mval*) * v->count);
		for_range(i, 0, x->count)
		{
			x->vals[i] = mval_copy(v->vals[i]);
		}
		break;
	}

	case MVAL_FUNC:
		x->func = v->func;
		if (v->func == NULL)
		{
			x->env = menv_copy(v->env);
			x->args = mval_copy(v->args);
			x->body = mval_copy(v->body);
		}
		break;

	case MVAL_ERROR:
		x->err = strdup(v->err);
		break;

	case MVAL_SYMBOL:
		x->sym = strdup(v->sym);
		break;

	case MVAL_STRING:
		x->str = strdup(v->str);
		break;

	case MVAL_NUM:
		x->num = v->num;
		break;
	}

	return x;
}

mval* mval_add(mval* v, mval* x)
{
	v->count++;
	v->vals = realloc(v->vals, sizeof(mval*) * v->count);
	v->vals[v->count - 1] = x;
	return v;
}

mval* mval_join(mval* x, mval* y)
{
	for_range(i, 0, y->count)
	{
		x = mval_add(x, y->vals[i]);
	}

	free(y->vals);
	free(y);

	return x;
}

mval* mval_pop(mval* v, int i)
{
	mval* x = v->vals[i];

	int cells_to_shift = v->count - i - 1;
	assert(cells_to_shift >= 0);

	memmove(
		&v->vals[i],
		&v->vals[i + 1],
		sizeof(mval*) * cells_to_shift
	);

	v->count--;
	v->vals = realloc(v->vals, sizeof(mval*) * v->count);

	return x;
}

mval* mval_take(mval* v, int i)
{
	mval* x = mval_pop(v, i);
	mval_del(v);
	return x;
}

int mval_eq(mval* x, mval* y)
{
	if (x->type != y->type)
		return 0;

	switch (x->type)
	{
	case MVAL_SEXPR:
	case MVAL_QEXPR:
	{
		if (x->count != y->count)
			return 0;
		for_range(i, 0, x->count)
		{
			if (!mval_eq(x->vals[i], y->vals[i]))
				return 0;
		}
		return 1;
	}
	case MVAL_FUNC:
		if (x->func || y->func)
			return x->func == y->func;
		return mval_eq(x->args, y->args)
			   && mval_eq(x->body, y->body);
	case MVAL_ERROR:
		return strcmp(x->err, y->err) == 0;
	case MVAL_SYMBOL:
		return strcmp(x->sym, y->sym) == 0;
	case MVAL_STRING:
		return strcmp(x->str, y->str) == 0;
	case MVAL_NUM:
		return x->num == y->num;
	}

	assert(0);
}

void mval_print_expr(mval* v, const char* paren)
{
	putchar(paren[0]);

	for_range(i, 0, v->count)
	{
		mval_print(v->vals[i]);

		if (i != (v->count - 1))
		{
			putchar(' ');
		}
	}

	putchar(paren[1]);
}

void mval_print_str(mval* v)
{
	char* escaped = strdup(v->str);
	escaped = mpcf_escape(escaped);
	printf("\"%s\"", escaped);
	free(escaped);
}

void mval_print(mval* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
		mval_print_expr(v, "()");
		break;
	case MVAL_QEXPR:
		mval_print_expr(v, "{}");
		break;
	case MVAL_FUNC:
		if (v->func != NULL)
		{
			printf("<builtin>");
		}
		else
		{
			printf("(\\ ");
			mval_print(v->args);
			putchar(' ');
			mval_print(v->body);
			putchar(')');
		}
		break;
	case MVAL_ERROR:
		printf("Error: %s", v->err);
		break;
	case MVAL_SYMBOL:
		printf("%s", v->sym);
		break;
	case MVAL_STRING:
		mval_print_str(v);
		break;
	case MVAL_NUM:
		printf("%li", v->num);
		break;
	}
}

void mval_println(mval* v)
{
	mval_print(v);
	putchar('\n');
}

const char* mtype_name(enum mval_type t)
{
	struct
	{
		enum mval_type type;
		const char* name;
	} const types[] = {
		{ MVAL_SEXPR, "S-Expression", },
		{ MVAL_QEXPR, "Q-Expression", },
		{ MVAL_FUNC, "Function", },
		{ MVAL_ERROR, "Error", },
		{ MVAL_SYMBOL, "Symbol", },
		{ MVAL_STRING, "String", },
		{ MVAL_NUM, "Number", },
	};

	for_range(i, 0, len(types))
	{
		if (types[i].type == t)
			return types[i].name;
	}

	return "Unknown Type";
}

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

//#define BUILTIN_MATH(symb, name)\
//    mval* builtin_##name (menv* e, mval* a) {return builtin_math_op(e, a, #symb)}

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

#define BUILTIN_CMP(F) \
    F(gt, >)                  \
    F(lt, <)                  \
    F(ge, >=)                 \
    F(le, <=)

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

#define CMP_FUNC(name, symb) \
    mval* builtin_##name (menv* e, mval* a) { return builtin_ord(e, a, #symb); }

BUILTIN_CMP(CMP_FUNC)

mval* builtin_cmp(menv* e, mval* a, const char* op)
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

typeof(menv_def)* get_menv_func(const char* func_name)
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

mval* builtin_fun(menv* e, mval* a)
{
	MASSERT_NUM("fun", a, 2);
	MASSERT_TYPE("fun", a, 0, MVAL_QEXPR);
	MASSERT_TYPE("fun", a, 1, MVAL_QEXPR);

	for_range(i, 0, a->vals[0]->count)
	{
		enum mval_type type = a->vals[0]->vals[i]->type;
		MASSERT(a, (type == MVAL_SYMBOL),
			"Cannot define non-symbol.\nGot %s, Expected %s.",
			mtype_name(type),
			mtype_name(MVAL_SYMBOL));
	}

	mval* body = mval_pop(a, 1);
	mval* args = builtin_tail(e, mval_copy(a));

	mval* to_def = mval_add(
		mval_add(
			mval_qexpr(),
			builtin_head(e, a)
		),
		mval_lambda(args, body)
	);

	return builtin_def(e, to_def);
}

mval* builtin_print(menv* e, mval* a)
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

mval* builtin_error(menv* e, mval* a)
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

void load_file(menv* e, const char* filename)
{
	mval* args = mval_add(
		mval_sexpr(),
		mval_str(filename)
	);

	mval* x = builtin_load(e, args);

	if (x->type == MVAL_ERROR)
		mval_println(x);
	mval_del(x);
}

void menv_add_builtin(menv* e, char* name, mbuiltin func)
{
	mval* k = mval_sym(name);
	mval* v = mval_func(func);
	menv_put(e, k, v);
	mval_del(k);
	mval_del(v);
}

//#define LOP(name) {#name, builtin_##name,}
//#define MOP(symb, name) {#symb, builtin_##name,}
//#define COP(name, symb) {#symb, builtin_##name,},

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
		{ "fun", builtin_fun, },
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

mval* mval_call(menv* e, mval* f, mval* a)
{
	if (f->func)
		return f->func(e, a);

	int given = a->count;
	int total = f->args->count;

	while (a->count)
	{
		if (f->args->count == 0)
		{
			mval_del(a);
			return mval_err(
				"Function called with too many arguments.\n"
				"Got %i, Expected %i.", given, total);
		}

		mval* sym = mval_pop(f->args, 0);

		if (strcmp(sym->sym, "&") == 0)
		{
			if (f->args->count != 1)
			{
				mval_del(a);
				return mval_err(
					"Function format invalid.\n"
					"Symbol '&' not followed by single symbol.");
			}

			mval* next_symbol = mval_pop(f->args, 0);
			menv_put(f->env, next_symbol, builtin_list(e, a));
			mval_del(sym);
			mval_del(next_symbol);
			break;
		}

		mval* val = mval_pop(a, 0);

		menv_put(f->env, sym, val);

		mval_del(sym);
		mval_del(val);
	}

	mval_del(a);

	if (f->args->count > 0
		&& strcmp(f->args->vals[0]->sym, "&") == 0)
	{
		if (f->args->count != 2)
		{
			return mval_err(
				"Function format invalid.\n"
				"Symbol '&' not followed by single symbol.");
		}

		mval_del(mval_pop(f->args, 0));

		mval* sym = mval_pop(f->args, 0);
		mval* val = mval_qexpr();

		menv_put(f->env, sym, val);

		mval_del(sym);
		mval_del(val);
	}

	if (f->args->count > 0)
		return mval_copy(f);

	f->env->par = e;

	return builtin_eval(
		f->env,
		mval_add(
			mval_sexpr(),
			mval_copy(f->body)
		)
	);
}

mval* mval_eval_sexpr(menv* e, mval* v)
{
	for_range(i, 0, v->count)
	{
		v->vals[i] = mval_eval(e, v->vals[i]);
	}

	for_range(i, 0, v->count)
	{
		if (v->vals[i]->type == MVAL_ERROR)
			return mval_take(v, i);
	}

	if (v->count == 0)
		return v;
	if (v->count == 1)
		return mval_take(v, 0);

	mval* f = mval_pop(v, 0);

	if (f->type != MVAL_FUNC)
	{
		mval* err = mval_err(
			"S-Expression starts with incorrect type.\n"
			"Got %s, Expected %s.",
			mtype_name(f->type), mtype_name(MVAL_FUNC)
		);
		mval_del(f);
		mval_del(v);
		return err;
	}

	mval* result = mval_call(e, f, v);
	mval_del(f);
	return result;
}

mval* mval_eval(menv* e, mval* v)
{
	if (v->type == MVAL_SYMBOL)
	{
		mval* x = menv_get(e, v);
		mval_del(v);
		return x;
	}

	if (v->type == MVAL_SEXPR)
		return mval_eval_sexpr(e, v);

	return v;
}

mval* mval_read_num(const char* s)
{
	errno = 0;
	long value = strtol(s, NULL, 10);

	if (errno == ERANGE)
		return mval_err("Invalid Number");

	return mval_num(value);
}

mval* mval_read_str(const char* s)
{
	// Cut off \" chars from start and end
	char* unescaped = strndup(s + 1, strlen(s) - 2);
	unescaped = mpcf_unescape(unescaped);
	mval* str = mval_str(unescaped);
	free(unescaped);
	return str;
}

mval* get_expr_type(const char* tag);
bool should_skip(const mpc_ast_t* child);

mval* mval_read(mpc_ast_t* t)
{
	const char* tag = t->tag;
	if (strstr(tag, "number"))
		return mval_read_num(t->contents);
	if (strstr(tag, "symbol"))
		return mval_sym(t->contents);
	if (strstr(tag, "string"))
		return mval_read_str(t->contents);

	mval* x = get_expr_type(tag);
	assert(x);

	for_range(i, 0, t->children_num)
	{
		struct mpc_ast_t* child = t->children[i];
		if (should_skip(child))
			continue;
		x = mval_add(x, mval_read(child));
	}

	return x;
}

mval* get_expr_type(const char* tag)
{
	if (strcmp(tag, ">") == 0)
		return mval_sexpr();

	struct
	{
		const char* tag;
		mval* (* ctor)(void);
	} const types[] = {
		{ "sexpr", mval_sexpr },
		{ "qexpr", mval_qexpr },
	};

	for_range(i, 0, len(types))
	{
		if (strstr(tag, types[i].tag))
			return types[i].ctor();
	}

	return NULL;
}

bool should_skip(const mpc_ast_t* child)
{
	// TODO: Study possibility of strstr(contents, "(){}")

	const char* parens[] = {
		"(", ")", "{", "}"
	};

	for_range(i, 0, len(parens))
	{
		if (strcmp(child->contents, parens[i]) == 0)
			return true;
	}

	if (strstr(child->tag, "comment"))
		return true;

	if (strcmp(child->tag, "regex") == 0)
		return true;

	return false;
}
