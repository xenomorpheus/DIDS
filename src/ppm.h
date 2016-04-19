/*
 * This is part of DIDS (Duplicate Image Detection System).
 * This module provides types and subroutine header definitions.
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
#include <libpq-fe.h>

typedef struct Color {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} Color;

/*
 *   PPM_Info struct : points to all needed info of a ppm.
 */
typedef struct PPM_Info {
	int width;
	int height;
	int modval;
	unsigned char *data;
} PPM_Info;

/*
 *
 *   Define a link in a linked list of 'Similar But Different'
 *   images.  E.g. Images that we will need to ignore
 *   false positive matches.
 */
typedef struct Similar_but_different {
	// The external ref of the image to ignore
	char *external_ref;
	// The next link in the chain
	struct Similar_but_different *next;
} Similar_but_different;

/*
 * Define a link in a single linked list of PPMs.
 */
typedef struct PicInfo {
	struct PicInfo *next;
	// What the external system uses to refer to this picture.
	char *external_ref;
	// A list of false positives we will need to ignore.
	Similar_but_different *similar_but_different;
	PPM_Info *picinf;
} PicInfo;

// ppm_info.c
PPM_Info *ppm_info_allocate(int width, int height);
void ppm_info_free(PPM_Info *ppm);

// ppm_sql.c
PGconn *ppm_sql_connect(FILE *sock_fh, char *sql_info);
void ppm_sql_disconnect(FILE *sock_fh, PGconn *psql);
PGresult *pq_query(PGconn *psql, const char *format, ...);

// ppm_dao.c
int xtod(char c);
char *ppm_to_hexdata(PPM_Info *ppm);
int ppm_store(FILE *sock_fh, PGconn *psql, char *external_ref, PPM_Info *ppm);
int ppm_del(FILE *sock_fh, PGconn *psql, char *external_ref);
PPM_Info *tuple_to_ppm(FILE *sock_fh, PGresult *result, int tuple);
PPM_Info *ppm_load_from_sql(FILE *sock_fh, PGconn *psql, char *external_ref);
int ppm_load_all_from_sql(FILE *sock_fh, PGconn *psql, PicInfo **list_ref);

// ppm_fullcompare.c
void fullcompare_set_work_list(PicInfo *list);
PicInfo *fullcompare_get_work_item(FILE *sock_fh);
void *fullcompare_worker(void *threadarg);
int fullcompare(FILE *sock_fh, PicInfo *full_list, unsigned int maxerr, int thread_count);
int quickcompare(FILE *sock_fh, PicInfo *list, unsigned int maxerr, char *filename, int compare_size);

// dids_server.c
PicInfo *CompareToList(FILE *sock_fh, PicInfo *pic, PicInfo *list, unsigned int maxerr);
int add(FILE *sock_fh, PGconn *psql, PicInfo **list_ref, char *filename,
		char *external_ref, int compare_size);
int del(FILE *sock_fh, PGconn *psql, PicInfo **Global_list_ref, char *external_ref);
void error(FILE *sock_fh, const char *fmt, ...);
int info(FILE *sock_fh, PicInfo *list);
int server_loop(FILE *orig_sock_fh, char *sql_info, int portno,
		int compare_size, unsigned int maxerr);

// ppm.c
PPM_Info *ppm_miniature_from_filename(FILE *sock_fh, char *filename, int compare_size);
int PPM_from_file(FILE *sock_fh, PPM_Info *ppm, char *fname);
unsigned int PPM_compare(FILE *sock_fh, PPM_Info *p1, PPM_Info *p2,
		unsigned int err_best_so_far);
void PPM_SetPixel(PPM_Info *ppm, int x, int y, Color c);
void PPM_SetBWPixel(PPM_Info *ppm, int x, int y, unsigned char l);
void PPM_GetPixel(PPM_Info *ppm, int x, int y, Color *c);
unsigned char PPM_GetBWPixel(PPM_Info *ppm, int x, int y, unsigned char *c);
void SetColor(Color *c, unsigned char r, unsigned char g, unsigned char b);

// ppm_list.c
PicInfo *PicInfoBuild(char *external_ref, PPM_Info *pic,Similar_but_different *similar_but_different);
void PicInfoDelete(PicInfo *pic);
void PicInfoAddToList(PicInfo **list_ref, PicInfo *hlp);
int PicInfoDeleteFromList(PicInfo **list_ref, char *external_ref);

// similar_but_different_dao.c
int similar_but_different_add_to_picinfo_list(FILE *sock_fh, PGconn *psql, PicInfo *picinfo_list_ref);
int similar_but_different_add_to_picinfo(FILE *sock_fh, PicInfo *picinfo, char *external_ref);
Similar_but_different *similar_but_different_search(Similar_but_different *sbd_ptr,char *external_ref);

