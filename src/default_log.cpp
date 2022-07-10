#include "internal.hpp"
#include <cstdio>

namespace woof {

static const char *level_str[] { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL" };

void
default_log(LogLevel level, const char *str, size_t len)
{
	printf("[%s] %*s\n", level_str[int(level)], int(len), str);
}

}
