#!/bin/sh
touch -c aclocal.m4
sleep 1
touch -c configure
sleep 1
touch -c Makefile.in
sleep 1
touch -c pardiff/Makefile.in
sleep 1
touch -c config.h.in
