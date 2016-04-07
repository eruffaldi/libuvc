#packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
#packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1
convert -size 640x480 -depth 8  -sampling-factor 4:2:0 -interlace plane yuv:$1 -colorspace rgb $1.jpg