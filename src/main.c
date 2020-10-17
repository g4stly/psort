#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <png.h>

#include "options.h"
#include "util.h"

struct image {
	png_image png;
	png_bytep buffer;
};

int is_white(png_bytep p, int ch_sz)
{
	u_int16_t r = *p;
	u_int16_t g = *(p + ch_sz);
	u_int16_t b = *(p + ch_sz * 2);
	return r == 255 && g == 255 && b == 255;
}

static int (*qualified)(png_bytep, png_bytep, int, struct options *);

static int check_mask(
	png_bytep p, png_bytep m, 
	int ch_sz, struct options *opt)
{
	return is_white(m, ch_sz);
}

static int check_value(
	png_bytep p, png_bytep m, 
	int ch_sz, struct options *opt)
{
	return opt->qualified(p, ch_sz, opt);
}

static int check_both(
	png_bytep p, png_bytep m, 
	int ch_sz, struct options *opt)
{
	return is_white(m, ch_sz) && opt->qualified(p, ch_sz, opt);
}

void read_image(struct image *i, const char *file, png_uint_32 format)
{
	i->png.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&i->png, file)) {
		die("read_image(): %s\n", i->png.message);
	}

	i->png.format = format;
	i->png.flags = i->png.flags | PNG_FORMAT_FLAG_LINEAR;
	i->buffer = malloc(PNG_IMAGE_SIZE(i->png));

	if (!i->buffer) die("read_image(): malloc():");
	if (!png_image_finish_read(&i->png, NULL, i->buffer, 0, NULL)) {
		die("read_image(): finish_read(): %s\n", i->png.message);
	}
}

/* should be merge sort... */
void sort_interval(png_bytep src, png_bytep dest,
	int len, int px_sz, int ch_sz, 
	struct options *opt)
{
	if (len <= 1) return;

	/*
	 * len needs to be pixels!
	 * half_len needs to be pixels!
	 * h needs to be bytes!
	 */

	double bl, br; 
	int half_len = len / 2;
	int lenp = len * px_sz;
	int h = half_len * px_sz;
	int i = 0, l = 0, r = h;

	sort_interval(dest, src, half_len, px_sz, ch_sz, opt);
	sort_interval(
		dest + h, src + h, 
		half_len + (len % 2),
		px_sz, ch_sz, opt);

	while (l < h && r < lenp) {
		bl = opt->value(src + l, ch_sz);
		br = opt->value(src + r, ch_sz);
		if (opt->compare(bl, br)) {
			memcpy(dest + i, src + l, px_sz);
			l += px_sz;
		} else {
			memcpy(dest + i, src + r, px_sz);
			r += px_sz;
		}
		i += px_sz;
	}

	while (l < h) {
		memcpy(dest + i, src + l, px_sz);
		l += px_sz;
		i += px_sz;
	}

	while (r < lenp) {
		memcpy(dest + i, src + r, px_sz);
		r += px_sz;
		i += px_sz;
	}
}

void sort_pixels_row(
	struct image *src, 
	struct image *dest,
	struct image *mask,
	int y, int px_sz, int ch_sz,
	struct options *opt)
{
	int i_start = -1;
	int i, x, q, len, offset;
	png_image png = dest->png;
	png_bytep buf = src->buffer;

	for (x = 0; x < png.width; x++) {
		i = (y * png.width + x) * px_sz;
		q = qualified(buf + i, mask->buffer + i, ch_sz, opt);
		if (i_start < 0) {
			if (q) i_start = x;
			continue;
		} else if (q && x != png.width - 1) {
		       	continue;
		}

		len = x - i_start;
		if (x == png.width - 1) len++;

		offset = ((y * png.width + i_start) * px_sz);
		sort_interval(
			src->buffer + offset,
			dest->buffer + offset,
			len, px_sz, ch_sz, opt
		);
		i_start = -1;
	}
}

void sort_pixels(
	struct image *s, 
	struct image *d, 
	struct image *m,
	struct options *opt)
{
	d->png.version = PNG_IMAGE_VERSION;
	d->png.width = s->png.width;
	d->png.height = s->png.height;
	d->png.format = s->png.format;
	d->png.flags = s->png.flags;

	int buffer_sz = PNG_IMAGE_SIZE(d->png);
	int pixel_sz = PNG_IMAGE_PIXEL_SIZE(d->png.format);
	int channel_sz = PNG_IMAGE_PIXEL_COMPONENT_SIZE(d->png.format);

	d->buffer = malloc(buffer_sz);
	if (!d->buffer) die("sort_pixels(): malloc():");
	memcpy(d->buffer, s->buffer, buffer_sz);

	for (int y = 0; y < d->png.height; y++) {
		printf("sorting row %i(%i)\n", y + 1, d->png.height);
		sort_pixels_row(s, d, m, y, pixel_sz, channel_sz, opt);
	}
}

void write_image(struct image *i, struct options *opt)
{
	if (!png_image_write_to_file(&i->png, 
		opt->output_file, 0, i->buffer, 0, NULL)) 
	{
		die("write_image(): %s\n", i->png.message);
	}
}

void free_image(struct image *i)
{
	png_image_free(&i->png);
	free(i->buffer);
}


int main(int argc, char **argv)
{
	struct image src;
	struct image dest;
	struct image mask;
	struct options opt;

	memset(&opt, 0, sizeof(opt));
	memset(&src, 0, sizeof(src));
	memset(&dest, 0, sizeof(dest));
	memset(&mask, 0, sizeof(mask));

	parse_options(argc, argv, &opt);

	qualified = check_value;
	read_image(&src, opt.input_file, opt.format);
	if (opt.mask_file) {
		read_image(&mask, opt.mask_file, opt.format);
		qualified = opt.pipeline_len > 0 ? check_both : check_mask;
		if (mask.png.width != src.png.width
			|| mask.png.height != src.png.height)
		{
			free_image(&src);
			free_image(&mask);
			die("fucked up mask\n");
		}
	}

	sort_pixels(&src, &dest, &mask, &opt);
	write_image(&dest, &opt);

	if (opt.mask_file) {
		free_image(&mask);
	}
	free_image(&dest);
	free_image(&src);

	return 0;
}
