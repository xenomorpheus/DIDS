/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module provides a linked list of PPMs.
 * Please see the README file for further details.
 *
 *    Copyright (C) 2000-2021  Michael John Bruins, BSc.
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
#include "ppm.h"

/*
 * build a PicInfo object and assign the properties.
 * Note: external_ref is used as passed, not duplicated.
 */
PicInfo *PicInfoBuild(char *external_ref, PPM_Info *pic,
		Similar_but_different *similar_but_different) {
	PicInfo *hlp = (PicInfo *) malloc(sizeof(PicInfo));
	if (!hlp) {
		return NULL;
	}
	hlp->external_ref = external_ref;
	hlp->picinf = pic;
	hlp->similar_but_different = similar_but_different;
	return hlp;
}

/*
 * Delete a PicInfo object and similiar_but_different details.
 */
void PicInfoDelete(PicInfo *hlp) {
	if (!hlp) {
		return;
	}
	Similar_but_different *similar_but_different = hlp->similar_but_different;
	Similar_but_different *last_similar_but_different = NULL;
	while (similar_but_different) {
		last_similar_but_different = similar_but_different;
		similar_but_different = last_similar_but_different->next;
		free(last_similar_but_different);
	}
	if (hlp->picinf) {
		ppm_info_free(hlp->picinf);
	}
	free(hlp);
}

/*
 * add the picture info to the  linked list.
 * Linked list is sorted in order of assending external_ref.
 *
 * Example:
 * AddToList(list, external_ref, picinfo);
 *
 */
// TODO insert to form a sorted list
// TODO We need to avoid searching through the entire list each time we insert.
// TODO Can we do something smart with the return value so we are always
// adding the the end of the list ?
// E.g. return the end of the list so that we can insert at the end.
void PicInfoAddToList(PicInfo **list_ref, PicInfo *picinfo_new) {

	// Insert so that external_ref strings are in ascending order.
	// Is the list empty?
	if (*list_ref == NULL) {
		*list_ref = picinfo_new;
		picinfo_new->next = NULL;
		return;
	}

	// Find the point in the chain where we need to insert this link.
	char *external_ref_new = picinfo_new->external_ref;
	PicInfo *ptr = *list_ref;
	PicInfo *last_ptr = NULL;
	while (ptr && strcmp(ptr->external_ref, external_ref_new) < 0) {
		last_ptr = ptr;
		ptr = ptr->next;
	}
        // insert at start
        if (last_ptr == NULL){
		picinfo_new->next = *list_ref;
		*list_ref = picinfo_new;
        }
	// insert at last_ptr
        else {
		last_ptr->next = picinfo_new;
		picinfo_new->next = ptr;
        }
}

/*
 * delete the PicInfo from list.
 *
 * PicInfoDelFromList(sock_fh, list, external_ref);
 *
 * Note: Will free the memory of the PicInfo it Deletes
 *
 * return
 *   0 - success
 *   1 - failed to find PicInfo with external_ref
 *
 */
int PicInfoDeleteFromList(PicInfo **list_ref, char *external_ref) {

	// Find the point in the chain where we need to insert this link.
	PicInfo *ptr = *list_ref;
	PicInfo *last_ptr = NULL;
	while (ptr && strcmp(  ptr->external_ref, external_ref  ) < 0) {
		last_ptr = ptr;
		ptr = ptr->next;
	}

	// Check to see if we fell off the end
	if (!ptr) {
		return 1;
	}

	// Wasn't in list. We went past where it should be
	if (strcmp(ptr->external_ref, external_ref) ) {
		return 2;
	}

	// At this point we have found link to delete

	// Check if it is the first link
	if (last_ptr) {
		last_ptr->next = ptr->next;
	} else {
		// update head if removed link was first link in list.
		*list_ref = ptr->next;
	}
	// Free the memory
	PicInfoDelete(ptr);
    return 0;
}

/*
 * add the external ref of the 'similar but different' 
 */

// Similar_but_different *similar_but_different;
