#!/bin/bash
cp ../gdb.s ../gdb.tmp.s
echo "list *$1" >> ../gdb.tmp.s
gdb --command=../gdb.tmp.s

