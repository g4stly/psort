# psort
sorts pixels, only works with png files (for now), dependancy on libpng

## usage
`psort [options] input_filename.png [options]`

basically, if you dont specify the input file with the `-s` option, 
the first argument that isn't an option should be the input filename.

options may appear before or after the input filename.

## options

- \-s, --source: filename for image to be sorted
- \-d, --destination: filename of image to be written
- \-m, --mask: filename for image mask

- \-g <value>, --gt <value>: sort pixels with a value greater than <value>
- \-l <value>, --lt <value>: sort pixels with a value less than <value>
- \-o, --or: sort pixels that are --gt <x> OR --lt <y> (default)
- \-a, --and: sort pixels that have a value --gt <x> AND --lt <y>
- \-v <quality>, --value <quality>: sort pixels according to one of the qualities below. you may abbreviate to the first letter.
  - luma: brightness
  - red: restrict to only red colors, then sort by brightness, not saturation
  - blue: restrict to only blue colors, then sort by brightness, not saturation
  - green: restrict to only green colors, then sort by brightness, not saturation
- \-r, --reverse: sort right to left instead of the default, which is left to right

## examples

`psort -g80 -l40 -doutput.png input.png`

`psort -g20 -a -l70 -dout.png in.png`

`psort --gt 40 --and --lt 120 --destination dest.png --source source.png`

`psort -vb -g1 -dout.png in.png`

`psort --value blue --gt 40 -dout.png in.png`

stuff like that. 
