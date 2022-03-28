#include <stdio.h>
#include <stdlib.h>

#include <editline/history.h>

#include "mpc/mpc.h"

#define VERSION_INFO "0.0.2"

typedef struct
{
	const char* name;
	const char* rule;
} ParserProperties_t;

long evaluate_tree(mpc_ast_t* t);

static mpc_parser_t* parsers[4];

enum
{
	NUMBER = 0, OPERATOR, EXPR, MISP
};

void cleanup(void)
{
	mpc_cleanup(4, parsers[NUMBER], parsers[OPERATOR], parsers[EXPR], parsers[MISP]);
}

void create_language(void)
{
	static ParserProperties_t parser_properties[] = {
		{ "number", " /-?[0-9]+/ ", },
		{ "operator", " '+' | '-' | '*' | '/' | '%' ", },
		{ "expr", " <number> | '(' <operator> <expr>+ ')' ", },
		{ "misp", " /^/ <operator> <expr>+ /$/ ", },
	};

#define len(array) (sizeof (array) / sizeof *(array))

	char language_grammar[2048];

	for (int i = 0; i < 4; ++i)
	{
		parsers[i] = mpc_new(parser_properties[i].name);

		char temp[256];

		sprintf(temp, "%s : %s ;",
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
	atexit(cleanup);

	puts("Misp Version " VERSION_INFO);
	puts("Empty input to exit\n");

	while (1)
	{
		char* input = readline("Misp>>> ");

		if (strlen(input) == 0)
		{
			free(input);
			break;
		}

		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>", input, parsers[MISP], &r))
		{
//			mpc_ast_print(r.output);

			long result = evaluate_tree(r.output);
			printf("%li\n", result);

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
	ADD, SUB, MUL, DIV, MOD
} operator_type(char* op)
{
	const char* operators[] = {
		[ADD] = "+",
		[SUB] = "-",
		[MUL] = "*",
		[DIV] = "/",
		[MOD] = "%",
	};

	for (int i = 0; i < len(operators); ++i)
	{
		if (strcmp(op, operators[i]) == 0)
			return i;
	}

	return -1;
}

long evaluate_operator(long x, char* op, long y)
{
	switch (operator_type(op))
	{
	case ADD:
		return x + y;
	case SUB:
		return x - y;
	case MUL:
		return x * y;
	case DIV:
		return x / y;
	case MOD:
		return x % y;
	}

	return 0;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
long evaluate_tree(mpc_ast_t* t)
{
	if (strstr(t->tag, "number"))
	{
		return strtol(t->contents, NULL, 10);
	}

	char* op = t->children[1]->contents;

	long x = evaluate_tree(t->children[2]);

	int i = 3;
	while (strstr(t->children[i]->tag, "expr"))
	{
		x = evaluate_operator(x, op, evaluate_tree(t->children[i]));
		++i;
	}

	return x;
}
#pragma clang diagnostic pop
