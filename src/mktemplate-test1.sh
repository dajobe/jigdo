. $srcdir/mktemplate-funcs.sh

# Fixed sometime after 0.7.0 release
random 256k >in1
cp in1 in2
random 128k >>in1
random 128k >>in2
random 128k >image
cat in1 >>image
random 128k >>image
mt in*
tlist <<EOF
in-template            0       131072
need-file         131072       393216 DC0EKqVrguQBW6nKp-TlHQ bqRXnKaA3-Y
in-template       524288       131072
image-info        655360              k0Hs013jj1tATexvCT9xyA 1024
EOF
