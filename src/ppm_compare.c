/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module is used to compare sets of PPM images.
 * This module uses multi-threaded code.
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

#define FULLCOMPARE_REPORT_COMPARE_INTERVAL 5000
#define FULLCOMPARE_THREAD_COUNT     8

// Standard
#include <pthread.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Custom
#include "dids.h"

// fullcompare

struct fullcompare_thread_data {
    int thread_id;
    FILE *sock_fh;
    unsigned int maxerr;
};

pthread_mutex_t fullcompare_mutex;

PicInfo *fullcompare_worklist;

// How many image comparison sets will be done.
unsigned long long fullcompare_set_count;

// How many image comparison sets remain to do.
unsigned long long fullcompare_set_count_remaining;

// How many image comparisons we are expecting to do.
long double fullcompare_compare_total;

// How many image comparison we have repored
long double fullcompare_compare_remaining_reported;

/*
 *  fullcompare_set_work_list
 *  Set the working list to do a full compare on.
 *
 *  list - the list of thumb nails to process
 */

void fullcompare_set_work_list(PicInfo *list) {
    pthread_mutex_lock(&fullcompare_mutex);
    fullcompare_worklist = list;

    // count how many work items.
    fullcompare_set_count = 0L;
    PicInfo *current_pic = list;
    while (current_pic) {
        fullcompare_set_count++;
        current_pic = current_pic->next;
    }
    fullcompare_set_count_remaining = fullcompare_set_count;
    // If there are N images, then there will be N-1 image sets to compare.
    // Each image set of will compare one less than the number of images in the set.
    // Hint: consider an Nx(N-1) grid and the triangle formed by comparing N images, with the remaining N-1 images, only once.
    fullcompare_compare_total = fullcompare_set_count
            * (fullcompare_set_count - 1) / 2;
    // We ensure we report the 100% by increasing the fullcompare_compare_remaining_reported over the INTERVAL.
    fullcompare_compare_remaining_reported = fullcompare_compare_total + FULLCOMPARE_REPORT_COMPARE_INTERVAL + 1;
    pthread_mutex_unlock(&fullcompare_mutex);
}

/*
 * fullcompare_get_work_item
 * give a work unit to a worker thread.
 *
 * sock_fh - error channel
 *
 * Return
 *    a list of thumb nails to process.
 *    NULL if no work remaining to process.
 *
 */

PicInfo *
fullcompare_get_work_item(FILE *sock_fh) {
    PicInfo *ret = NULL;
    pthread_mutex_lock(&fullcompare_mutex);
    if (fullcompare_worklist) {

        // Get a work item.
        ret = fullcompare_worklist;
        fullcompare_worklist = fullcompare_worklist->next;


        // Hint consider an NxN grid and the triangle formed by comparing any two images exactly once.
        long double fullcompare_compare_remaining =
            fullcompare_set_count_remaining * (fullcompare_set_count_remaining - 1) / 2;

            if ( (fullcompare_compare_remaining_reported - fullcompare_set_count_remaining) > FULLCOMPARE_REPORT_COMPARE_INTERVAL ){
            fullcompare_compare_remaining_reported = fullcompare_set_count_remaining;

            long double percent_complete = 100.0
                - (fullcompare_compare_remaining / fullcompare_compare_total * 100);
            fprintf(sock_fh,
                "fullcompare_progress: %6.2Lf%% complete, sets remaining=%llu/%llu\n",
                percent_complete, fullcompare_set_count_remaining,
                fullcompare_set_count);
            fflush(sock_fh);
        }

        // Reduce the set count
        --fullcompare_set_count_remaining;
    }
    else {
        fprintf(sock_fh, "fullcompare_progress: 100.00%% complete\n");
        fflush(sock_fh);
    }
    pthread_mutex_unlock(&fullcompare_mutex);
    return ret;
}

/*
 * fullcompare_worker
 * A worker thread for comparing lists of images.
 *
 */

void *fullcompare_worker(void *threadarg) {
    struct fullcompare_thread_data *my_data =
            (struct fullcompare_thread_data *) threadarg;
    int thread_id = my_data->thread_id;
    FILE *sock_fh = my_data->sock_fh;
    unsigned int maxerr = my_data->maxerr;

    debug(sock_fh, "fullcompare_worker: Start %d", thread_id);
    fflush(sock_fh);
    PicInfo *current_pic;
    while ((current_pic = fullcompare_get_work_item(sock_fh)) && current_pic->next) {
        CompareToList(sock_fh, current_pic, current_pic->next, maxerr);
    }
    debug(sock_fh, "fullcompare_worker: Stop %d", thread_id);
    fflush(sock_fh);
    pthread_exit(NULL);
}

/*
 * fullcompare
 *
 * sock_fh     - error channel
 * full_list   - The list of thumbnails to look for possible duplicates within.
 * maxerr      - If the difference between two thumbnails is lower than maxerr, the files are considered similar.
 *
 * Return 0        on success.
 *        non-zero on error.
 */

