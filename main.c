#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <editline/history.h>

#include "mpc/mpc.h"
#include "misp/parsing.h"
#include "misp/misp.h"

#define VERSION_INFO "0.0.14"

static menv* GLOBAL_ENV = NULL;

void init_global_env()
{
	GLOBAL_ENV = menv_new();
	menv_add_builtins(GLOBAL_ENV);
}

void delete_global_env(void)
{
	assert(GLOBAL_ENV != NULL);
	menv_del(GLOBAL_ENV);
}

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

void free_input(char* const* s)
{
	free(*s);
}

int interactive_prompt(void)
{
	while (1)
	{
		__attribute__((__cleanup__(free_input)))
		char* const input = readline("misp> ");

		if (strlen(input) == 0)
			return 0;

		add_history(input);

		interpret_input(GLOBAL_ENV, input);
	}
}

int main(int argc, char* argv[])
{
	init_lang_parsers();
	atexit(cleanup_lang_parsers);

	init_global_env();
	atexit(delete_global_env);

	puts("Misp Version " VERSION_INFO);
	puts("Empty input to exit\n");

	if (argc >= 2)
	{
		for (int i = 1; i < argc; ++i)
			load_file(GLOBAL_ENV, argv[i]);

		return 0;
	}

	return interactive_prompt();
}
