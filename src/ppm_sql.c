/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module provides connectivity to an SQL database.
 * Please see the README file for further details.
 *
 *    Copyright (C) 2000-2022  Michael John Bruins, BSc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <stdarg.h>
#include "dids.h"

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

