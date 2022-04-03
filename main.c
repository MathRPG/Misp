#include <stdio.h>
#include <stdlib.h>

#include <editline/history.h>

#include "mpc/mpc.h"
#include "misp/parsing.h"
#include "misp/misp.h"

#define VERSION_INFO "0.0.14"

void interpret_input(menv* e, const char* input)
{
	mpc_result_t r;
	if (mpc_parse("<stdin>", input, get_lang_parser(), &r))
	{
		mval* v = mval_read(r.output);
		v = mval_eval(e, v);
		mval_println(v);
		mval_del(v);

		mpc_ast_delete(r.output);
	}
	else
	{
		mpc_err_print(r.error);
		mpc_err_delete(r.error);
	}
}

void interpret_input_(menv* e, char* input)
{
	int pos = 0;
	mval* expr = mval_read_expr_(input, &pos, '\0');
	mval* x = mval_eval(e, expr);
	mval_println(x);
	mval_del(x);
}

int interactive_prompt(menv* e)
{
	while (1)
	{
		char* const input = readline("misp> ");

		if (strlen(input) == 0)
		{
			free(input);
			menv_del(e);
			return 0;
		}

		add_history(input);

		interpret_input_(e, input);

		free(input);
	}
}

int main(int argc, char* argv[])
{
	init_lang_parsers();
	atexit(cleanup_lang_parsers);

	puts("Misp Version " VERSION_INFO);
	puts("Empty input to exit\n");

	menv* e = menv_new();
	menv_add_builtins(e);

	if (argc >= 2)
	{
		for (int i = 1; i < argc; ++i)
		{
			load_file(e, argv[i]);
		}

		menv_del(e);
		return 0;
	}

	return interactive_prompt(e);
}