int fullcompare(FILE *sock_fh, PicInfo *full_list, unsigned int maxerr,
        int thread_count) {

    if (full_list == NULL) {
        fprintf(sock_fh, "ERROR: fullcompare passed empty list\n");
        fflush(sock_fh);
        return 2;
    }

    // Set up work to do.
    fullcompare_set_work_list(full_list);

    // Set up threads
    struct fullcompare_thread_data thread_data_array[thread_count];
    pthread_t threads[thread_count];
    int thread_id;
    for (thread_id = 0; thread_id < thread_count; thread_id++) {
        debug(sock_fh, "In main: creating thread %d", thread_id);
        fflush(sock_fh);
        thread_data_array[thread_id].thread_id = thread_id;
        thread_data_array[thread_id].sock_fh = sock_fh;
        thread_data_array[thread_id].maxerr = maxerr;

        int rc = pthread_create(&threads[thread_id], NULL, fullcompare_worker,
                (void *) &thread_data_array[thread_id]);
        if (rc) {
            fprintf(sock_fh, "ERROR: return code from pthread_create() is %d\n",
                    rc);
            fflush(sock_fh);
            return 1;
        }
    }
    // wait for threads.
    for (thread_id = 0; thread_id < thread_count; thread_id++) {
        debug(sock_fh, "In main: thread %d finished", thread_id);
        fflush(sock_fh);
        pthread_join(threads[thread_id], NULL);
    }
    return 0;
}

/*
 *   compare an image to the list
 *
 *   return
 *       closest ppm that is below maxerr
 *       otherwise NULL.
 *
 *   side effects
 *       report images under maxerr. Used by fuzzy duplicate processing.
 */
// TODO consider adding a flag for reporting all matches under maxerr, not just the best.
// When flag set then don't use err_best_so_far in call to PPM_compare, use maxerr

PicInfo *CompareToList(FILE *sock_fh, PicInfo *pic, PicInfo *picinfo_list, unsigned int maxerr) {

    // Some quick sanity checks
    if (!picinfo_list) {
        fprintf(sock_fh, "ERROR: CompareToList picinfo_list is NULL\n");
        fflush(sock_fh);
        return NULL;
    }
    if (!pic) {
        fprintf(sock_fh, "ERROR: CompareToList pic is NULL\n");
        fflush(sock_fh);
        return NULL;
    }
    if (maxerr == 0) {
        fprintf(sock_fh, "ERROR: CompareToList maxerr is zero\n");
        fflush(sock_fh);
        return NULL;
    }

    /*
     *   Start comparing to all other pictures
     */
    char *external_ref = pic->external_ref;
    unsigned int err_this_compare;
    unsigned int err_best_so_far = UINT_MAX;
    PicInfo *best_match = NULL;

    // Even when we can't find a better match we keep processing because we want
    // to supply a list of close matches to fuzzy duplicate processing.
    while (picinfo_list) {
        // TODO replace err_best_so_far in next line with maxerr if we TRUELY want to
        // find all similar images under maxerr.
        err_this_compare = PPM_compare(sock_fh, pic->picinf, picinfo_list->picinf, err_best_so_far);

        // If this compare is closer than maxerr AND better than any previous comparisons.
        if (err_this_compare < maxerr){

            /*
            *   As we want DIDS to report close matches, DIDS needs to
            *   ignore 'similar_but_different' cases at the point it decides if
            *   to compare two images. Waiting until later and weeding out the
            *   results won't work as the next closest image won't be reported.
            */
            Similar_but_different *sbd = similar_but_different_search(pic->similar_but_different, picinfo_list->external_ref);
            if (sbd) {
                debug(sock_fh, "ignoring previous similar_but_different: %s, %s", external_ref, sbd->external_ref);
                fflush(sock_fh);
                picinfo_list = picinfo_list->next;
                continue;
            }

            fprintf(sock_fh, "Match: %s, %s, %u\n", external_ref, picinfo_list->external_ref, err_this_compare);
            fflush(sock_fh);

            if (err_this_compare < err_best_so_far) {
                err_best_so_far = err_this_compare;
                best_match = picinfo_list;
            }

        }
        picinfo_list = picinfo_list->next;
    }
    return best_match;
}

/*
 * quickcompare
 *
 * Look for the similar files in the database.
 * The supplied file WONT be added to the database.
 * Return 0 on success
 */

int quickcompare(FILE *sock_fh, PicInfo *picinfo_list, unsigned int maxerr, char *filename, char *external_ref,
    int compare_size) {

    int result = access (filename, R_OK); // for readable
    if ( result != 0 ){
        fprintf(sock_fh, "ERROR: quickcompare - no read access for filename '%s'\n", filename);
        fflush(sock_fh);
        return 1;
    }
    PPM_Info *ppm_miniature = ppm_miniature_from_filename(sock_fh, filename, compare_size);
    if (!ppm_miniature) {
        fprintf(sock_fh, "ERROR: quickcompare - ppm_miniature_from_filename filename %s failed\n",
                filename);
        fflush(sock_fh);
        return 1;
    }

    // Compare to existing PPMs in list
    debug(sock_fh, "quickcompare calling CompareToList with filename '%s'", filename);
    debug(sock_fh, "quickcompare maxerr %u, external ref '%s'", maxerr, external_ref);
    fflush(sock_fh);
    PicInfo *pic = PicInfoBuild(external_ref, ppm_miniature, NULL);
    CompareToList(sock_fh, pic, picinfo_list, maxerr);

    ppm_info_free(ppm_miniature);
    debug(sock_fh, "quickcompare done");
    fflush(sock_fh);
    return 0;
}
