#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

void error(int status, const char *format, ...) {
  va_list var_arg;
  char buf[ERR_BUF_SIZE];

  va_start(var_arg, format);
  vsnprintf(buf, ERR_BUF_SIZE, format, var_arg);
  va_end(var_arg);

  perror(buf);

  exit(status);
}
