/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module is used to store PPM details in RAM.
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
#include <stdlib.h>
#include "ppm.h"

/*
 * reserve memory for a PPM_Info object
 */
PPM_Info *ppm_info_allocate(int width, int height) {
	PPM_Info *ppm = (PPM_Info *) malloc(sizeof(PPM_Info));
	if (!ppm) {
		return NULL;
	}
	ppm->width = width;
	ppm->height = height;
	ppm->modval = width * 3;
	ppm->data = malloc(3 * width * height);
	if (ppm->data == NULL) {
		free(ppm);
		return NULL;
	}
	return ppm;
}

/*
 * free ppm struct from mem
 */
void ppm_info_free(PPM_Info *ppm) {
	if (ppm->data) {
		free(ppm->data);
	}
	free(ppm);
}

