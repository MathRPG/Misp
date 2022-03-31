#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <editline/history.h>

#include "mpc/mpc.h"
#include "misp/misp.h"

#define VERSION_INFO "0.0.11"

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

mpc_parser_t** create_language(void)
{
	struct
	{
		const char* name, * rule;
	} const parser_properties[] = {
//		{ "number", " /[+-]?([0-9]*[.])?[0-9]+/ ", },
		{ "number", " /[+-]?[0-9]+/ ", },
		{ "sym", " /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/", },
		{ "sexpr", " '(' <expr>* ')' ", },
		{ "qexpr", " '{' <expr>* '}' ", },
		{ "expr", " <number> | <sym> | <sexpr> | <qexpr> ", },
		{ "misp", " /^/ <expr>* /$/ ", },
	};

#define MISP 5
	assert(MISP + 1 == len(parser_properties));

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
		parsers[0], parsers[1], parsers[2], parsers[3], parsers[4], parsers[5]);

	return parsers;
}

void cleanup_parsers(mpc_parser_t* const* const parsers)
{
	mpc_cleanup(5, parsers[0], parsers[1], parsers[2], parsers[3], parsers[4], parsers[5]);
}

int main()
{
	mpc_parser_t* const* const parsers = create_language();

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
			menv_delete(e);
			cleanup_parsers(parsers);
			return 0;
		}

		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>", input, parsers[MISP], &r))
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
