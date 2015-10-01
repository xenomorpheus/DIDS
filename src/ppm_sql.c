/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module provides connectivity to an SQL database.
 * Please see the README file for further details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <stdarg.h>
#include "ppm.h"

/*
 * ppm_sql_connect
 *
 * return null on failure.
 *
 */

PGconn *ppm_sql_connect(FILE *sock_fh, char *sql_info) {

	// Connect to SQL database
	PGconn *psql = PQconnectdb(sql_info);
	if (!psql) {
		fprintf(sock_fh, "libpq error: PQconnectdb returned NULL.\nSQL: %s\n",
				sql_info);
		return NULL;
	}
	if (PQstatus(psql) != CONNECTION_OK) {
		fprintf(sock_fh, "libpq error: %s", PQerrorMessage(psql));
		return NULL;
	}
	return psql;
}

/*
 * ppm_sql_disconnect (sock_fh, psql);
 *
 */

void ppm_sql_disconnect(FILE *sock_fh, PGconn *psql) {
	PQfinish(psql);

}

/*
 * pq_query
 *
 * Wrapper function for PQexec to allow arbitrary number of arguments
 */

PGresult *
pq_query(PGconn *psql, const char *format, ...) {
	va_list argv;
	char *ptrQuery;
	PGresult *result;
	va_start(argv, format);
	vasprintf(&ptrQuery, format, argv);
	va_end(argv);
	if (!ptrQuery)
		return (0);
	result = PQexec(psql, ptrQuery);
	free(ptrQuery);
	return (result);
}

