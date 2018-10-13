#!/bin/bash

#Simple and stupid script to (re)generate image data. Needs an Unix-ish environment with
#ImageMagick and xxd installed.

convert nyan_64x32.gif nyan_64x32-f%02d.rgb
convert lenna.png lenna.rgb

OUTF="../anim.c"

echo '//Auto-generated' > $OUTF
echo 'static const unsigned char myanim[]={' >> $OUTF
{
	for x in nyan_64x32-f*.rgb; do
		echo $x >&2
		cat $x
	done 
	cat lenna.rgb
} | xxd -i >> $OUTF
echo "};" >> $OUTF
echo 'const unsigned char *anim=&myanim[0];' >> $OUTF
