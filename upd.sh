#! /bin/sh
# [RA]
rm -rf jigdo
cvs -Q -d/usr/local/cvsroot co jigdo
export master=~/proj/jigdo
(
cd jigdo
find -type f|grep -v CVS \
| while read r;do
#    if test ! -f ../$r; then
	echo ln -n -f "$master/$r" "../$r"
	ln -n -f "$master/$r" "../$r"
#    fi
done
)
rm -rf jigdo
