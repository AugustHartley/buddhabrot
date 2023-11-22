#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <complex.h>
#include <math.h>

#include "globals.h"
#include "rand.h"

#ifndef BUDDHABROT
#define BUDDHABROT buddhabrot

#pragma GCC target "sse2"

#define TEST(a) a[0] & a[1] & a[2] & a[3]

#define VEC_SIZE 4
#endif

typedef float fvec __attribute__((vector_size(VEC_SIZE * 4)));
typedef int32_t ivec __attribute__((vector_size(VEC_SIZE * 4)));
typedef uint32_t uvec __attribute__((vector_size(VEC_SIZE * 4)));

void *BUDDHABROT(void *arg) {
	uint32_t s[4];
	memcpy(s, arg, sizeof(uint32_t[4]));
	int total_r, total_g, total_b;
	fvec zi, zr, ci, cr, z2i, z2r;
	ivec iter, esc;
	fvec *zi_hist = aligned_alloc(sizeof(fvec), ITERATIONS * sizeof(fvec));
	fvec *zr_hist = aligned_alloc(sizeof(fvec), ITERATIONS * sizeof(fvec));
	uint32_t *img = calloc(IMG_S, sizeof(uint32_t));
	int samples = SAMPLES / VEC_SIZE;
	for (int i = 0; i < samples; i++) {
		for (int j = 0; j < VEC_SIZE; j++) {
			rand_point:
			uint32_t uzi = xoro_next(s) >> 9 | 0x40800000;
			uint32_t uzr = xoro_next(s) >> 9 | 0x40800000;
			zi[j] = *(float*)(&uzi) - 6.0;
			zr[j] = *(float*)(&uzr) - 6.0;
			z2i[j] = zi[j] * zi[j];
			float a = zr[j] + 1.0;
			if (a * a + z2i[j] <= 0.0625) goto rand_point;
			float b = zr[j] - 0.25;
			float c = b * b + z2i[j];
			if (c * (c + b) <= z2i[j] * 0.25) goto rand_point;
		}
		ci = zi; cr = zr;
		z2i = zi * zi; z2r = zr * zr;
		esc = (ivec){0};
		iter = (ivec){0};
		for (int j = 0; j < ITERATIONS; j++) {
			esc |= z2i + z2r > 4.0;
			zi = (zr + zr) * zi + ci;
			zr = z2r - z2i + cr;
			z2i = zi * zi;
			z2r = zr * zr;
			zi_hist[j] = zi;
			zr_hist[j] = zr;
			if (TEST(esc)) break;
			iter += ~esc & 1;
		}
		for (int j = 0; j < VEC_SIZE; j++) {
			int col = 0;

			if (iter[j] < 50) { col = 2; }
			else if (iter[j] < 500) { col = 1; }
			else if (iter[j] < 5001) { col = 0; }
			else continue;

			for (int k = 0; k < iter[j]; k++) {
				unsigned int x = zi_hist[k][j] * scale_s + scale_x;
				unsigned int y = zr_hist[k][j] * scale_s + scale_y;
				if (x >= IMG_X || y >= IMG_Y) continue;
				img[(y * IMG_X + x) * 3 + col] += 1;
			}
		}
	}
	TOTAL_R += total_r;
	TOTAL_G += total_g;
	TOTAL_B += total_b;
	free(zi_hist);
	free(zr_hist);
	return img;
}
