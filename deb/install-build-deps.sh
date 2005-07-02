#! /bin/sh

cmd() { echo "$@"; "$@"; }

cmd apt-get install \
  debhelper \
  zlib1g-dev \
  libbz2-dev \
  libdb4.2-dev \
  libgtk2.0-dev \
  libcurl3-dev
