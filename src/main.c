#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <png.h>

#include "util.h"

struct options {
	int flags;
	int format;
	int gt_value;
	int lt_value;
	const char *input_file;
	const char *output_file;
	int (*compare)(double, double);
	int (*pipeline[2])(png_bytep, int, struct options *);
	int pipeline_len;
};

struct image {
	png_image png;
	png_bytep buffer;
};

double brightness(png_bytep p, int ch_sz)
{
	u_int16_t r = *p;
	u_int16_t g = *(p + ch_sz);
	u_int16_t b = *(p + ch_sz * 2);
	// u_int16_t a = *(p + ch_sz * 3);
	return 0.2126*r + 0.7152*g + 0.0722*b;
}

int qualified_gt(png_bytep p, int ch_sz, struct options *opt)
{
	return brightness(p, ch_sz) > opt->gt_value;
}

int qualified_lt(png_bytep p, int ch_sz, struct options *opt)
{
	return brightness(p, ch_sz) < opt->lt_value;
}

int compare_left(double left, double right)
{
	return left > right;
}

int compare_right(double left, double right)
{
	return left < right;
}

void parse_options(int argc, char **argv, struct options *opt)
{
	char c;
	int sort_right = 1;
	while ((c = getopt(argc, argv, "o:g:l:r12")) > 0) switch (c) {
	case 'o':
		opt->output_file = optarg;
		break;
	case 'g':
		if (opt->pipeline_len < 2) {
			opt->pipeline[opt->pipeline_len++] = qualified_gt;
		}
		opt->gt_value = atoi(optarg);
		break;
	case 'l':
		if (opt->pipeline_len < 2) {
			opt->pipeline[opt->pipeline_len++] = qualified_lt;
		}
		opt->lt_value = atoi(optarg);
		break;
	case 'r':
		sort_right = !sort_right;
		break;
	case '1':
		opt->format = PNG_FORMAT_RGBA;
		opt->flags = 0;
		break;
	case '2':
		opt->format = PNG_FORMAT_LINEAR_RGB_ALPHA;
		opt->flags |= PNG_FORMAT_FLAG_LINEAR;
		break;
	default:
		die("usage: todo\n");
		break;
	}

	if (!opt->output_file || optind >= argc) {
		die("usage: todo\n");
	}
	if (opt->pipeline_len < 1) {
		die("what the fuck are you doing....\n");
	}
	if (!opt->format) {
		opt->format = PNG_FORMAT_RGBA;
	}

	opt->input_file = argv[optind];
	opt->compare = sort_right ? compare_right : compare_left;
}

void read_image(struct image *i, struct options *opt)
{
	i->png.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&i->png, opt->input_file)) {
		die("read_image(): %s\n", i->png.message);
	}

	i->png.format = opt->format;
	i->png.flags = i->png.flags | PNG_FORMAT_FLAG_LINEAR;
	i->buffer = malloc(PNG_IMAGE_SIZE(i->png));

	if (!i->buffer) die("read_image(): malloc():");
	if (!png_image_finish_read(&i->png, NULL, i->buffer, 0, NULL)) {
		die("read_image(): finish_read(): %s\n", i->png.message);
	}
}

// bubbles~~ ^-^
// todo: not bubbles
void sort_interval(
	png_bytep src, 
	png_bytep dest, 
	int len, int px_sz, int ch_sz,
	struct options *opt)
{
	int sorted = 0;
	double left, right;
	png_byte tmp[px_sz];
	int i, lim = (len - 1) * px_sz;
	while (!(sorted++)) for (i = 0; i < lim; i += px_sz) {
		right = brightness(dest + i + px_sz, ch_sz);
		left = brightness(dest + i, ch_sz);
		if (opt->compare(left, right)) {
			sorted = 0;
			memcpy(tmp, dest + i, px_sz);
			memcpy(dest + i, dest + i + px_sz, px_sz);
			memcpy(dest + i + px_sz, tmp, px_sz);
		}
	}
}

void sort_pixels_row(
	struct image *src, 
	struct image *dest,
	int y, int px_sz, int ch_sz,
	struct options *opt)
{
	int i_start = -1;
	int qualified = 0;
	int i, x, p, len, offset;
	png_bytep buf = src->buffer;
	png_image png = dest->png;

	for (x = 0; x < png.width; x++) {
		qualified = 0;
		i = (y * png.width + x) * px_sz;
		for (p = 0; !qualified && p < opt->pipeline_len; p++) {
			qualified = opt->pipeline[p](buf + i, ch_sz, opt);
		}

		if (i_start < 0) {
			if (qualified) i_start = x;
			continue;
		} else if (qualified && x != png.width - 1) {
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

void sort_pixels(struct image *s, struct image *d, struct options *opt)
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
		sort_pixels_row(s, d, y, pixel_sz, channel_sz, opt);
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
	struct options opt;

	memset(&opt, 0, sizeof(opt));
	memset(&src, 0, sizeof(src));
	memset(&dest, 0, sizeof(dest));

	parse_options(argc, argv, &opt);

	read_image(&src, &opt);
	sort_pixels(&src, &dest, &opt);
	write_image(&dest, &opt);

	free_image(&dest);
	free_image(&src);

	return 0;
}
