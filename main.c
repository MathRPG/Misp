#include <stdio.h>
#include <stdlib.h>

#include <editline/history.h>

#include "mpc/mpc.h"
#include "misp/parsing.h"
#include "misp/misp.h"

#define VERSION_INFO "0.0.14"

int main()
{
	init_lang_parsers();

	puts("Misp Version " VERSION_INFO);
	puts("Empty input to exit\n");

	menv* e = menv_new();
	menv_add_builtins(e);

	while (1)
	{
		char* const input = readline("misp> ");

		if (strlen(input) == 0)
		{
			free(input);
			menv_del(e);
			cleanup_lang_parsers();
			return 0;
		}

		add_history(input);

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

		free(input);
	}
}
