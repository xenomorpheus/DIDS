/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module is used to put and get PPM details from a database.
 * Please see the README file for further details.
 *
 *    Copyright (C) 2000-2019  Michael John Bruins, BSc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <stdarg.h>
#include "ppm.h"

/*


 CREATE SEQUENCE id_ppm_seq;



 CREATE TABLE dids_ppm (
 id integer CONSTRAINT ppm_id_pk PRIMARY KEY default nextval('id_ppm_seq'::regclass),
 width integer not null,
 height integer not null,
 hexdata text,
 created timestamp not null default now(),
 modified timestamp not null default now(),
 external_ref text not null,
 constraint external_ref references direntry(fingerprint),
 unique (external_ref)
 );


 */

/*
 * ppm_to_hexdata
 *
 * convert the ppm image into a hex string.
 * e.g. so a ppm can be stored in an SQL database.
 *
 * Note: free the result when you are finished with it.
 *
 * On success return a string of hex digits.
 * On failure returns NULL;
 *
 */

char *ppm_to_hexdata(PPM_Info *ppm) {
	char *hexdata = NULL;
	int x, y, h, w, offset;
	Color c;
	char hexchar[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A',
			'B', 'C', 'D', 'E', 'F' };

	h = ppm->height;
	w = ppm->width;
	// allocate 6 characters for each pixel i.e. two hex chars per each R,G and B.
	// plus one for the string terminator
	hexdata = (char *) malloc((6 * w * h) + 1);
	if (!hexdata) {
		fprintf(stderr, "Error: failed to allocate memory for ppm_to_hexdata");
		return NULL;
	}
	hexdata[6 * w * h] = '\0';
	// Set the hex chars that correspond to each pixel.
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			PPM_GetPixel(ppm, x, y, &c);
			offset = 6 * ((y * w) + x);
			hexdata[offset + 0] = hexchar[((unsigned int) c.r) / 16];
			hexdata[offset + 1] = hexchar[((unsigned int) c.r) % 16];
			hexdata[offset + 2] = hexchar[((unsigned int) c.g) / 16];
			hexdata[offset + 3] = hexchar[((unsigned int) c.g) % 16];
			hexdata[offset + 4] = hexchar[((unsigned int) c.b) / 16];
			hexdata[offset + 5] = hexchar[((unsigned int) c.b) % 16];
		}
	}
	return hexdata;
}

/*
 * xtod : Hex to decimal conversion
 *
 */
int xtod(char c) {
	return ((c >= '0' && c <= '9') ?
			c - '0' :
			((c >= 'A' && c <= 'F') ?
					c - 'A' + 10 : ((c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0)));
}

/*
 * hexdata_to_ppm
 *
 * convert the ppm hex-string into plain ppm data.
 * e.g. retrieve a ppm from an SQL database.
 *
 * Note: You are responsible for attaching the data to the ppm.
 *       free the result when you are finished with it.
 *
 * On success returns ppm data.
 * On failure returns NULL;
 *
 * Limitation : hexstring must only contain hex characters.
 *   No errors will be throw for invalid input data.
 */

void hexdata_to_ppm(PPM_Info *ppm, char *hexstring) {
	int x, y, h, w, offset;
	Color c;

	h = ppm->height;
	w = ppm->width;
	// Set the hex chars that correspond to each pixel.
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			offset = 6 * ((y * w) + x);
			// red
			c.r = 16 * xtod(hexstring[offset + 0])
					+ xtod(hexstring[offset + 1]);
			// green
			c.g = 16 * xtod(hexstring[offset + 2])
					+ xtod(hexstring[offset + 3]);
			// blue
			c.b = 16 * xtod(hexstring[offset + 4])
					+ xtod(hexstring[offset + 5]);
			PPM_SetPixel(ppm, x, y, c);
		}
	}
	return;
}

/*
 * Store ppm image in database
 *
 * sock_fh      - error channel
 *
 * return 0 on success
 * return non-zero on failure.
 *
 */

