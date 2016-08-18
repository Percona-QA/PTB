#!/bin/bash
./ptb_xtrabackup_test.sh \
	--defaults-file=./log-archive-test.cfg \
        --mysql-rootdir=/mnt/bin/ps-5.6 \
        --vardir=/mnt/test/var \
        --cachedir=/mnt/test/cache \
        --prepare-rootdir=/mnt/randgen \
        --load-rootdir=/mnt/randgen \
        --backup-rootdir=/mnt/bin/xb-2.1-xtradb56-log-archive \
	--restore-rootdir=/mnt/bin/xb-2.1-xtradb56-log-archive \
        $@
exit $?

./ptb_xtrabackup_test.sh \
	--defaults-file=./log-archive-test.cfg \
        --mysql-rootdir=/mnt/bin/ps-5.6 \
        --vardir=/mnt/test/var \
        --cachedir=/mnt/test/cache \
        --prepare-rootdir=/mnt/sysbench/sysbench \
        --load-rootdir=/mnt/sysbench/sysbench \
        --backup-rootdir=/mnt/bin/xb-2.1-xtradb56-log-archive \
	--restore-rootdir=/mnt/bin/xb-2.1-xtradb56-log-archive \
        $@

