/*
 *
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module provides a miniature images (PPMs) used for image comparisons.
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
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <wand/MagickWand.h>

#include "ppm.h"

void PPM_SetPixel(PPM_Info *ppm, int x, int y, Color c) {
	if ((x < 0) || (x >= ppm->width)) {
		return;
	}

	if ((y < 0) || (y >= ppm->height)) {
		return;
	}

	ppm->data[ppm->modval * y + 3 * x] = c.r;
	ppm->data[ppm->modval * y + 3 * x + 1] = c.g;
	ppm->data[ppm->modval * y + 3 * x + 2] = c.b;
}

void PPM_SetBWPixel(PPM_Info *ppm, int x, int y, unsigned char l) {
	if ((x < 0) || (x >= ppm->width)) {
		return;
	}

	if ((y < 0) || (y >= ppm->height)) {
		return;
	}

	ppm->data[ppm->modval * y + 3 * x] = l;
	ppm->data[ppm->modval * y + 3 * x + 1] = l;
	ppm->data[ppm->modval * y + 3 * x + 2] = l;
}

/*
 * Get rgb color value of pixel
 */
void PPM_GetPixel(PPM_Info *ppm, int x, int y, Color *c) {
	if ((x < 0) || (x >= ppm->width)) {
		return;
	}
	if ((y < 0) || (y >= ppm->height)) {
		return;
	}

	c->r = ppm->data[ppm->modval * y + 3 * x];
	c->g = ppm->data[ppm->modval * y + 3 * x + 1];
	c->b = ppm->data[ppm->modval * y + 3 * x + 2];
}

/*
 * Get black and white pixel value
 */
unsigned char PPM_GetBWPixel(PPM_Info *ppm, int x, int y, unsigned char *c) {
	unsigned short v;

	if ((x < 0) || (x >= ppm->width)) {
		return 0;
	}
	if ((y < 0) || (y >= ppm->height)) {
		return 0;
	}

	v = (ppm->data[ppm->modval * y + 3 * x]
			+ ppm->data[ppm->modval * y + 3 * x + 1]
			+ ppm->data[ppm->modval * y + 3 * x + 2]) / 3;
	*c = v;

	return (*c);
}

/*
 * Compare a MatrixSize x MatrixSize picture on a rgb error basis
 *
 * Taken largely from PictureCompare.c
 *
 * sock_fh         - Print errors/warnings/info to this stream
 * p1              - PPM to compare with p2.
 * p2              - PPM to compare with p1.
 * err_best_so_far - Abort if error factor exceeds this number.
 *    There is no point in continuing as we have already found a closer match.
 *
 * Return:
 * The error factor between the two PPMs from each other.
 * 0 = identical.  The higher the number the more different the images.
 *
 */
unsigned int PPM_compare(FILE *sock_fh, PPM_Info *p1, PPM_Info *p2,
		unsigned int err_best_so_far) {
	int x;
	int y;
	Color c1, c2;
	unsigned int error = 0;

	if ((p1->width != p2->width) || (p1->height != p2->height)) {
		fprintf(sock_fh,
				"Compare pictures not identical in size, internal error\n");
		fprintf(sock_fh, "p1 w %d, p2 w %d, p1 h %d, p2 h %d\n", p1->width,
				p2->width, p1->height, p2->height);
		return (-1);
	}

	for (y = 0; y < p1->height; y++) {
		for (x = 0; x < p1->width; x++) {
			PPM_GetPixel(p1, x, y, &c1);
			PPM_GetPixel(p2, x, y, &c2);

			/*
			 * Calculate diff into error
			 */
			error += ((c1.r - c2.r) * (c1.r - c2.r));
			error += ((c1.g - c2.g) * (c1.g - c2.g));
			error += ((c1.b - c2.b) * (c1.b - c2.b));
		}
		// Each row we check if the error factor exceeds our best PPM match so far.
		// If so there is no point in continuing.
		if (error > err_best_so_far){
			return error;
		}
	}

	return error;
}

void ReportWandException(MagickWand *wand, FILE *sock_fh) {
	ExceptionType severity;
	char *description = MagickGetException(wand, &severity);
	(void) fprintf(sock_fh, "ERROR: %s %s %lu %s\n", GetMagickModule(),
			description);
	description = (char *) MagickRelinquishMemory(description);
}

void ThrowWandException(MagickWand *wand, FILE *sock_fh) {
	ReportWandException(wand, sock_fh);
	exit(-1);
}

/*
 * ppm_miniature_from_filename
 *
 * sock_fh  - error channel
 * filename - name of file to read
 * new_size - size of miniature required
 *
 * Return a miniature PPM image from the filename.
 * Null on failure.
 *
 */

PPM_Info *ppm_miniature_from_filename(FILE *sock_fh, char *filename,
		int new_size) {
	PPM_Info *ppm;
	MagickWand *magick_wand = NewMagickWand();

	/*
	 Read an image.
	 */

	MagickBooleanType status = MagickReadImage(magick_wand, filename);
	if (status == MagickFalse) {
		ReportWandException(magick_wand, sock_fh);
		magick_wand = DestroyMagickWand(magick_wand);
		return NULL;
	}

	uint32_t width = MagickGetImageWidth(magick_wand);
	uint32_t height = MagickGetImageHeight(magick_wand);

	// Too small
	if ((width < new_size) || (height < new_size)) {
		fprintf(sock_fh,
				"ERROR: ppm_miniature_from_filename Failed on filename %s because size (%dx%d) below compare size\n",
				filename, width, height);
		magick_wand = DestroyMagickWand(magick_wand);
		return NULL;
	}

	/*
	 Turn the images into a thumbnail sequence.
	 */
	MagickResetIterator(magick_wand);
	while (MagickNextImage(magick_wand) != MagickFalse)
		MagickResizeImage(magick_wand, new_size, new_size, LanczosFilter, 1.0);

	//attempt to set Image depth to 8.
	// Image depth can automatically change to 16 after resize
	// http://www.imagemagick.org/discourse-server/viewtopic.php?f=6&t=18262
	MagickSetImageDepth(magick_wand, 8);

	width = MagickGetImageWidth(magick_wand);
	height = MagickGetImageHeight(magick_wand);

	ppm = ppm_info_allocate(width, height);
	if (ppm == NULL) {
		fprintf(sock_fh,
				"ERROR: ppm_miniature_from_filename Failed to allocate memory for image from filename %s\n",
				filename);
		magick_wand = DestroyMagickWand(magick_wand);
		return NULL;
	}

	ppm->width = width;
	ppm->height = height;
	ppm->modval = 3 * ppm->width;
	status = MagickExportImagePixels(magick_wand, 0, 0, width, height, "RGB",
			CharPixel, ppm->data);

	if (status == MagickFalse) {
		fprintf(sock_fh,
				"ERROR: ppm_miniature_from_filename Failed. Error from MagickExportImagePixels follows:\n");
		ReportWandException(magick_wand, sock_fh);
		magick_wand = DestroyMagickWand(magick_wand);
		free(ppm);
		return NULL;
	}

	//destroy the wand
	magick_wand = DestroyMagickWand(magick_wand);
	return ppm;
}

