#!/bin/bash -x

cd ..
make

cd test_area

sudo umount dir2
sudo rmmod secondfs

sudo insmod ../secondfs.ko
sudo umount dir2
mkdir -p dir dir2 dir3
dd if=/dev/urandom of=new.img bs=512 count=2048
../mkfs.secondfs -D new.img

set -e

sudo mount -t secondfs -o loop new.img ./dir2

cd dir2
mkdir abc
mkdir abc/def abc/ghi abc/jkl
mkdir abc/def/123
touch abc/def/456.txt

ls -R
stat . abc abc/def abc/ghi abc/def/123 abc/def/456.txt

rm -rf ../dir3/*
cp -R * ../dir3/

cd ../dir3

ls -R
stat . abc abc/def abc/ghi abc/def/123 abc/def/456.txt

cd ../dir2

mv abc/def/456.txt abc/def/123/xxx.txt

ls -R
stat . abc abc/def abc/ghi abc/def/123 abc/def/123/xxx.txt

mv -T abc/def abc/ghi

ls -R
stat . abc abc/ghi abc/ghi/123 abc/ghi/123/xxx.txt

cd ..
sudo umount dir2
../fsck.secondfs new.img

sudo mount -t secondfs -o loop new.img ./dir2

cd dir2

ls -R
stat . abc abc/ghi abc/ghi/123 abc/ghi/123/xxx.txt

rm -rf abc

ls -R
stat .

cd ..
sudo umount dir2
../fsck.secondfs new.img

sudo rmmod secondfs