#!/bin/bash
./ptb_xtrabackup_test.sh \
	--defaults-file=./changed_page_bitmap_sysbench.cfg \
        --mysql-rootdir=/mnt/bin/ps-5.5-changed-page-bitmap \
        --vardir=/mnt/var/cpb_sysbench \
        --cachedir=/mnt/cache \
        --prepare-rootdir=/mnt/sysbench/sysbench \
        --load-rootdir=/mnt/sysbench/sysbench \
        --backup-rootdir=/mnt/bin/xb-2.1-xtradb55-changed-page-bitmap \
	--restore-rootdir=/mnt/bin/xb-2.1-xtradb55-changed-page-bitmap \
        $@
exit $?
