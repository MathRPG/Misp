#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

// TODO: double, more ops, refactor

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "misp.h"

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

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

const struct MispList EmptyMispList = {
	.count = 0, .values = NULL,
};

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

// TODO: remake interface
mval* builtin(mval* v, const moper* op)
{
	if (op->type == MOPER_UNARY)
		return op->apply_unary(v);

	mlist* l = &v->exprs;

	for_range(i, 0, l->count)
	{
		if (l->values[i]->type != MVAL_INT)
		{
			mval_delete(v);
			return mval_error("Unsupported type for operation");
		}
	}

	mval* x = mval_pop(l, 0);

	if (l->count == 0)
	{
		op->apply_binary(x, NULL);
		return x;
	}

	while (l->count > 0)
	{
		mval* y = mval_pop(&v->exprs, 0);
		op->apply_binary(x, y);
		mval_delete(y);
	}

	mval_delete(v);
	return x;
}

#define MASSERT(args, cond, err) \
    if (!(cond)) { mval_delete(args); return mval_error(err); }

mval* builtin_head(mval* v)
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

mval* builtin_tail(mval* v)
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

mval* builtin_list(mval* v)
{
	v->type = MVAL_QEXPR;
	return v;
}

mval* builtin_eval(mval* v)
{
	mlist* l = &v->exprs;

	MASSERT(v, l->count == 1,
		"Function 'eval' expects 1 argument");
	MASSERT(v, l->values[0]->type == MVAL_QEXPR,
		"Function 'eval' expects QEXPR argument");

	mval* x = mval_take(v, 0);
	x->type = MVAL_SEXPR;
	return mval_eval(x);
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

mval* builtin_join(mval* v)
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

mval* builtin_add(mval* x, mval* y)
{
	x->num += y->num;
	return x;
}

mval* builtin_sub(mval* x, mval* y)
{
	if (y == NULL)
	{
		x->num *= -1;
		return x;
	}

	x->num -= y->num;
	return x;
}

mval* builtin_div(mval* x, mval* y)
{
	x->num /= y->num;
	return x;
}

static const moper* get_operator_by_symbol(char* sym)
{
#define OP1(name) { #name, MOPER_UNARY, builtin_ ## name, }
#define OP2(symb, name) { #symb, MOPER_BINARY, .apply_binary = builtin_ ## name, }

	static const moper ops[] = {
		OP1(list), OP1(head), OP1(tail), OP1(join), OP1(eval),
		OP2(+, add), OP2(-, sub), OP2(/, div),
	};

	for_range(i, 0, len(ops))
	{
		if (strcmp(ops[i].name, sym) == 0)
			return &ops[i];
	}

	return NULL;
}

mval* apply_operator(mval* v, const moper* op)
{
	if (op != NULL)
	{
		return builtin(v, op);
	}
	else
	{
		return mval_error("Unknown Function");
	}
}

mval* mval_eval_sexpr(mval* v)
{
	mlist* l = &v->exprs;

	for_range(i, 0, l->count)
	{
		l->values[i] = mval_eval(l->values[i]);
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

	mval* f = mval_pop(&v->exprs, 0);

	switch (f->type)
	{
	case MVAL_SYMBOL:
	{
		const moper* op = get_operator_by_symbol(f->symbol);
		mval* result = apply_operator(v, op);
		mval_delete(f);
		return result;
	}
	default:
	{
		mval_delete(f);
		mval_delete(v);
		return mval_error("Invalid S-expression");
	}
	}
}

mval* mval_eval(mval* v)
{
	if (v->type == MVAL_SEXPR)
		return mval_eval_sexpr(v);
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