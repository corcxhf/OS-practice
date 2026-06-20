#include <stdlib.h>
#include <string.h>
#include "vi.h"

char *cmd_pipe(char *cmd, char *ibuf, int oproc)
{
	(void)cmd;
	(void)ibuf;
	(void)oproc;
	return NULL;
}

int cmd_exec(char *cmd)
{
	(void)cmd;
	return 1;
}

char *cmd_unix(char *path, char *ibuf)
{
	(void)path;
	(void)ibuf;
	return NULL;
}
