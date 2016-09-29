/*
 *
 * This program is designed to test the functions of the image
 * comparison software.
 * 
 * Usage:
 *
 *  ./server_image_test <SQL_INFO> <IMAGE_FILENAME>
 *
 *  ./server_image_test "dbname = 'myTestDatabase' user = 'myTestSqlUser' connect_timeout = '10'" myTestImage.jpg
 *
 * 1) to add the miniature image into the SQL.
 *
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

#define COMPARE_SIZE  16
#define COMPARE_TRESHOLD 50000

// Standard
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libpq-fe.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <wand/MagickWand.h>

// Custom
#include "ppm.h"

void usage(FILE *sock_fh, char *appname) {
	fprintf(sock_fh, "\n");
	fprintf(sock_fh, "Usage:\n\n");
	fprintf(sock_fh, "  %s <SQL_INFO> <IMAGE_FILENAME>\n", appname);
	fprintf(sock_fh, "\n");
	fprintf(sock_fh, "  %s \"dbname = 'myTestDatabase' user = 'myTestSqlUser' connect_timeout = '10'\" myTestImage.jpg\n", appname);
	fprintf(sock_fh, "\n");
}

int main(int argc, char *argv[]) {
	int compare_size = COMPARE_SIZE;
	unsigned int maxerr = COMPARE_TRESHOLD;
	FILE *sock_fh = stdout;
	if (argc < 3) {
		fprintf(stderr, "ERROR: Not enough arguments. Quitting\n");
		usage(sock_fh, argv[0]);
		exit(1);
	}
	char *sql_info = argv[1];
	char *filename = argv[2];

	// Connect to SQL database
	PGconn *psql = ppm_sql_connect(sock_fh, sql_info);
	if (!psql) {
		fprintf(sock_fh,
				"ERROR: libpq error: PQconnectdb returned NULL.\nSQL details: %s\nQuitting\n",
				sql_info);
		exit(1);
	}

	MagickWandGenesis();

	PPM_Info *ppm = ppm_miniature_from_filename(sock_fh, filename,
			compare_size);
	if (!ppm) {
		ppm_sql_disconnect(sock_fh, psql);
		fprintf(sock_fh, "ERROR: ppm_miniature_from_filename - Failed to convert image to a PPM. Quitting.\n");
		exit(1);
	}
	fprintf(sock_fh,"SUCCESS: ppm_miniature_from_filename.\n");

	PicInfo *hlp = PicInfoBuild("ref 1", ppm, NULL);
	if (!hlp) {
		fprintf(sock_fh, "ERROR: Build - Failed. Quitting\n");
		exit(1);
	}
	
	// Add an image with id "ref 1"
	PicInfo *list = NULL;
	PicInfoAddToList(&list, hlp);

	// Look for the same ppm image as we have just added to the list.
	// Of course it should find the image in the list.
	PicInfo *pic = PicInfoBuild("ref 2", ppm,NULL);
	PicInfo *closest = CompareToList(sock_fh, pic, list, maxerr);
	if (closest) {
		fprintf(sock_fh, "SUCCESS: CompareToList - We found a similar image.\n");
	} else {
		fprintf(sock_fh, "ERROR: CompareToList - Failed to find a similar image.\n");
	}

    // Store the ppm in the SQL database
	int status = ppm_store(sock_fh, psql, "ref 1", ppm);
	if (status != 0) {
		fprintf(sock_fh, "ERROR: ppm_store - Failed with code %d. Quitting\n", status);
		ppm_sql_disconnect(sock_fh, psql);
		exit(1);
	}
	fprintf(sock_fh, "SUCCESS: ppm_store - Storing PPM in SQL.\n");

	// Finished testing
	MagickWandTerminus();
	fprintf(sock_fh, "INFO: disconnecting SQL\n");
	ppm_sql_disconnect(sock_fh, psql);
	fprintf(sock_fh, "INFO: End of Test\n");
	exit(0);
}

