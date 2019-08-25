#!/bin/sh
# As git does not preserve file timestamps, autotools usually would be
# triggered to run right after git clone.
# This script patches file timestamps to prevent autotools from running.
touch -c aclocal.m4
sleep 1
touch -c configure
sleep 1
touch -c Makefile.in
sleep 1
touch -c pardiff/Makefile.in
sleep 1
touch -c config.h.in
