/*
 * This is a DIDS (Duplicate Image Detection System) helper module.
 * This module is used to store PPM details in RAM.
 * Please see the README file for further details.
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

