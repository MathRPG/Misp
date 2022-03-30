#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

// TODO: double, more ops, refactor

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include "misp.h"

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

#define MASSERT(args, cond, fmt, ...) \
    if (!(cond)) { mval_delete((args)); return mval_error((fmt), ##__VA_ARGS__); }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-macro-parentheses"
#define MVAL_CTOR(name, type, arg, enum_, field_set) \
    mval* mval_ ## name (type arg) {             \
        mval* v = malloc(sizeof *v);        \
        *v = (mval) {(enum_), field_set,};   \
        return v;                             \
    }
#pragma clang diagnostic pop

MVAL_CTOR_LIST

#undef MVAL_CTOR

mval* mval_error(char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

#define ERR_MAX_SIZE 512

	mval* v = malloc(sizeof *v);
	*v = (mval){ MVAL_ERROR, .error = malloc(ERR_MAX_SIZE), };
	vsnprintf(v->error, ERR_MAX_SIZE - 1, fmt, va);

	v->error = realloc(v->error, strlen(v->error) + 1);

	va_end(va);
	return v;
}

const struct MispList EmptyMispList = {
	.count = 0, .values = NULL,
};

mval* mval_copy(mval* v)
{
	mval* x = malloc(sizeof(mval));
	x->type = v->type;

	switch (v->type)
	{
	case MVAL_SEXPR:
	case MVAL_QEXPR:
	{
		x->exprs.count = v->exprs.count;
		x->exprs.values = malloc(sizeof(mval*) * v->exprs.count);
		for_range(i, 0, x->exprs.count)
		{
			x->exprs.values[i] = mval_copy(v->exprs.values[i]);
		}
		break;
	}
	case MVAL_FUNC:
		x->func = v->func;
		break;
	case MVAL_ERROR:
		x->error = strdup(v->error);
		break;
	case MVAL_SYMBOL:
		x->symbol = strdup(v->symbol);
		break;
	case MVAL_INT:
		x->num = v->num;
		break;
	}

	return x;
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
		mval_delete(e->vals[i]);
	}

	free(e->syms);
	free(e->vals);
	free(e);
}

mval* menv_get(menv* e, mval* k)
{
	for_range(i, 0, e->count)
	{
		if (strcmp(e->syms[i], k->symbol) == 0)
			return mval_copy(e->vals[i]);
	}

	return mval_error("Unbound symbol!");
}

