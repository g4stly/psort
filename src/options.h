#ifndef options_h_
#define options_h_

#include <png.h>

struct options {
	int format;
	int gt_value;
	int lt_value;
	const char *mask_file;
	const char *input_file;
	const char *output_file;
	double (*value)(png_bytep, int);
	int (*compare)(double, double);
	int (*qualified)(png_bytep, int, struct options *);
	int (*pipeline[2])(png_bytep, int, struct options *);
	int pipeline_len;
};

void parse_options(int argc, char **argv, struct options *opt);

#endif
