#include <stdio.h>
#include <stdarg.h>
#include "log.h"

#define BUF_MAX_SIZE	(512)
#define NONE			("\033[0m")
#define RED				("\033[0;32;31m")

void log_message(log_level level, char *fmt, ...)
{
	char buf[BUF_MAX_SIZE] = {0};
	va_list args;
	static char *level_str[LOG_LEVEL_NUM] = {
			"INFO:",
			"WARM:",
			"ERR:",
			"DEBUG:",
		};
	static char *colors[LOG_LEVEL_NUM] = {
			"",
			"",
			RED,
			""
		};

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	printf("%s%s\t%s\n%s", colors[level], level_str[level], buf, NONE);
	va_end(args);
}