void menv_put(menv* e, mval* k, mval* v)
{
	for_range(i, 0, e->count)
	{
		if (strcmp(e->syms[i], k->symbol) == 0)
		{
			mval_delete(e->vals[i]);
			e->vals[i] = mval_copy(v);
			return;
		}
	}

	e->count++;
	e->vals = realloc(e->vals, sizeof(mval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	e->vals[e->count - 1] = mval_copy(v);
	e->syms[e->count - 1] = strdup(k->symbol);
}

void mval_delete(mval* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
	case MVAL_QEXPR:
	{
		for_range(i, 0, v->exprs.count)
		{
			mval_delete(v->exprs.values[i]);
		}

		free(v->exprs.values);
		break;
	}

	case MVAL_ERROR:
		free(v->error);
		break;

	case MVAL_SYMBOL:
		free(v->symbol);
		break;

	case MVAL_FUNC:
	case MVAL_INT:
		break;
	}

	free(v);
}

void mval_exprs_print(mlist* l, const char* paren)
{
	putchar(paren[0]);

	for_range(i, 0, l->count)
	{
		mval_print(l->values[i]);

		if (i != (l->count - 1))
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
		mval_exprs_print(&v->exprs, "()");
		break;
	case MVAL_QEXPR:
		mval_exprs_print(&v->exprs, "{}");
		break;
	case MVAL_FUNC:
		printf("<function>");
		break;
	case MVAL_ERROR:
		printf("Error: %s", v->error);
		break;
	case MVAL_SYMBOL:
		printf("%s", v->symbol);
		break;
	case MVAL_INT:
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
		{ MVAL_INT, "Number", },
	};

	for_range(i, 0, len(types))
	{
		if (types[i].type == t)
			return types[i].name;
	}

	return "Unknown Type";
}

mval* mval_read_num(const char* s)
{
	errno = 0;
	long value = strtol(s, NULL, 10);

	if (errno == ERANGE)
		return mval_error("Invalid Number");

	return mval_int(value);
}

void mval_list_add(mlist* l, mval* x)
{
	l->count++;
	l->values = realloc(l->values, sizeof(mval*) * l->count);
	l->values[l->count - 1] = x;
}

void mval_add(mval* v, mval* x)
{
	mval_list_add(&v->exprs, x);
}

mval* mval_pop(mlist* l, int i)
{
	mval* x = l->values[i];

	unsigned cells_to_shift = l->count - i - 1;

	memmove(
		&l->values[i],
		&l->values[i + 1],
		sizeof(mval*) * cells_to_shift
	);

	l->count--;
	l->values = realloc(l->values, sizeof(mval*) * l->count);

	return x;
}

mval* mval_take(mval* v, int i)
{
	mval* x = mval_pop(&v->exprs, i);
	mval_delete(v);
	return x;
}

mval* builtin_head(menv* e, mval* v)
{
	mlist* l = &v->exprs;

	MASSERT(v, l->count == 1,
		"Function 'head' expects 1 argument");
	MASSERT(v, l->values[0]->type == MVAL_QEXPR,
		"Function 'head' expects QEXPR argument");
	MASSERT(v, l->values[0]->exprs.count != 0,
		"Function 'head' expects QEXPR with at least 1 element");

	mval* x = mval_take(v, 0);

	while (x->exprs.count > 1)
	{
		mval_delete(mval_pop(&x->exprs, 1));
	}

	return x;
}

mval* builtin_tail(menv* e, mval* v)
{
	mlist* l = &v->exprs;

	MASSERT(v, l->count == 1,
		"Function 'tail' expects 1 argument");
	MASSERT(v, l->values[0]->type == MVAL_QEXPR,
		"Function 'tail' expects QEXPR argument");
	MASSERT(v, l->values[0]->exprs.count != 0,
		"Function 'tail' expects QEXPR with at least 1 element");

	mval* x = mval_take(v, 0);
	mval_delete(mval_pop(&x->exprs, 0));
	return x;
}

mval* builtin_list(menv* e, mval* v)
{
	v->type = MVAL_QEXPR;
	return v;
}

mval* builtin_eval(menv* e, mval* v)
{
	mlist* l = &v->exprs;

	MASSERT(v, l->count == 1,
		"Function 'eval' expects 1 argument");
	MASSERT(v, l->values[0]->type == MVAL_QEXPR,
		"Function 'eval' expects QEXPR argument");

	mval* x = mval_take(v, 0);
	x->type = MVAL_SEXPR;
	return mval_eval(e, x);
}

mval* mval_join(mval* x, mval* y)
{
	while (y->exprs.count)
	{
		mval_list_add(&x->exprs, mval_pop(&y->exprs, 0));
	}

	mval_delete(y);
	return x;
}

mval* builtin_join(menv* e, mval* v)
{
	mlist* l = &v->exprs;
	for_range(i, 0, l->count)
	{
		MASSERT(v, l->values[i]->type == MVAL_QEXPR,
			"Function 'join' expects QEXPR arguments");
	}

	mval* x = mval_pop(l, 0);

	while (l->count)
	{
		x = mval_join(x, mval_pop(l, 0));
	}

	mval_delete(v);
	return x;
}

mval* builtin_math_op(menv* e, mval* a, char* symbol)
{
	mlist* l = &a->exprs;

	for_range(i, 0, l->count)
	{
		if (l->values[i]->type != MVAL_INT)
		{
			mval_delete(a);
			return mval_error("Math operation on non-number");
		}
	}

	mval* x = mval_pop(l, 0);

	if (l->count == 0 && strcmp(symbol, "-") == 0)
	{
		x->num = -x->num;
	}

	while (l->count > 0)
	{
		mval* y = mval_pop(l, 0);

#define OP(symb, body) \
    if(strcmp(symbol, #symb) == 0) { \
        if (0) {} else { body }   \
        x->num symb##= y->num;    \
    }

		OP(+, ;)
		OP(-, ;)
		OP(*, ;)
		OP(/, {
			if (y->num == 0)
			{
				mval_delete(x);
				mval_delete(y);
				x = mval_error("Zero Division Error");
				break;
			}
		})

		mval_delete(y);
	}

	mval_delete(a);
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

void menv_add_builtin(menv* e, char* name, mbuiltin func)
{
	mval* k = mval_symbol(name);
	mval* v = mval_func(func);
	menv_put(e, k, v);
	mval_delete(k);
	mval_delete(v);
}

mval* builtin_def(menv* e, mval* a)
{
	mlist* l = &a->exprs;

	MASSERT(a, l->values[0]->type == MVAL_QEXPR,
		"Function 'def' passed with incorrect type for argument 0.\n"
		"Got %s, Expected %s.",
		mval_type_name(l->values[0]->type),
		mval_type_name(MVAL_QEXPR));

	mlist* syms = &l->values[0]->exprs;

	for_range(i, 0, syms->count)
	{
		MASSERT(a, syms->values[i]->type == MVAL_SYMBOL,
			"Function 'def' received non-symbol in argument list.\n"
			"Got %s, Expected %s.",
			mval_type_name(syms->values[i]->type),
			mval_type_name(MVAL_SYMBOL));
	}

	MASSERT(a, syms->count == l->count - 1,
		"Function 'def' passed with incorrect number of arguments.\n"
		"Got %s, Expected %s.", l->count, syms->count + 1);

	for_range(i, 0, syms->count)
	{
		menv_put(e, syms->values[i], l->values[i + 1]);
	}

	mval_delete(a);
	return mval_sexpr();
}

void menv_add_builtins(menv* e)
{
	struct
	{
		char* name;
		mbuiltin func;
	} const builtins[] = {
		// List Functions
#define LOP(name) {#name, builtin_##name,}
		LOP(list), LOP(head), LOP(tail),
		LOP(eval), LOP(join),

		// Others
		LOP(def),

		// Math Functions
#define MOP(symb, name) {#symb, builtin_##name,}
		MOP(+, add), MOP(-, sub),
		MOP(*, mul), MOP(/, div),
	};

	for_range(i, 0, len(builtins))
	{
		menv_add_builtin(e, builtins[i].name, builtins[i].func);
	}
}

mval* mval_eval_sexpr(menv* e, mval* v)
{
	mlist* l = &v->exprs;

	for_range(i, 0, l->count)
	{
		l->values[i] = mval_eval(e, l->values[i]);
	}

	for_range(i, 0, l->count)
	{
		if (l->values[i]->type == MVAL_ERROR)
			return mval_take(v, i);
	}

	switch (l->count)
	{
	case 0:
		return v;
	case 1:
		return mval_take(v, 0);
	}

	mval* f = mval_pop(l, 0);

	if (f->type != MVAL_FUNC)
	{
		mval_delete(v);
		mval_delete(f);
		return mval_error("First value in SEXPR not a function");
	}

	mval* result = f->func(e, v);
	mval_delete(f);
	return result;
}

mval* mval_eval(menv* e, mval* v)
{
	if (v->type == MVAL_SYMBOL)
	{
		mval* x = menv_get(e, v);
		mval_delete(v);
		return x;
	}

	if (v->type == MVAL_SEXPR)
		return mval_eval_sexpr(e, v);

	return v;
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

bool hasAddableExpression(mpc_ast_t* const* child)
{
	if (strcmp((*child)->tag, "regex") == 0)
		return false;

	const char* parens[] = {
		"(", ")", "{", "}"
	};

	for_range(i, 0, len(parens))
	{
		if (strcmp((*child)->contents, parens[i]) == 0)
			return false;
	}

	return true;
}

mval* mval_read(mpc_ast_t* t)
{
	const char* tag = t->tag;
	if (strstr(tag, "number"))
		return mval_read_num(t->contents);
	if (strstr(tag, "symbol"))
		return mval_symbol(t->contents);

	mval* x = get_expr_type(tag);
	assert(x);

	for (mpc_ast_t** child = t->children;
		 child != t->children + t->children_num;
		 ++child)
	{
		if (hasAddableExpression(child))
		{
			mval_list_add(&x->exprs, mval_read(*child));
		}
	}

	return x;
}