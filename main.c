#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <editline/history.h>

#define VERSION_INFO "0.0.1"

int main()
{
	puts("Misp Version " VERSION_INFO);
	puts("Press Ctrl+C to exit\n");

	{
		volatile bool loop_prompt = true;
		while (loop_prompt)
		{
			char* input = readline("Misp>>> ");
			add_history(input);

			printf("> %s\n", input);

			free(input);
		}
	}

	return 0;
}
