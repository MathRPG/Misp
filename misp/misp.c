#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "DanglingPointer"

// TODO: double, more ops, refactor

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include "misp.h"

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
		break;

	case MVAL_ERROR:
		free(v->err);
		break;

	case MVAL_SYMBOL:
		free(v->sym);
		break;

	case MVAL_NUM:
		break;
	}

	free(v);
}

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
		break;

	case MVAL_ERROR:
		x->err = strdup(v->err);
		break;

	case MVAL_SYMBOL:
		x->sym = strdup(v->sym);
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
		printf("<function>");
		break;
	case MVAL_ERROR:
		printf("Error: %s", v->err);
		break;
	case MVAL_SYMBOL:
		printf("%s", v->sym);
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

const char* mval_type_name(enum mval_type t)
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
		.count = 0,
		.syms = NULL,
		.vals = NULL,
	};
	return e;
}

void menv_delete(menv* e)
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

mval* menv_get(menv* e, mval* k)
{
	for_range(i, 0, e->count)
	{
		if (strcmp(e->syms[i], k->sym) == 0)
			return mval_copy(e->vals[i]);
	}

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

#define MASSERT(args, cond, fmt, ...) \
    if (!(cond)) {                    \
    mval* err = mval_err(fmt, ##__VA_ARGS__); \
    mval_del((args)); return err; }

#define MASSERT_TYPE(func, args, index, expect) \
    MASSERT(args, (args)->vals[index]->type == (expect), \
        "Function '%s' passed with incorrect type for argument %i.\n" \
        "Got %s, Expected %s.",                       \
        func, index, mval_type_name((args)->vals[index]->type), mval_type_name(expect))

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

mval* builtin_def(menv* e, mval* a)
{
	MASSERT_TYPE("def", a, 0, MVAL_QEXPR);

	mval* syms = a->vals[0];

	for_range(i, 0, syms->count)
	{
		MASSERT(a, (syms->vals[i]->type == MVAL_SYMBOL),
			"Function 'def' cannot define non-symbol.\n"
			"Got %s, Expected %s.",
			mval_type_name(syms->vals[i]->type),
			mval_type_name(MVAL_SYMBOL));
	}

	MASSERT(a, (syms->count == a->count - 1),
		"Function 'def' passed with incorrect number of arguments.\n"
		"Got %i, Expected %i.",
		syms->count, a->count - 1);

	for_range(i, 0, syms->count)
	{
		menv_put(e, syms->vals[i], a->vals[i + 1]);
	}

	mval_del(a);
	return mval_sexpr();
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

void menv_add_builtins(menv* e)
{
	struct
	{
		char* name;
		mbuiltin func;
	} const static builtins[] = {
		{ "def", builtin_def, },
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
	};

	for_range(i, 0, len(builtins))
	{
		menv_add_builtin(e, builtins[i].name, builtins[i].func);
	}
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
			mval_type_name(f->type), mval_type_name(MVAL_FUNC)
		);
		mval_del(f);
		mval_del(v);
		return err;
	}

	mval* result = f->func(e, v);
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

mval* get_expr_type(const char* tag);
bool should_skip(const mpc_ast_t* child);

mval* mval_read(mpc_ast_t* t)
{
	const char* tag = t->tag;
	if (strstr(tag, "number"))
		return mval_read_num(t->contents);
	if (strstr(tag, "sym"))
		return mval_sym(t->contents);

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

	if (strcmp(child->tag, "regex") == 0)
		return true;

	return false;
}
