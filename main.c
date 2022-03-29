#include <stdio.h>
#include <stdlib.h>

#include <editline/history.h>

#include "mpc/mpc.h"

#define VERSION_INFO "0.0.2"

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) for(int (var) = (start); (var) < (stop); ++(var))

typedef enum mval_type
{
	MVAL_NUM,
	MVAL_FLT,
	MVAL_ERR,
} type_t;

typedef enum merr_type
{
	MERR_DIV_ZERO,
	MERR_BAD_OP,
	MERR_BAD_NUM,
	MERR_BAD_TYPE,
} error_t;

typedef long num_t;
typedef long double flt_t;

typedef struct MispValue
{
	enum mval_type type;

	union
	{
		num_t num;
		flt_t flt;
		error_t err;
	};

} mval_t;

mval_t mval_num(long x)
{
	return (mval_t){ MVAL_NUM, .num = x, };
}

mval_t mval_err(error_t x)
{
	return (mval_t){ MVAL_ERR, .err = x, };
}

mval_t mval_flt(long double x)
{
	return (mval_t){ MVAL_FLT, .flt = x, };
}

void mval_print(mval_t v)
{
	switch (v.type)
	{
	case MVAL_NUM:
		printf("%li", v.num);
		return;
	case MVAL_ERR:
		printf("Error: %s", (const char* []){
			"Division by Zero",
			"Invalid Operator",
			"Invalid Number",
			"Unexpected Type",
		}[v.err]);
		return;
	case MVAL_FLT:
		printf("%Lf", v.flt);
	}
}

void mval_println(mval_t v)
{
	mval_print(v);
	putchar('\n');
}

mval_t evaluate_tree(mpc_ast_t* t);

static mpc_parser_t* parsers[4];

void create_language(void)
{
	struct
	{
		const char* name, * rule;
	} const parser_properties[] = {
		{ "number", " /[+-]?([0-9]*[.])?[0-9]+/ ", },
		{ "operator", " '+' | '-' | '*' | '/' | '%' | \"min\" | \"max\" | '^' ", },
		{ "expr", " <number> | '(' <operator> <expr>+ ')' ", },
		{ "misp", " /^/ <operator> <expr>+ /$/ ", },
	};

	static char language_grammar[2048];

	for_range(i, 0, 4)
	{
		parsers[i] = mpc_new(parser_properties[i].name);

		char temp[256];

		sprintf(temp, "%s : %s ; ",
			parser_properties[i].name,
			parser_properties[i].rule);

		strcat(language_grammar, temp);
	}

	mpca_lang(MPCA_LANG_DEFAULT, language_grammar,
		parsers[0], parsers[1], parsers[2], parsers[3]);
}

int main()
{
	create_language();

	puts("Misp Version " VERSION_INFO);
	puts("Empty input to exit\n");

	while (1)
	{
		char* input = readline("Misp>>> ");

		if (strlen(input) == 0)
		{
			free(input);
			mpc_cleanup(4, parsers[0], parsers[1], parsers[2], parsers[3]);
			break;
		}

		add_history(input);

		enum
		{
			MISP = 3
		};

		mpc_result_t r;
		if (mpc_parse("<stdin>", input, parsers[MISP], &r))
		{
			mval_t result = evaluate_tree(r.output);
			mval_println(result);

			mpc_ast_delete(r.output);
		}
		else
		{
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}

	return 0;
}

enum
{
	ADD, SUB, MUL, DIV, MOD, MIN, MAX, EXP
} operator_type(char* op)
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
	case MVAL_NUM:
		return (mval_t){ MVAL_FLT, .flt = (long double)x.num, };
	default:
		return x;
	}
}

mval_t eval_op_flt(mval_t x_, char* op, mval_t y_)
{
	long double x = x_.flt, y = y_.flt;

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
			   ? mval_err(MERR_DIV_ZERO)
			   : mval_flt(x / y);
	case MOD:
		return mval_err(MERR_BAD_TYPE);
	case MIN:
		return mval_flt(x <= y ? x : y);
	case MAX:
		return mval_flt(x >= y ? x : y);
	default:
		return mval_err(MERR_BAD_OP);
	}
}

mval_t eval_op_num(mval_t x_, char* op, mval_t y_)
{
	long x = x_.num, y = y_.num;

	switch (operator_type(op))
	{
	case EXP:
		return mval_num((long)powl(x, y));
	case ADD:
		return mval_num(x + y);
	case SUB:
		return mval_num(x - y);
	case MUL:
		return mval_num(x * y);
	case DIV:
		return y == 0
			   ? mval_err(MERR_DIV_ZERO)
			   : mval_num(x / y);
	case MOD:
		return y == 0
			   ? mval_err(MERR_DIV_ZERO)
			   : mval_num(x % y);
	case MIN:
		return mval_num(x <= y ? x : y);
	case MAX:
		return mval_num(x >= y ? x : y);
	default:
		return mval_err(MERR_BAD_OP);
	}
}

mval_t evaluate_operator(mval_t x, char* op, mval_t y)
{
	if (x.type == MVAL_ERR) return x;
	if (y.type == MVAL_ERR) return y;

	if (x.type == MVAL_NUM && y.type == MVAL_NUM)
		return eval_op_num(x, op, y);

	return eval_op_flt(
		promote_value(x),
		op,
		promote_value(y)
	);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
mval_t evaluate_tree(mpc_ast_t* t)
{
	if (strstr(t->tag, "number"))
	{
		errno = 0;
		long double value = strtold(t->contents, NULL);

		if (errno == ERANGE)
			return mval_err(MERR_BAD_NUM);

		if (strchr(t->contents, '.'))
			return mval_flt(value);
		return mval_num((long)value);
	}

	char* op = t->children[1]->contents;

	mval_t x = evaluate_tree(t->children[2]);

	if (t->children_num - 3 == 1 && strcmp(op, "-") == 0)
	{
		switch (x.type)
		{
		case MVAL_ERR:
			return x;
		case MVAL_NUM:
			return mval_num(-x.num);
		case MVAL_FLT:
			return mval_flt(-x.flt);
		}
	}

	int i = 3;
	while (strstr(t->children[i]->tag, "expr"))
	{
		x = evaluate_operator(x, op, evaluate_tree(t->children[i]));
		++i;
	}

	return x;
}
#pragma clang diagnostic pop