int ppm_store(FILE *sock_fh, PGconn *psql, char *external_ref, PPM_Info *ppm) {
	PGresult *result;

	// TODO: use a binary storage type in postgres
	char *hexdata = ppm_to_hexdata(ppm);
	if (!hexdata) {
		fprintf(sock_fh, "ppm_to_hexdata failed for external_ref %s",
				external_ref);
		return 1;
	}

	result =
			pq_query(psql,
					"INSERT INTO dids_ppm (width,height,hexdata,external_ref) values (%d,%d,'%s','%s');",
					ppm->width, ppm->height, hexdata, external_ref);

	free(hexdata);

	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		fprintf(sock_fh, "ppm_store: libpq command failed: %s",
				PQerrorMessage(psql));
		return 1;
	}
	PQclear(result);
	return 0; //success
}

/*
 * Delete ppm image from database
 *
 * sock_fh      - error channel
 *
 * return 0 on success
 * return non-zero on failure.
 *
 */

int ppm_del(FILE *sock_fh, PGconn *psql, char *external_ref) {
	PGresult *result = pq_query(psql,
			"DELETE FROM dids_ppm WHERE external_ref = '%s';", external_ref);

	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		fprintf(sock_fh, "ppm_store: libpq command failed: %s",
				PQerrorMessage(psql));
		return 1;
	}
	PQclear(result);
	return 0; //success
}

/*
 * tuple_to_ppm
 *
 * Convert an SQL tuple to a ppm image in memory.
 *
 * This routine can be repeatedly called on each tuple
 * of a select.  Be careful of memory used in your select!
 * Hint : use an SQL cursor.
 *
 * sock_fh      - error channel
 *
 *
 */

PPM_Info *tuple_to_ppm(FILE *sock_fh, PGresult *result, int tuple) {
	PPM_Info *ppm;
	int width_fnum, height_fnum, hexdata_fnum;
	char *hexdata_ptr;
	unsigned char *ppm_colour_data = NULL;
	int col, hextext_len;

	// create a ppm ready to populate
	ppm = (PPM_Info *) malloc(sizeof(PPM_Info));
	if (!ppm) {
		fprintf(sock_fh,
				"ERROR: tuple_to_ppm: failed to malloc memory for ppm for tuple %d",
				tuple);
		return NULL;
	}

	/* Use PQfnumber to avoid assumptions about field order
	 in result */
	width_fnum = PQfnumber(result, "width");
	height_fnum = PQfnumber(result, "height");
	hexdata_fnum = PQfnumber(result, "hexdata");

	/* Get the field values (we ignore possibility they are
	 null!) */
	/* The binary representation of INT4 is in network byte
	 order, which we'd better coerce to the local byte
	 order. */

	ppm->width = atol(PQgetvalue(result, tuple, width_fnum));
	ppm->height = atol(PQgetvalue(result, tuple, height_fnum));

	hexdata_ptr = PQgetvalue(result, tuple, hexdata_fnum);

	/* The binary representation of TEXT is, well, text,
	 and since libpq was nice enough to append a zero
	 byte to it, it'll work just fine as a C string. The
	 binary representation of BYTEA is a bunch of bytes,
	 which could include embedded nulls so we have to pay
	 attention to field length. */
	hextext_len = PQgetlength(result, tuple, hexdata_fnum);

	if ((ppm->height * ppm->width * 6) != hextext_len) {
		int external_ref_fnum = PQfnumber(result, "external_ref");
		char *external_ref = PQgetvalue(result, tuple, external_ref_fnum);

		fprintf(stderr,
				"tupl_to_ppm: data length mis-match. external_ref=%s, hextext_len=%d, height=%d, width=%d, other=%d\n",
				external_ref, hextext_len, ppm->height, ppm->width,
				ppm->height * ppm->width * 6);
		return NULL;
	}

	ppm->modval = ppm->width * 3;

	// allocate 3 bytes for each pixel i.e. one byte per each R,G and B.
	ppm_colour_data = (unsigned char *) malloc(3 * ppm->width * ppm->height);
	if (!ppm_colour_data) {
		fprintf(stderr, "Error: failed to allocate memory for hexdata_to_ppm");
		return NULL;
	}
	ppm->data = ppm_colour_data;

	// decode the image pixel from hexdata.
	hexdata_to_ppm(ppm, hexdata_ptr);
	return ppm; //success
}

