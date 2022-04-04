#include "parsing.h"

#include <assert.h>

#define len(array) (sizeof (array) / sizeof *(array))
#define for_range(var, start, stop) \
    for(int (var) = (start); (var) < (stop); ++(var))

#define PARSER_COUNT 8
static mpc_parser_t* lang_parsers[PARSER_COUNT];

#define ALL_PARSERS \
    lang_parsers[0], lang_parsers[1], lang_parsers[2], lang_parsers[3],\
    lang_parsers[4], lang_parsers[5], lang_parsers[6], lang_parsers[7]

void init_lang_parsers(void)
{
	struct
	{
		const char* name, * rule;
	} const parser_properties[] = {
		{ "number", " /[+-]?[0-9]+/ ", },
		{ "symbol", " /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/", },
		{ "string", " /\"(\\\\.|[^\"])*\"/ ", },
		{ "comment", " /;[^\\r\\n]*/ ", },
		{ "sexpr", " '(' <expr>* ')' ", },
		{ "qexpr", " '{' <expr>* '}' ", },
		{ "expr", " <number> | <symbol> | <string> |"
				  " <comment> | <sexpr> | <qexpr> ", },
		{ "misp", " /^/ <expr>* /$/ ", },
	};

	assert(PARSER_COUNT == len(parser_properties));

	static char language_grammar[512];

	for_range(i, 0, len(parser_properties))
	{
		lang_parsers[i] = mpc_new(parser_properties[i].name);

		char temp[256];

		sprintf(temp, " %s : %s ; ",
			parser_properties[i].name,
			parser_properties[i].rule);

		strcat(language_grammar, temp);
	}

	mpca_lang(MPCA_LANG_DEFAULT, language_grammar, ALL_PARSERS);
}

mpc_parser_t* get_lang_parser(void)
{
	return lang_parsers[PARSER_COUNT - 1];
}

void cleanup_lang_parsers(void)
{
	mpc_cleanup(PARSER_COUNT, ALL_PARSERS);
}