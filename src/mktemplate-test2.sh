. $srcdir/mktemplate-funcs.sh

# Fixed in CVS on 2005-06-04, mktemplate.cc:239
random 100 127 >in1
random 924 >image
cat image >>in1
random 1k >>image
mt in*
tlist <<EOF
in-template            0         1948
image-info          1948              5Vc0dSieO4kfcLVRA8KSrA 1024
EOF