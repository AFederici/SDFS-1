#!/bin/bash

dd if=/dev/urandom of=1x1.txt bs=1024*1024 count=1
dd if=/dev/urandom of=1x2.txt bs=1024*1024 count=1
dd if=/dev/urandom of=2x1.txt bs=1024*1024 count=2
dd if=/dev/urandom of=2x2.txt bs=1024*1024 count=2
dd if=/dev/urandom of=3x1.txt bs=1024*1024 count=3
dd if=/dev/urandom of=3x2.txt bs=1024*1024 count=3
dd if=/dev/urandom of=4x1.txt bs=1024*1024 count=4
dd if=/dev/urandom of=4x2.txt bs=1024*1024 count=4
dd if=/dev/urandom of=5x1.txt bs=1024*1024 count=5
dd if=/dev/urandom of=5x2.txt bs=1024*1024 count=5
dd if=/dev/urandom of=10x1.txt bs=1024*1024 count=10
dd if=/dev/urandom of=10x2.txt bs=1024*1024 count=10
dd if=/dev/urandom of=20x1.txt bs=1024*1024 count=20
dd if=/dev/urandom of=20x2.txt bs=1024*1024 count=20
