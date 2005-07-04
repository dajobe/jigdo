# Functions for mktemplate-test*.sh
set -e
rm -rf mktemplate-testdir
mkdir mktemplate-testdir
cd mktemplate-testdir

# Write $1 bytes of random data to stdout
random() {
    ../util/random "$@"
#    dd if=/dev/urandom bs="$bs" count=1 2>/dev/null
}


if test "$1" = "all"; then
    shift 1
    mtargs="--report=noprogress --debug=make-template"
else
    mtargs="--report=quiet --debug=~general"
fi

mt() {
    #echo ../jigdo-file make-template -0 $mtargs --image=image "$@"
    if ../jigdo-file make-template -0 $mtargs --image=image "$@"; then
	../jigdo-file list-template --debug=~general \
	    --template=image.template >image.tlist
	return 0
    else
	return 1
    fi
}

# Provide on stdin the expected image.tlist
tlist() {
    if diff -u image.tlist -; then true; else
	echo "FAILED: image.tlist does not have expected contents!"
	return 1
    fi
}
