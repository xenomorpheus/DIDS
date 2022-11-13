#include <stdio.h>
#include <stdarg.h>
#include "dids.h"
/*
 * Print error to socket.
 * Similar to fprintf except all lines printed will be prefaced with "ERROR: ".
 */

void error(FILE *sock_fh, const char *format, ...) {
   char buffer[BUFFER_SIZE];
   va_list args;

   va_start(args, format);
   vsnprintf(buffer, (sizeof buffer) - 1, format, args);
   va_end(args);

   fprintf(sock_fh, "ERROR: %s\n", buffer);
   fflush(sock_fh);
}

/*
 * Print debug to socket.
 * Similar to fprintf except all lines printed will be prefaced with "DEBUG: ".
 */

void debug(FILE *sock_fh, const char *format, ...) {
   char buffer[BUFFER_SIZE];
   va_list args;

   va_start(args, format);
   vsnprintf(buffer, (sizeof buffer) - 1, format, args);
   va_end(args);

   fprintf(sock_fh, "DEBUG: %s\n", buffer);
   fflush(sock_fh);
}
