#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <editline/history.h>

#include "mpc/mpc.h"

#define VERSION_INFO "0.0.2"

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

// TODO: double, more ops, refactor

typedef enum mval_type
{
	MVAL_SEXPR,
	MVAL_ERROR,
	MVAL_SYMBOL,
	MVAL_INT,
	MVAL_FLOAT,
} type_t;

typedef struct MispValue mval;

typedef struct MispList
{
	int count;
	mval** values;
} mlist;

struct MispValue
{
	enum mval_type type;

	union
	{
		mlist sexpr;
		char* err;
		char* sym;
		long num;
		double dbl;
	};
};

static const mlist EmptyMispList = {
	.count = 0, .values = NULL,
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-macro-parentheses"
#define MVAL_CTOR(name, type, arg, enum_, field_set) \
    mval* mval_ ## name (type arg) {             \
        mval* v = malloc(sizeof *v);        \
        *v = (mval) {(enum_), field_set,};   \
        return v;                             \
    }
#pragma clang diagnostic pop

MVAL_CTOR(int, long, x, MVAL_INT, x)
MVAL_CTOR(error, char*, m, MVAL_ERROR, .err = strdup(m))
MVAL_CTOR(float, long double, x, MVAL_FLOAT, x)
MVAL_CTOR(symbl, char*, s, MVAL_SYMBOL, .sym = strdup(s))
MVAL_CTOR(sexpr, void, , MVAL_SEXPR, EmptyMispList)

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
		free(v->err);
		break;

	case MVAL_SYMBOL:
		free(v->sym);
		break;

	case MVAL_INT:
	case MVAL_FLOAT:
		break;
	}

	free(v);
}

void mval_print(mval*);
void mval_sexpr_print(mlist*);

void mval_print(mval* v)
{
	switch (v->type)
	{
	case MVAL_SEXPR:
		mval_sexpr_print(&v->sexpr);
		break;
	case MVAL_ERROR:
		printf("Error: %s", v->err);
		break;
	case MVAL_SYMBOL:
		printf("%s", v->sym);
		break;
	case MVAL_INT:
		printf("%li", v->num);
		break;
	case MVAL_FLOAT:
		printf("%lf", v->dbl);
		break;
	}
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

void mval_println(mval* v)
{
	mval_print(v);
	putchar('\n');
}

bool is_integer(const char* s)
{
	return strchr(s, '.') == NULL;
}

mval* mval_read_num(const char* s)
{
	errno = 0;
	long double value = strtold(s, NULL);

	if (errno == ERANGE)
		return mval_error("Invalid Number");

	if (is_integer(s))
		return mval_int((long)value);
	return mval_float(value);
}

void mval_list_add(mlist* v, mval* x)
{
	v->count++;
	v->values = realloc(v->values, sizeof(mval*) * v->count);
	v->values[v->count - 1] = x;
}

bool hasMispExpression(mpc_ast_t* const* child)
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
		return mval_symbl(t->contents);

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

bool isNumeric(mval* v)
{
	return v->type == MVAL_INT || v->type == MVAL_FLOAT;
}

typedef struct MispOperator
{
	char* name;
	void (* apply)(mval*, mval*);
} moper;

mval* builtin_op(mval* v, const moper* op)
{
	mlist* l = &v->sexpr;

	for_range(i, 0, l->count)
	{
		if (!isNumeric(l->values[i]))
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

mval* mval_eval(mval* v);
mval* mval_eval_sexpr(mval* v);

mval* mval_eval(mval* v)
{
	if (v->type == MVAL_SEXPR)
		return mval_eval_sexpr(v);
	return v;
}

void mval_builtin_add(mval* x, mval* y)
{
	if (y == NULL)
		return;

	x->num += y->num;
}

void mval_builtin_sub(mval* x, mval* y)
{
	if (y == NULL)
	{
		x->num *= -1;
		return;
	}

	x->num -= y->num;
}

void mval_builtin_div(mval* x, mval* y)
{
	x->num /= y->num;
}

const moper* get_operator_by_symbol(char* sym)
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
		mval* result = builtin_op(v, get_operator_by_symbol(f->sym));
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
		{ "symbol", " '+' | '-' | '*' | '/' ", },
		{ "sexpr", " '(' <expr>* ')' ", },
		{ "expr", " <number> | <symbol> | <sexpr> ", },
		{ "misp", " /^/ <expr>* /$/ ", },
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

void cleanup_parsers(
	__attribute__((unused)) int _,
	mpc_parser_t* const* const parsers)
{
	mpc_cleanup(5, parsers[0], parsers[1], parsers[2], parsers[3], parsers[4]);
}

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
			mval* v = mval_read(r.output);
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
