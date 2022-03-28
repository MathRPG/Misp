#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <editline/history.h>

#include "mpc/mpc.h"

#define VERSION_INFO "0.0.2"

typedef struct
{
	const char* name;
	const char* definition;
	mpc_parser_t* parser_ptr;
} Parser_t;

int main()
{
	const char* parser_names[] = {
		"number", "operator", "expr", "misp"
	};

	mpc_parser_t* Number, * Operator, * Expr, * Misp;
	mpc_parser_t** parsers[4] = { &Number, &Operator, &Expr, &Misp };

	for (int i = 0; i < sizeof parsers / sizeof *parsers; ++i)
		*parsers[i] = mpc_new(parser_names[i]);

	mpca_lang(MPCA_LANG_DEFAULT, ""
								 "number 	: /-?[0-9]+/ ;"
								 "operator	: '+' | '-' | '*' | '/' | '%' ;"
								 "expr		: <number> | '(' <operator> <expr>+ ')' ;"
								 "misp		: /^/ <operator> <expr>+ /$/ ;",
		Number, Operator, Expr, Misp);

	puts("Misp Version " VERSION_INFO);
	puts("Press Ctrl+C to exit\n");

	{
		volatile bool loop_prompt = true;
		while (loop_prompt)
		{
			char* input = readline("Misp>>> ");
			add_history(input);

			mpc_result_t r;
			if (mpc_parse("<stdin>", input, Misp, &r))
			{
				mpc_ast_print(r.output);
				mpc_ast_delete(r.output);
			}
			else
			{
				mpc_err_print(r.error);
				mpc_err_print(r.error);
			}

			free(input);
		}
	}

	mpc_cleanup(4, Number, Operator, Expr, Misp);
	return 0;
}
