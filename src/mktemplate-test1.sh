. $srcdir/mktemplate-funcs.sh

# Fixed sometime after 0.7.0 release
random 256k >in1
cp in1 in2
random 1k >>in1
random 1k >>in2
random 1k >image
cat in1 >>image
random 1k >>image
mt in*
tlist <<EOF
in-template            0         1024
need-file           1024       263168 wurT473rMMQ7z8-AXpV-EA bqRXnKaA3-Y
in-template       264192         1024
image-info        265216              n4TcazH7_MH5aBJhesK0tQ 1024
EOF
