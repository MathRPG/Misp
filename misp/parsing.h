#pragma once

#include "../mpc/mpc.h"

void init_lang_parsers(void);
mpc_parser_t* get_lang_parser(void);
void cleanup_lang_parsers(void);