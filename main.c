#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#include <stdio.h>
#include <stdlib.h>

#include <editline/history.h>
#include <stdbool.h>
#include <stdint.h>

#include "mpc/mpc.h"

#define VERSION_INFO "0.0.2"

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

typedef enum mval_type
{
	MVAL_SEXPR,
	MVAL_ERROR,
	MVAL_SYMBL,
	MVAL_NUMBR,
	MVAL_FLOAT,
} type_t;

typedef enum merr_type
{
	MERR_DIV_ZERO,
	MERR_BAD_OPER,
	MERR_BAD_NUM,
	MERR_BAD_TYPE,
} error_t;

typedef struct MispValue mval_t;

typedef struct MispList
{
	int count;
	mval_t** cells;
} mlist_t;

struct MispValue
{
	enum mval_type type;

	union
	{
		mlist_t sexpr;
		char* err;
		char* sym;
		long num;
		double dbl;
	};
};

static const mlist_t MispEmptyList = {
	.count = 0, .cells = NULL,
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-macro-parentheses"
#define MVAL_CTOR(name, type, arg, enum_, field_set) \
    mval_t* mval_ ## name (type arg) {             \
        mval_t* v = malloc(sizeof *v);        \
        *v = (mval_t) {(enum_), field_set,};   \
        return v;                             \
    }
#pragma clang diagnostic pop

MVAL_CTOR(numbr, long, x, MVAL_NUMBR, x)
MVAL_CTOR(error, char*, m, MVAL_ERROR, .err = strdup(m))
MVAL_CTOR(float, long double, x, MVAL_FLOAT, x)
MVAL_CTOR(symbl, char*, s, MVAL_SYMBL, .sym = strdup(s))
MVAL_CTOR(sexpr, void, , MVAL_SEXPR, MispEmptyList)

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
void mval_delete(mval_t* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
	{
		for_range(i, 0, v->sexpr.count)
		{
			mval_delete(v->sexpr.cells[i]);
		}

		free(v->sexpr.cells);
		break;
	}

	case MVAL_ERROR:
		free(v->err);
		break;

	case MVAL_SYMBL:
		free(v->sym);
		break;

	case MVAL_NUMBR:
	case MVAL_FLOAT:
		break;
	}

	free(v);
}
#pragma clang diagnostic pop

mval_t $mval_num(long x)
{
	return (mval_t){ MVAL_NUMBR, .num = x, };
}

mval_t $mval_err(error_t x)
{
	return (mval_t){ MVAL_ERROR, .num = x, };
}

mval_t mval_flt(long double x)
{
	return (mval_t){ MVAL_FLOAT, .dbl = x, };
}

void mval_print(mval_t* v);
void mval_sexpr_print(mlist_t* v);

void mval_print(mval_t* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
		mval_sexpr_print(&v->sexpr);
		break;
	case MVAL_ERROR:
		printf("Error: %s", v->err);
		break;
	case MVAL_SYMBL:
		printf("%s", v->sym);
		break;
	case MVAL_NUMBR:
		printf("%li", v->num);
		break;
	case MVAL_FLOAT:
		printf("%Lf", v->dbl);
		break;
	}
}

void mval_sexpr_print(mlist_t* l)
{
	putchar('(');

	for_range(i, 0, l->count)
	{
		mval_print(l->cells[i]);

		if (i != (l->count - 1))
		{
			putchar(' ');
		}
	}

	putchar(')');
}

void mval_println(mval_t* v)
{
	mval_print(v);
	putchar('\n');
}

mval_t* mval_read_num(const char* s)
{
	errno = 0;
	long double value = strtold(s, NULL);

	if (errno == ERANGE)
		return mval_error("Invalid Number");

	switch ((intptr_t)strchr(s, '.'))
	{
	case 0: // Integer
		return mval_numbr((long)value);
	default:
		return mval_float(value);
	}
}

void mval_list_add(struct MispList* v, mval_t* x)
{
	v->count++;
	v->cells = realloc(v->cells, sizeof(mval_t*) * v->count);
	v->cells[v->count - 1] = x;
}

bool hasMispExpression(mpc_ast_t* const* child)
{
	return !(strcmp((*child)->tag, "regex") == 0
			 || strcmp((*child)->contents, "(") == 0
			 || strcmp((*child)->contents, ")") == 0);
}

