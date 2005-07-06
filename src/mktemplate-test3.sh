. $srcdir/mktemplate-funcs.sh

# Check for the --(no-)greedy-matching option
random 1k >in1
random 100 >in2
cat in1 >>in2
random 100 >>in2
random 1k >image
cat in2 >>image
random 1k >>image

mt --greedy-matching in* 
tlist <<EOF
in-template            0         1124
need-file           1124         1024 Mvxiwrh2fyjAM5pc-SqVmw bqRXnKaA3-Y
in-template         2148         1124
image-info          3272              VpC5LRjWgSa2F9k3qU1K4g 1024
EOF

mt -f --no-greedy-matching in* 
tlist <<EOF
in-template            0         1024
need-file           1024         1224 XZIwc2lniIK-aC--xQjStw xrU6ZA7646M
in-template         2248         1024
image-info          3272              VpC5LRjWgSa2F9k3qU1K4g 1024
EOF
