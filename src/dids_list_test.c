/*
 *
 * This program is designed to test the functions of the linked list.
 *
 *  ./dids_list_test
 *
 * 1) to add the miniature image into the SQL.
 *
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

// Standard
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Custom
#include "dids.h"

void show_list( PicInfo *list){
  printf("List is\n");
  while(list != NULL){
    printf("  '%s'\n", list->external_ref);
        list = list->next;
  }
}

int main(int argc, char *argv[]) {
    // These strings must be in order
    char *external_refs[] = { "ref_0", "ref_1", "ref_2", "ref_3", "ref_4" };
    printf("Start test\n");
    PicInfo *ref_0 = PicInfoBuild(external_refs[0], NULL, NULL);
    PicInfo *ref_1 = PicInfoBuild(external_refs[1], NULL, NULL);
    PicInfo *ref_2 = PicInfoBuild(external_refs[2], NULL, NULL);
    PicInfo *ref_3 = PicInfoBuild(external_refs[3], NULL, NULL);
    PicInfo *ref_4 = PicInfoBuild(external_refs[4], NULL, NULL);
    if (!ref_0 || !ref_1 || !ref_2 || !ref_3 || !ref_4) {
        printf("ERROR: PicInfoBuild - Failed. Quitting\n");
        exit(1);
    }
    // Add without rhythm, and you won't attract a worm.
    int error_count=0;
    PicInfo *list = NULL;
    FILE *sock_fh = stdout;
    PicInfoAddToList(sock_fh, &list, ref_1);
    PicInfoAddToList(sock_fh, &list, ref_0);
    PicInfoAddToList(sock_fh, &list, ref_4);
    PicInfoAddToList(sock_fh, &list, ref_3);
    PicInfoAddToList(sock_fh, &list, ref_2);

    // Test list is ordered
        int i = 0;
    for(PicInfo *ptr = list;ptr;ptr = ptr->next, i++) {
        if (strcmp(ptr->external_ref, external_refs[i])) {
            error_count++;
            printf("ERROR: At pos %d Expecting External ref='%s', but got '%s'\n", i,  external_refs[i], ptr->external_ref);
        }
    }

    // Delete start, middle and end
    if( PicInfoDeleteFromList(&list, external_refs[0])) {
        error_count++;
        printf("ERROR: failed to remove by ref '%s'\n", external_refs[0]);
    }
    if( PicInfoDeleteFromList(&list, external_refs[2])) {
        error_count++;
        printf("ERROR: failed to remove by ref '%s'\n", external_refs[2]);
    }
    if( PicInfoDeleteFromList(&list, external_refs[4])) {
        error_count++;
        printf("ERROR: failed to remove by ref '%s'\n", external_refs[4]);
    }
    // Check remaining list
    // 0 element.
    if (! list ){
        error_count++;
        printf("ERROR: list after removal didn't contain element 0.\n");
    }
    else if( strcmp(list->external_ref, external_refs[1])) {
        error_count++;
        printf("ERROR: list after removal. 0. external ref expected  '%s' but got '%s'\n",
                external_refs[1], list->external_ref);
    }
    // 1st element.
    if (! list || !(list->next) ){
        error_count++;
        printf("ERROR: list after removal didn't contain element 1.\n");
    }
    else if( strcmp(list->next->external_ref, external_refs[3])) {
        error_count++;
        printf("ERROR: list after removal. 1. external ref expected '%s' but got '%s'\n",
                external_refs[3], list->next->external_ref);
    }
    if (error_count){
        printf("ERROR: There were test failures.\n");
    }
    else{
        printf("INFO: End test. All tests passed.\n");
    }
    exit(0);
}
