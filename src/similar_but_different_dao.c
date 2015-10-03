/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module provides a mechanism to stop false positives from being reported again.
 * Please see the README file for further details.
 *
 *    Copyright (C) 2000-2015  Michael John Bruins, BSc.
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
 * similar_but_different_add_to_picinfo_list
 *
 * Add similar_but_different information to the entries in the picinfo_list.
 *
 * IMORTANT REQUIREMENT:
 *   external_ref < external_ref_other
 *      This restriction permits faster processing in this routine and in the duplicate detection code.
 *      The comparison logic will only compare the current image to an image with a higher value for external_ref.
 *      This approach means there is only the need to record the image relationship on one of the images, not both.
 *      This halves the number of image relationships to search through.
 *
 * Return 0 on success
 *        non-zero on failure.
 */
int similar_but_different_add_to_picinfo_list(FILE *sock_fh, PGconn *psql,
		PicInfo *picinfo_list_ref) {

	if (!picinfo_list_ref) {
		return 4;
	}
	if (!psql) {
		return 3;
	}
	if (!sock_fh) {
		return 2;
	}

	// By having the external_ref results ordered, and also the external_ref < external_ref_other:
	// 1) This subroutine execution path becomes a one-pass operation.
	// 2) The image comparison logic needs to do less work
	PGresult *pq_result =
			pq_query(psql,
					"SELECT external_ref, external_ref_other FROM dids_similar_but_different ORDER BY external_ref;");

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

	/* Use PQfnumber to avoid assumptions about field order in result */
	int external_ref_fnum = PQfnumber(pq_result, "external_ref");
	int external_ref_other_fnum = PQfnumber(pq_result, "external_ref_other");
	int rc; // scoping - don't keep reallocating RAM
	for (tuple = 0; tuple < tuples; tuple++) {
		// don't reuse Postgres private memory
		char *external_ref = strdup(
				PQgetvalue(pq_result, tuple, external_ref_fnum));
		char *external_ref_other = strdup(
				PQgetvalue(pq_result, tuple, external_ref_other_fnum));

		// Fast forward to the picinfo entry for this external_ref, or
		// just after if there insn't a picinfo entry for this external_ref.
		while (picinfo_list_ref
				&& (strcmp(picinfo_list_ref->external_ref, external_ref) < 0)) {
			picinfo_list_ref = picinfo_list_ref->next;
		}

		// Update the picinfo entry with the external_ref_other to avoid.
		if (picinfo_list_ref
				&& strcmp(picinfo_list_ref->external_ref, external_ref) == 0) {
			rc = similar_but_different_add_to_picinfo(sock_fh, picinfo_list_ref,
					external_ref_other);
			external_ref_other = NULL; // picinfo_list_ref now owns this string.
			if (rc) {
				fprintf(sock_fh,
						"ERROR: similar_but_different_add_to_picinfo_list failed call to similar_but_different_add_to_picinfo, error code=%d\n",
						rc);
				return 2;
			}
		}
		if (external_ref) { // should always be defined, but we check anyway.
			free(external_ref);
		}
		if (external_ref_other) { // might be NULL if stored in picinfo_list_ref
			free(external_ref_other);
		}

	}
	// Cleanup
	PQclear(pq_result);

	// Success
	return 0;
}

/*
 * prepend to external_ref_other to the chain of 'similar but different' external refs.
 */

int similar_but_different_add_to_picinfo(FILE *sock_fh, PicInfo *picinfo,
		char *external_ref) {
	Similar_but_different *new_sbd_link = (Similar_but_different *) malloc(
			sizeof(Similar_but_different));
	if (!new_sbd_link) {
		fprintf(sock_fh,
				"ERROR: similar_but_different_add_to_picinfo failed to malloc\n");
		return 1;
	}
	new_sbd_link->external_ref = external_ref;
	// prepend to linked list
	new_sbd_link->next = picinfo->similar_but_different;
	picinfo->similar_but_different = new_sbd_link;
	return 0;
}

Similar_but_different *similar_but_different_search(
		Similar_but_different *sbd_ptr, char *external_ref) {
	while (sbd_ptr) {
		if (!strcmp(external_ref, sbd_ptr->external_ref)) {
			return sbd_ptr;
		}
		sbd_ptr = sbd_ptr->next;
	}
	return NULL;
}

