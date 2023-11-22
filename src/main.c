#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <math.h>
#include <cpuid.h>
#include <png.h>
#include <getopt.h>

#include "globals.h"
#include "rand.h"

void *buddhabrot(void *);

#ifdef __x86_64__
void *buddhabrot_avx2(void *);
#endif

int SAMPLES = 20000000;
int ITERATIONS = 5000;
int MIN_ITERATION = 0;
int IMG_X = 1024;
int IMG_Y = 512;
size_t IMG_S;

float scale_s;
float scale_x;
float scale_y;

_Atomic int TOTAL_R = 0;
_Atomic int TOTAL_G = 0;
_Atomic int TOTAL_B = 0;

int main(int argc, char *argv[]) {
	uint32_t s[4] = {0xa047ab50, 0x88144c49, 0xe85b73c1, 0x67c8ba8d};

	char *out_filename = "img.png";
	{
		int c;
		struct option long_options[] = {
			{"samples", required_argument, 0, 's'},
			{"iters", required_argument, 0, 'i'},
			{"min-iter", required_argument, 0, 'm'},
			{"width", required_argument, 0, 'w'},
			{"height", required_argument, 0, 'h'},
			{"output", required_argument, 0, 'o'},
			{"help", no_argument, 0, 256},
			{"version", no_argument, 0, 257},
			{0, 0, 0, 0}
		};
		int option_index = 0;
		while ((c = getopt_long(argc, argv, "s:i:m:w:h:o:", long_options, &option_index)) != -1) {
			switch (c) {
			case 's':
				SAMPLES = strtol(optarg, NULL, 0); break;
			case 'i':
				ITERATIONS = strtol(optarg, NULL, 0); break;
			case 'm':
				MIN_ITERATION = strtol(optarg, NULL, 0); break;
			case 'w':
				IMG_X = strtol(optarg, NULL, 0); break;
			case 'h':
				IMG_Y = strtol(optarg, NULL, 0); break;
			case 'o':
				if (optarg[0] == '-' && optarg[1] == '\0') out_filename = "/dev/stdout";
				else out_filename = optarg;
				break;
			case 256:
				printf(
					"Usage: %s [OPTION]...\n"
					"Generate a buddhabrot fractal.\n\n"
					"Mandatory arguments to long options are mandatory for short options too.\n"
					"  -s, --samples=SAMPLES              Number of samples to take for the fractal.\n"
					"  -i, --iters=ITERATIONS             Number of iterations for the fractal.\n"
					"  -m, --min-iter=MINIMUM_ITERATIONS  Minimum iteration to draw to the image.\n"
					"  -w, --width=WIDTH                  Width of the image.\n"
					"  -h, --height=HEIGHT                Height of the image.\n"
					"  -o, --output=OUTPUT_FILENAME       File to output image, use a '-' for stdout.\n"
					"      --help                         Show this help text and exit.\n"
					"      --version                      Show version information and exit.\n", argv[0]
				);
				return 0;
			case 257:
				printf(
					"buddhabrot 0.9\n"
					"Copyright (C) 2023 dankcatlord\n"
					"License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
					"This is free software: you are free to change and redistribute it.\n"
					"There is NO WARRANTY, to the extent permitted by law.\n\n"
					"Written by dankcatlord. <https://git.dankcatlord.net/dankcatlord/buddhabrot>\n"
				);
				return 0;
			default:
				printf("getopt: %d\n", c);
			case '?':
				return 1;
			}
		}
	}
	FILE *out_file = fopen(out_filename, "wb");

	IMG_S = IMG_X * IMG_Y * 3;
	scale_s = (float)IMG_Y * 0.25;
	scale_x = (float)IMG_X * 0.5;
	scale_y = (float)IMG_Y * 0.5;

	void *(*buddhabrot_func)(void*);
	if (__builtin_cpu_supports("avx2")) buddhabrot_func = buddhabrot_avx2;
	else buddhabrot_func = buddhabrot;

	size_t thread_count = sysconf(_SC_NPROCESSORS_ONLN);
	fprintf(stderr, "Generating image with %d thread(s).\n", thread_count);

	SAMPLES /= thread_count;

	pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
	uint32_t **thread_state = malloc(thread_count * sizeof(uint32_t*));

	for (int i = 0; i < thread_count; i++) {
		thread_state[i] = malloc(sizeof(uint32_t[4]));
		memcpy(thread_state[i], s, sizeof(uint32_t[4]));
		if (pthread_create(&threads[i], NULL, buddhabrot_func, thread_state[i]) != 0) exit(1);
		xoro_jump(s);
	}

	uint32_t *grand_img = calloc(IMG_S, sizeof(uint32_t));
	for (int i = 0; i < thread_count; i++) {
		uint32_t *img;
		if (pthread_join(threads[i], (void**)&img) != 0) exit(1);
		free(thread_state[i]);
		for (int j = 0; j < IMG_S; j++) {
			grand_img[j] += img[j];
		}
		free(img);
	}
	free(threads);
	free(thread_state);

	for (int i = 0; i < IMG_S / 3; i++) {
		grand_img[i * 3 + 1] += grand_img[i * 3 + 2];
		grand_img[i * 3 + 0] += grand_img[i * 3 + 1];
		grand_img[i * 3 + 2] *= 5;
		grand_img[i * 3 + 1] *= 3;
		grand_img[i * 3 + 0] *= 2;
	}
	
	uint32_t max_val = 0;
	for (int i = 0; i < IMG_S; i++) {
		if (grand_img[i] > max_val) max_val = grand_img[i];
	}

	uint16_t *out_img = malloc(IMG_S * sizeof(uint16_t));
	for (int i = 0; i < IMG_S; i++) {
		out_img[i] = fmin((float)grand_img[i] * (12500.0f / (SAMPLES * thread_count)), 1.0) * 65535;
	}

	free(grand_img);

	{
		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		png_infop info_ptr = png_create_info_struct(png_ptr);

		png_init_io(png_ptr, out_file);

		png_set_IHDR(png_ptr, info_ptr, IMG_X, IMG_Y, 16, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		png_write_info(png_ptr, info_ptr);
		png_set_swap(png_ptr);

		for (int i = 0; i < IMG_Y; i++) png_write_row(png_ptr, &out_img[i * IMG_X * 3]);

		png_write_end(png_ptr, info_ptr);

		png_destroy_write_struct(&png_ptr, &info_ptr);
	}

	fclose(out_file);

	return 0;
}
