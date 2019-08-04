
#include <stdio.h>
#include <stdarg.h>

#include "StormSocketLog.h"

void StormSocketLog(const char * fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vprintf (fmt, args);
  va_end (args);
}

