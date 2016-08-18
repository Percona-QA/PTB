#!/bin/bash
./ptb_xtrabackup_test.sh \
	--defaults-file=./changed_page_bitmap_rqg.cfg \
        --mysql-rootdir=/mnt/bin/ps-5.5-changed-page-bitmap \
        --vardir=/mnt/var/cpb_rqg \
        --cachedir=/mnt/cache \
        --prepare-rootdir=/mnt/randgen \
        --load-rootdir=/mnt/randgen \
        --backup-rootdir=/mnt/bin/xb-2.1-xtradb55-changed-page-bitmap \
	--restore-rootdir=/mnt/bin/xb-2.1-xtradb55-changed-page-bitmap \
        $@
exit $?