/*
 * Load miniature image from database
 *
 * sock_fh      - error channel
 *
 * return ppm on success, null otherwise.
 *
 */

PPM_Info *ppm_load_from_sql(FILE *sock_fh, PGconn *psql, char *external_ref) {
	PQprintOpt options = { 0 };
	PPM_Info *ppm;

	PGresult *result =
			pq_query(psql,
					"SELECT external_ref,width,height,hexdata FROM dids_ppm where external_ref = '%s'",
					external_ref);

	if ((PQresultStatus(result) != PGRES_COMMAND_OK)
			&& (PQresultStatus(result) != PGRES_TUPLES_OK)) {
		fprintf(sock_fh,
				"ppm_load_from_sql: libpq command failed: Status: %s\n",
				PQresStatus(PQresultStatus(result)));
		fprintf(sock_fh, "ppm_load_from_sql: libpq command failed: Error: %s\n",
				PQresultErrorMessage(result));
		PQclear(result);
		return NULL;
	}

	// No ppm for this external_ref
	if (PQntuples(result) == 0) {
		PQclear(result);
		return NULL;
	}

	ppm = tuple_to_ppm(sock_fh, result, 0);
	PQclear(result);

	if (!ppm) {
		fprintf(stderr,
				"ppm_load_from_sql: failed to malloc memory for ppm for external_ref='%s'",
				external_ref);
		return NULL;
	}
	return ppm;
}

/*
 * ppm_load_all_from_sql
 *
 * Read in all the PPM from SQL, one at a time.
 *
 * Return 0 on success
 *        non-zero on failure.
 */
int ppm_load_all_from_sql(FILE *sock_fh, PGconn *psql,
		PicInfo **picinfo_list_ref) {

	PGresult *pq_result = pq_query(psql,
			"SELECT * FROM dids_ppm order by external_ref;");

	if ((PQresultStatus(pq_result) != PGRES_COMMAND_OK)
			&& (PQresultStatus(pq_result) != PGRES_TUPLES_OK)) {
		fprintf(sock_fh,
				"ERROR: ppm_load_all_from_sql: libpq command failed: %s\n",
				PQerrorMessage(psql));
		return 1;
	}

	// No images - success
	int tuple, tuples;
	if (!(tuples = PQntuples(pq_result))) {
		PQclear(pq_result);
		return 0;
	}
	// Get the field number
	int external_ref_fnum = PQfnumber(pq_result, "external_ref");

	// Fetch the tuples
	PicInfo *last_added = NULL;
	for (tuple = 0; tuple < tuples; tuple++) {

		// TODO iterate over list of external_ref values.
		char * external_ref = PQgetvalue(pq_result, tuple, external_ref_fnum);

		// Clone Postgres' private memory as it may change.
		external_ref = strdup(external_ref);

		// LOAD PPM
		PPM_Info *ppm = tuple_to_ppm(sock_fh, pq_result, tuple);
		if (!ppm) {
			fprintf(sock_fh,
					"ERROR: ppm_load_from_sql failed in ppm_load_all_from_sql for external_ref='%s'",
					external_ref);
			return 3;
		}
		PicInfo *hlp = PicInfoBuild(external_ref, ppm, NULL);
		// Only fails if out of memory, so we return.
		if (!hlp) {
			fprintf(sock_fh, "ERROR: PicInfoBuild failed\n");
			return 4;
		}
		if (last_added) {
			PicInfoAddToList(&last_added, hlp);
		} else {
			PicInfoAddToList(picinfo_list_ref, hlp);
		}
        last_added = hlp;
	}
	// Cleanup
	PQclear(pq_result);

	// Success
	return 0;
}