mval_t* mval_read(mpc_ast_t* t)
{
	const char* tag = t->tag;
	if (strstr(tag, "number"))
		return mval_read_num(t->contents);
	if (strstr(tag, "symbol"))
		return mval_symbl(t->contents);

	// Is not root and is not sexpr
	if (strcmp(tag, ">") != 0 && !strstr(tag, "sexpr"))
		return mval_error("Invalid token");

	mval_t* x = mval_sexpr();

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

mval_t* mval_pop(mlist_t* l, int i)
{
	mval_t* x = l->cells[i];

	unsigned cells_to_shift = l->count - i - 1;

	memmove(
		&l->cells[i],
		&l->cells[i + 1],
		sizeof(mval_t*) * cells_to_shift
	);

	l->count--;
	l->cells = realloc(l->cells, sizeof(mval_t*) * l->count);

	return x;
}

mval_t* mval_take(mval_t* v, int i)
{
	mval_t* x = mval_pop(&v->sexpr, i);
	mval_delete(v);
	return x;
}

bool isNumeric(mval_t* v)
{
	return v->type == MVAL_NUMBR || v->type == MVAL_FLOAT;
}

typedef struct MispOperator
{
	char* name;
	void (* apply)(mval_t*, mval_t*);
} mop_t;

mval_t* builtin_op(mval_t* v, const mop_t* op)
{
	mlist_t* l = &v->sexpr;

	for_range(i, 0, l->count)
	{
		if (!isNumeric(l->cells[i]))
		{
			mval_delete(v);
			return mval_error("Unsupported type for operation");
		}
	}

	mval_t* x = mval_pop(l, 0);

	if (l->count == 0)
	{
		op->apply(x, NULL);
		return x;
	}

	while (l->count > 0)
	{
		mval_t* y = mval_pop(&v->sexpr, 0);
		op->apply(x, y);
		mval_delete(y);
	}

	mval_delete(v);
	return x;
}

mval_t* mval_eval(mval_t* v);
mval_t* mval_eval_sexpr(mval_t* v);

mval_t* mval_eval(mval_t* v)
{
	if (v->type == MVAL_SEXPR)
		return mval_eval_sexpr(v);
	return v;
}

void mval_builtin_add(mval_t* x, mval_t* y)
{
	if (y == NULL)
		return;

	x->num += y->num;
}

void mval_builtin_sub(mval_t* x, mval_t* y)
{
	if (y == NULL)
	{
		x->num *= -1;
		return;
	}

	x->num -= y->num;
}

void mval_builtin_div(mval_t* x, mval_t* y)
{
	x->num /= y->num;
}

const mop_t* get_operator_by_symbol(char* sym)
{
	static const mop_t ops[] = {
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

mval_t* mval_eval_sexpr(mval_t* v)
{
	mlist_t* l = &v->sexpr;

	for_range(i, 0, l->count)
	{
		l->cells[i] = mval_eval(l->cells[i]);
	}

	for_range(i, 0, l->count)
	{
		if (l->cells[i]->type == MVAL_ERROR)
			return mval_take(v, i);
	}

	switch (l->count)
	{
	case 0:
		return v;
	case 1:
		return mval_take(v, 0);
	}

	mval_t* f = mval_pop(&v->sexpr, 0);

	switch (f->type)
	{
	case MVAL_SYMBL:
	{
		mval_t* result = builtin_op(v, get_operator_by_symbol(f->sym));
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

mpc_parser_t** create_language(void)
{
	struct
	{
		const char* name, * rule;
	} const parser_properties[] = {
		{ "number", " /[+-]?([0-9]*[.])?[0-9]+/ ", },
		{ "symbol", " '+' | '-' | '*' | '/' | '%' | \"min\" | \"max\" | '^' ", },
		{ "sexpr", " '(' <sexpr>* ')' ", },
		{ "sexpr", " <number> | <symbol> | <sexpr> ", },
		{ "misp", " /^/ <sexpr>* /$/ ", },
	};

	enum
	{
		MISP = len(parser_properties),
	};

	static mpc_parser_t* parsers[len(parser_properties)];
	static char language_grammar[2048];

	for_range(i, 0, len(parser_properties))
	{
		parsers[i] = mpc_new(parser_properties[i].name);

		char temp[256];

		sprintf(temp, " %s : %s ; ",
			parser_properties[i].name,
			parser_properties[i].rule);

		strcat(language_grammar, temp);
	}

	mpca_lang(MPCA_LANG_DEFAULT, language_grammar,
		parsers[0], parsers[1], parsers[2], parsers[3], parsers[4]);

	return parsers;
}

void cleanup_parsers(int _, mpc_parser_t* const* const parsers)
{
	mpc_cleanup(5, parsers[0], parsers[1], parsers[2], parsers[3], parsers[4]);
}

mval_t $evaluate_tree(mpc_ast_t* t);

int main()
{
	mpc_parser_t* const* const parsers = create_language();
	on_exit((void (*)(int, void*))cleanup_parsers, (void*)parsers);

	puts("Misp Version " VERSION_INFO);
	puts("Empty input to exit\n");

	while (1)
	{
		char* const input = readline("Misp> ");

		if (strlen(input) == 0)
		{
			free(input);
			return 0;
		}

		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>", input, parsers[4], &r))
		{
			mval_t* v = mval_read(r.output);
			v = mval_eval(v);
			mval_println(v);
			mval_delete(v);

			mpc_ast_delete(r.output);
		}
		else
		{
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}
}

enum op_type
{
	ADD = 0, SUB, MUL, DIV, MOD, MIN, MAX, EXP
};

enum op_type operator_type(char* op)
{
	const char* operators[] = {
		[ADD] = "+",
		[SUB] = "-",
		[MUL] = "*",
		[DIV] = "/",
		[MOD] = "%",
		[MIN] = "min",
		[MAX] = "max",
		[EXP] = "^",
	};

	for_range(i, 0, len(operators))
	{
		if (strcmp(op, operators[i]) == 0)
			return i;
	}

	return 0;
}

mval_t promote_value(mval_t x)
{
	switch (x.type)
	{
	case MVAL_NUMBR:
		return (mval_t){ MVAL_FLOAT, .dbl = (long double)x.num, };
	default:
		return x;
	}
}

mval_t eval_op_flt(mval_t x_, char* op, mval_t y_)
{
	long double x = x_.dbl, y = y_.dbl;

	switch (operator_type(op))
	{
	case EXP:
		return mval_flt(powl(x, y));
	case ADD:
		return mval_flt(x + y);
	case SUB:
		return mval_flt(x - y);
	case MUL:
		return mval_flt(x * y);
	case DIV:
		return y == 0.0
			   ? $mval_err(MERR_DIV_ZERO)
			   : mval_flt(x / y);
	case MOD:
		return $mval_err(MERR_BAD_TYPE);
	case MIN:
		return mval_flt(x <= y ? x : y);
	case MAX:
		return mval_flt(x >= y ? x : y);
	default:
		return $mval_err(MERR_BAD_OPER);
	}
}

mval_t eval_op_num(mval_t x_, char* op, mval_t y_)
{
	long x = x_.num, y = y_.num;

	switch (operator_type(op))
	{
	case EXP:
		return $mval_num((long)powl(x, y));
	case ADD:
		return $mval_num(x + y);
	case SUB:
		return $mval_num(x - y);
	case MUL:
		return $mval_num(x * y);
	case DIV:
		return y == 0
			   ? $mval_err(MERR_DIV_ZERO)
			   : $mval_num(x / y);
	case MOD:
		return y == 0
			   ? $mval_err(MERR_DIV_ZERO)
			   : $mval_num(x % y);
	case MIN:
		return $mval_num(x <= y ? x : y);
	case MAX:
		return $mval_num(x >= y ? x : y);
	default:
		return $mval_err(MERR_BAD_OPER);
	}
}

mval_t evaluate_operator(mval_t x, char* op, mval_t y)
{
	if (x.type == MVAL_ERROR) return x;
	if (y.type == MVAL_ERROR) return y;

	if (x.type == MVAL_NUMBR && y.type == MVAL_NUMBR)
		return eval_op_num(x, op, y);

	return eval_op_flt(
		promote_value(x),
		op,
		promote_value(y)
	);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
mval_t $evaluate_tree(mpc_ast_t* t)
{
	if (strstr(t->tag, "number"))
	{
		errno = 0;
		long double value = strtold(t->contents, NULL);

		if (errno == ERANGE)
			return $mval_err(MERR_BAD_NUM);

		if (strchr(t->contents, '.'))
			return mval_flt(value);
		return $mval_num((long)value);
	}

	char* op = t->children[1]->contents;

	mval_t x = $evaluate_tree(t->children[2]);

	if (t->children_num - 3 == 1 && strcmp(op, "-") == 0)
	{
		switch (x.type)
		{
		case MVAL_ERROR:
			return x;
		case MVAL_NUMBR:
			return $mval_num(-x.num);
		case MVAL_FLOAT:
			return mval_flt(-x.dbl);
		}
	}

	int i = 3;
	while (strstr(t->children[i]->tag, "sexpr"))
	{
		x = evaluate_operator(x, op, $evaluate_tree(t->children[i]));
		++i;
	}

	return x;
}
#pragma clang diagnostic pop

#pragma clang diagnostic pop