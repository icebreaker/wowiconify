/*{{{
    MIT LICENSE

    Copyright (c) 2021, Mihail Szabolcs

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the 'Software'), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
}}}*/
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <memory.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <io.h> // for _findfirst
	#include <windows.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifndef WOWICONIFY_VERSION
	#define WOWICONIFY_VERSION 0x100
#endif

#ifndef INPUT_IMAGE_MAX
	#define INPUT_IMAGE_MAX 512
#endif

#ifndef INPUT_IMAGE_SIZE
	#define INPUT_IMAGE_SIZE 32
#endif

#ifndef INPUT_IMAGE_BPP
	#define INPUT_IMAGE_BPP 4
#endif

#ifndef INPUT_IMAGE_STRIDE_IN_BYTES
	#define INPUT_IMAGE_STRIDE_IN_BYTES INPUT_IMAGE_SIZE * INPUT_IMAGE_BPP
#endif

#ifndef INPUT_IMAGE_COLOR_MATCH_THRESHOLD
	#define INPUT_IMAGE_COLOR_MATCH_THRESHOLD (INPUT_IMAGE_MAX << 2)
#endif

#ifndef INPUT_IMAGE_COLOR_SAMPLE_SIZE
	#define INPUT_IMAGE_COLOR_SAMPLE_SIZE (INPUT_IMAGE_SIZE >> 2)
#endif

#ifndef SOURCE_IMAGE_MAX_SIZE
	#define SOURCE_IMAGE_MAX_SIZE 24000
#endif

typedef struct
{
	union
	{
		uint32_t u32;
		struct { uint8_t r, g, b, a; };
	};
} input_image_color_t;

typedef struct
{
	int w, h, bpp;
} input_image_info_t;

typedef struct
{
	uint8_t *pixels;
	input_image_color_t color;
} input_image_t;

static input_image_t input_images[INPUT_IMAGE_MAX + 1] = { { 0, } };

static void unload_input_images(void);
static bool load_input_images(const int argc, const char *argv[]);
static const input_image_t *match_input_image(const input_image_color_t *color, const input_image_t *prev);
static bool write_output_image(const char *output_filename, const char *source_filename);

