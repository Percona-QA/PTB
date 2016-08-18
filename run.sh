#!/bin/bash
MYSQL_INSTALL_DIR=/data/bin/jen-80/ps-5.6
XTRABACKUP_INSTALL_DIR=/data/bin/jen-80/xb-2.2
RQG_INSTALL_DIR=/home/glorch/dev/RQG
DATA_DIR=/data/dev/jen-80

./ptb_xtrabackup_test.sh \
	--defaults-file=./xb_basic.test \
        --mysql-rootdir=${MYSQL_INSTALL_DIR} \
        --vardir=${DATA_DIR} \
        --prepare-rootdir=${RQG_INSTALL_DIR} \
	--prepare=./include/rqg_prepare.sh \
        --load-rootdir=${RQG_INSTALL_DIR} \
	--load=./include/rqg_load.sh \
        --backup-rootdir=${XTRABACKUP_INSTALL_DIR} \
	--restore-rootdir=${XTRABACKUP_INSTALL_DIR} \
        $@
exit $?
