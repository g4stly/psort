#include <stdlib.h>
#include <getopt.h>
#include <png.h>

#include "options.h"
#include "util.h"


const char *opt_str = "raov:g:l:d:";
static struct option long_opts[] = {
	{ "mask", required_argument, NULL, 'm' },
	{ "destination", required_argument, NULL, 'd' },

	{ "value", required_argument, NULL, 'v' },
	{ "gt", required_argument, NULL, 'g' },	// value greater than arg
	{ "lt", required_argument, NULL, 'l' },	// value less than
	{ "reverse", no_argument, NULL, 'r' },
	{ "and", no_argument, NULL, 'a' },
	{ "or", no_argument, NULL, 'o' },
};

static double red(png_bytep p, int ch_sz);
static double blue(png_bytep p, int ch_sz);
static double green(png_bytep p, int ch_sz);
static double brightness(png_bytep p, int ch_sz);
static int compare_left(double left, double right);
static int compare_right(double left, double right);
static int qualified_gt(png_bytep p, int ch_sz, struct options *opt);
static int qualified_lt(png_bytep p, int ch_sz, struct options *opt);
static int qualified_or(png_bytep p, int ch_sz, struct options *opt);
static int qualified_and(png_bytep p, int ch_sz, struct options *opt);

void parse_options(int argc, char **argv, struct options *opt)
{
	int i;
	char c;
	opt->value = brightness;
	opt->compare = compare_left;
	opt->qualified = qualified_or;
	while ((c = getopt_long(argc, argv, opt_str, long_opts, &i)) > 0) 
	switch (c) {
	case 'd':
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
	case 'v': 
		switch(optarg[0]) {
		case 'l': opt->value = brightness; break;
		/* todo: make these not stupid */
		case 'r': opt->value = red; break;
		case 'g': opt->value = green; break;
		case 'b': opt->value = blue; break;
		default: die("fucked up yo\n"); break;
  		}
		break;
	case 'r':
		opt->compare = compare_right;
		break;
	case 'a':
		opt->qualified = qualified_and;
		break;
	case 'o':
		opt->qualified = qualified_or;
		break;
	default:
		die("usage: todo\n");
		break;
	}

	// no output file
	if (!opt->output_file || optind >= argc) {
		die("usage: todo\n");
	}

	// no pipeline specified
	if (opt->pipeline_len < 1) {
		// die("usage: todo\n");
		die("what the fuck are you doing....\n");
	}

	opt->format = PNG_FORMAT_RGBA;
	opt->input_file = argv[optind];
}

static double brightness(png_bytep p, int ch_sz)
{
	u_int16_t r = *p;
	u_int16_t g = *(p + ch_sz);
	u_int16_t b = *(p + ch_sz * 2);
	return 0.2126*r + 0.7152*g + 0.0722*b;
}

static double red(png_bytep p, int ch_sz)
{
	return (u_int16_t)*p;
}

static double green(png_bytep p, int ch_sz)
{
	return (u_int16_t)*(p + ch_sz);
}

static double blue(png_bytep p, int ch_sz)
{
	return (u_int16_t)*(p + ch_sz*2);
}

static int compare_left(double left, double right)
{
	return left > right;
}

static int compare_right(double left, double right)
{
	return left < right;
}

static int qualified_gt(png_bytep p, int ch_sz, struct options *opt)
{
	return opt->value(p, ch_sz) >= opt->gt_value;
}

static int qualified_lt(png_bytep p, int ch_sz, struct options *opt)
{
	return opt->value(p, ch_sz) <= opt->lt_value;
}

static int qualified_or(png_bytep buf, int ch_sz, struct options *opt)
{
	int qualified = 0;
	for (int p = 0; !qualified && p < opt->pipeline_len; p++) {
		qualified = opt->pipeline[p](buf, ch_sz, opt);
	}
	return qualified;
}

static int qualified_and(png_bytep buf, int ch_sz, struct options *opt)
{
	int qualified = 1;
	for (int p = 0; qualified && p < opt->pipeline_len; p++) {
		qualified = qualified && opt->pipeline[p](buf, ch_sz, opt);
	}
	return qualified;
}