int main(int argc, char *argv[])
{
	bool ret;

	if(argc < 4)
	{
		fprintf(stderr, "usage: %s output.png source.png input1.png [input2...]\n", argv[0]);
		return EXIT_FAILURE;
	}

	ret = load_input_images(argc - 3, (const char **) argv + 3);

	if(ret)
		ret = write_output_image(argv[1], argv[2]);

	unload_input_images();
	return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void unload_input_images(void)
{
	for(input_image_t *input_image = input_images; input_image->pixels != NULL; input_image++)
	{
		free(input_image->pixels);
		input_image->pixels = NULL;
	}
}

static inline bool load_input_image_info(input_image_info_t *input_image_info, const char *filename)
{
	if(!stbi_info(filename, &input_image_info->w, &input_image_info->h, &input_image_info->bpp))
	{
		fprintf(stderr, "failed to load image info '%s': %s\n", filename, stbi_failure_reason());
		return false;
	}

	return true;
}

static inline input_image_color_t input_image_sample_color(const input_image_t *input_image, const int w, const int h)
{
	input_image_color_t *colors;
	int ww, hh;

#if INPUT_IMAGE_COLOR_SAMPLE_SIZE < INPUT_IMAGE_SIZE
	uint8_t sample_buffer[INPUT_IMAGE_COLOR_SAMPLE_SIZE * INPUT_IMAGE_COLOR_SAMPLE_SIZE * INPUT_IMAGE_BPP] = { 0, };

	if(stbir_resize_uint8(
		input_image->pixels,
		w,
		h,
		0,
		sample_buffer,
		INPUT_IMAGE_COLOR_SAMPLE_SIZE,
		INPUT_IMAGE_COLOR_SAMPLE_SIZE,
		0,
		INPUT_IMAGE_BPP)
	)
	{
		colors = (input_image_color_t *) sample_buffer;
		ww = INPUT_IMAGE_COLOR_SAMPLE_SIZE;
		hh = INPUT_IMAGE_COLOR_SAMPLE_SIZE;
	}
	else
#endif
	{
		colors = (input_image_color_t *) input_image->pixels;
		ww = w;
		hh = h;
	}

	const int cx = ww >> 1;
	const int cy = hh >> 1;
	const int xo = cx >> 1;
	const int yo = cy >> 1;

	input_image_color_t left_color  = colors[(cx - xo) + (cy * ww)];
	input_image_color_t right_color = colors[(cx + xo) + (cy * ww)];
	input_image_color_t up_color    = colors[cx + ((cy - yo) * ww)];
	input_image_color_t down_color  = colors[cx + ((cy + yo) * ww)];

	return (input_image_color_t) {
		.r = (left_color.r + right_color.r + up_color.r + down_color.r) >> 2,
		.g = (left_color.g + right_color.g + up_color.g + down_color.g) >> 2,
		.b = (left_color.b + right_color.b + up_color.b + down_color.b) >> 2,
		.a = 0xFF
	};
}

static inline bool load_input_image(input_image_t *input_image, const char *filename, const int w, const int h)
{
	int iw, ih, ibpp;
	uint8_t *input_pixels, *output_pixels;

	input_pixels = stbi_load(filename, &iw, &ih, &ibpp, INPUT_IMAGE_BPP);
	if(input_pixels == NULL)
	{
		fprintf(stderr, "failed to load image '%s': %s\n", filename, stbi_failure_reason());
		return false;
	}

	output_pixels = malloc(w * h * INPUT_IMAGE_BPP);
	if(output_pixels == NULL)
	{
		stbi_image_free(input_pixels);
		fprintf(stderr, "failed to allocate memory to resize image '%s'\n", filename);
		return false;
	}

	if(!stbir_resize_uint8(input_pixels, iw, ih, 0, output_pixels, w, h, 0, INPUT_IMAGE_BPP))
	{
		stbi_image_free(input_pixels);
		free(output_pixels);
		fprintf(stderr, "failed to resize image '%s'\n", filename);
		return false;
	}

	stbi_image_free(input_pixels);

	input_image->pixels = output_pixels;
	input_image->color = input_image_sample_color(input_image, w, h);

	return true;
}

static bool load_input_images(const int argc, const char *argv[])
{
	input_image_t *input_image = input_images;
	int n;

#ifdef _WIN32
	struct _finddata_t file_info;
	intptr_t file_handle;
	char path[MAX_PATH + 1] = { 0, };
	char *sep = NULL;

	strncpy(path, argv[0], MAX_PATH);

	for(char *s = path; *s != '\0'; s++)
	{
		if(*s == '/' || *s == '\\')
		{
			*s = '/';
			sep = s;
		}
	}

	if(sep != NULL)
		*sep = '\0';

	file_handle = _findfirst(argv[0], &file_info);
	if(file_handle == -1)
	{
		fprintf(stderr, "failed to find any input images with the pattern '%s'\n", argv[0]);
		return false;
	}

	n = 0;
	for(;;)
	{
		if(!(file_info.attrib & _A_SUBDIR) && strstr(file_info.name, ".png") != NULL)
		{
			char file_path[MAX_PATH + 1] = { 0, };
			snprintf(file_path, MAX_PATH, "%s/%s", path, file_info.name);

			if(!load_input_image(input_image++, file_path, INPUT_IMAGE_SIZE, INPUT_IMAGE_SIZE))
			{
				_findclose(file_handle);
				return false;
			}

			n++;
		}

		if(n >= INPUT_IMAGE_MAX || _findnext(file_handle, &file_info) != 0)
			break;
	}
	_findclose(file_handle);

	fprintf(stderr, "loaded %d input images\n", n);
	return n > 0;
#else
	n = argc > INPUT_IMAGE_MAX ? INPUT_IMAGE_MAX : argc;

	for(int i = 0; i < n; i++)
	{
		if(!load_input_image(input_image++, argv[i], INPUT_IMAGE_SIZE, INPUT_IMAGE_SIZE))
			return false;
	}

	fprintf(stderr, "loaded %d out of %d input images\n", n, argc);
#endif

	return true;
}

static const input_image_t *match_input_image(const input_image_color_t *color, const input_image_t *prev)
{
	const input_image_t *match = prev;
	int match_color = INPUT_IMAGE_COLOR_MATCH_THRESHOLD;

	for(const input_image_t *input_image = input_images; input_image->pixels != NULL; input_image++)
	{
		const int r = input_image->color.r - color->r;
		const int g = input_image->color.g - color->g;
		const int b = input_image->color.b - color->b;
		const int c = r * r + g * g + b * b;

		if(c <= match_color)
		{
			match_color = c;
			match = input_image;
		}
	}

	return match;
}

static bool write_output_image(const char *output_filename, const char *source_filename)
{
	int sw, sh, w, h;
	size_t stride_in_bytes;
	size_t size;
	uint8_t *output_pixels;
	bool ret;
	input_image_color_t *source_colors;
	input_image_t source_image;
	input_image_info_t source_image_info;
	const input_image_t *input_image = input_images;

	if(!load_input_image_info(&source_image_info, source_filename))
		return false;

	sw = source_image_info.w;
	sh = source_image_info.h;

	for(;;)
	{
		w = sw * INPUT_IMAGE_SIZE;
		h = sh * INPUT_IMAGE_SIZE;

		if(w <= SOURCE_IMAGE_MAX_SIZE && h <= SOURCE_IMAGE_MAX_SIZE)
			break;

		sw >>= 1;
		sh >>= 1;

		if(sw <= 0 || sh <= 0)
		{
			fprintf(stderr, "failed to resize output image '%s'\n", output_filename);
			return false;
		}

		fprintf(stderr, "resized output image '%s' to %dx%d\n", output_filename, sw, sh);
	}

	fprintf(stderr, "processing output image '%s'\n", output_filename);

	size = w * h * INPUT_IMAGE_BPP;
	stride_in_bytes = w * INPUT_IMAGE_BPP;

	if(!load_input_image(&source_image, source_filename, sw, sh))
		return false;

	output_pixels = malloc(size);
	if(output_pixels == NULL)
	{
		free(source_image.pixels);
		fprintf(stderr, "failed to allocate enough memory for output image '%s'\n", output_filename);
		return false;
	}

	memset(output_pixels, 0xFF, size);
	source_colors = (input_image_color_t *) source_image.pixels;

	for(int y = 0; y < sh; y++)
	{
		const int yo = y * INPUT_IMAGE_SIZE * stride_in_bytes;

		for(int x = 0; x < sw; x++)
		{
			const int xx = x * INPUT_IMAGE_STRIDE_IN_BYTES;
			input_image = match_input_image(source_colors++, input_image);

			uint8_t *dst = output_pixels + xx + yo;
			uint8_t *src = input_image->pixels;

			for(int i = 0; i < INPUT_IMAGE_SIZE; i++)
			{
				memcpy(dst, src, INPUT_IMAGE_STRIDE_IN_BYTES);
				src += INPUT_IMAGE_STRIDE_IN_BYTES;
				dst += stride_in_bytes;
			}
		}
	}

	if(strstr(output_filename, ".jpg") != NULL)
	{
		ret = stbi_write_jpg(output_filename, w, h, INPUT_IMAGE_BPP, output_pixels, 90);
	}
	else
	{
		stbi_write_png_compression_level = 9;
		ret = stbi_write_png(output_filename, w, h, INPUT_IMAGE_BPP, output_pixels, stride_in_bytes);
	}

	if(ret)
		fprintf(stderr, "wrote output image to '%s'\n", output_filename);
	else
		fprintf(stderr, "failed to write output image '%s'\n", output_filename);

	free(output_pixels);
	free(source_image.pixels);
	return ret;
}

// https://www.warcrafttavern.com/community/art-resources/icon-pack-4300-wow-retail-icons-in-png/
/* vim: set ts=4 sw=4 sts=4 noet: */
