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

void mval_delete(mval* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
	{
		for_range(i, 0, v->sexpr.count)
		{
			mval_delete(v->sexpr.values[i]);
		}

		free(v->sexpr.values);
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

void mval_sexpr_print(mlist* l)
{
	putchar('(');

	for_range(i, 0, l->count)
	{
		mval_print(l->values[i]);

		if (i != (l->count - 1))
		{
			putchar(' ');
		}
	}

	putchar(')');
}

void mval_print(mval* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
		mval_sexpr_print(&v->sexpr);
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
	long double value = strtold(s, NULL);

	if (errno == ERANGE)
		return mval_error("Invalid Number");

	return mval_int((long)value);
}

void mval_list_add(mlist* v, mval* x)
{
	v->count++;
	v->values = realloc(v->values, sizeof(mval*) * v->count);
	v->values[v->count - 1] = x;
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
	mval* x = mval_pop(&v->sexpr, i);
	mval_delete(v);
	return x;
}

mval* builtin_op(mval* v, const moper* op)
{
	mlist* l = &v->sexpr;

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
		op->apply(x, NULL);
		return x;
	}

	while (l->count > 0)
	{
		mval* y = mval_pop(&v->sexpr, 0);
		op->apply(x, y);
		mval_delete(y);
	}

	mval_delete(v);
	return x;
}

static void mval_builtin_add(mval* x, mval* y)
{
	if (y == NULL)
		return;

	x->num += y->num;
}

static void mval_builtin_sub(mval* x, mval* y)
{
	if (y == NULL)
	{
		x->num *= -1;
		return;
	}

	x->num -= y->num;
}

static void mval_builtin_div(mval* x, mval* y)
{
	x->num /= y->num;
}

static const moper* get_operator_by_symbol(char* sym)
{
	static const moper ops[] = {
		{ "+", mval_builtin_add, },
		{ "-", mval_builtin_sub, },
		{ "/", mval_builtin_div, },
	};

	for_range(i, 0, len(ops))
	{
		if (strcmp(ops[i].name, sym) == 0)
			return &ops[i];
	}

	return NULL;
}

mval* mval_eval_sexpr(mval* v)
{
	mlist* l = &v->sexpr;

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

	mval* f = mval_pop(&v->sexpr, 0);

	switch (f->type)
	{
	case MVAL_SYMBOL:
	{
		mval* result = builtin_op(v, get_operator_by_symbol(f->symbol));
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

static bool hasMispExpression(mpc_ast_t* const* child)
{
	return !(strcmp((*child)->tag, "regex") == 0
			 || strcmp((*child)->contents, "(") == 0
			 || strcmp((*child)->contents, ")") == 0);
}

mval* mval_read(mpc_ast_t* t)
{
	const char* tag = t->tag;
	if (strstr(tag, "number"))
		return mval_read_num(t->contents);
	if (strstr(tag, "symbol"))
		return mval_symbol(t->contents);

	// Is not root and is not sexpr
	if (strcmp(tag, ">") != 0 && !strstr(tag, "sexpr"))
		return mval_error("Invalid token");

	mval* x = mval_sexpr();

	for (mpc_ast_t** child = t->children;
		 child != t->children + t->children_num;
		 ++child)
	{
		if (hasMispExpression(child))
		{
			mval_list_add(&x->sexpr, mval_read(*child));
		}
	}

	return x;
}