#!/bin/bash

################################################################################
# Percona Test Bench (c) 2013 Percona Ireland Ltd
################################################################################
# Originally Created 04/2013 George Ormond Lorch III
# Written by George Ormond Lorch III
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# Launchpad homepage:
# * https://launchpad.net/percona-test-bench
#
# Bazaar code repository:
# * lp:percona-test-bench
#
# LICENSE
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License. Please see the
# LICENSE file for information about licensing and use restrictions of
# this software.
################################################################################

. ./include/ptb_core.inc

#########################################################
# Percona Test Bench - sysbench_oltp_prepare            #
#########################################################

################################################################################
# Runs sysbench prepare

COMPRESSED_COLUMN=1

function prepare()
{
	local rpt_prefix="prepare()"
	ptb_init $PTB_OPT_vardir $PTB_OPT_verbosity "$PTB_OPT_prepare_logfile"

	local rc=1
	local sysbench_test="${PTB_OPT_prepare_rootdir}/tests/db/parallel_prepare.lua" 
	if [ ! -r "$sysbench_test" ]; then
		ptb_report_error "$rpt_prefix - can not access sysbench test $sysbench_test" 
		rc=$PTB_RET_INVALID_ARGUMENT
		ptb_cleanup 0
		return $rc
	fi

	ptb_sql $PTB_OPT_server_id "DROP DATABASE IF EXISTS sbtest"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
		ptb_cleanup 0
		return $rc
	fi

	ptb_sql $PTB_OPT_server_id "CREATE DATABASE sbtest"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
		ptb_cleanup 0
		return $rc
	fi
	##########################################################################################
	# START
	# This is temporary fix for : https://github.com/Percona-QA/PTB/issues/10
	# Date: 18 october 2016
	# Future improvement -> add command parsing option for user and password in xtrabackup_common.inc
	# (Also see xtrabackup_incremental_backup.sh)
	ptb_sql $PTB_OPT_server_id "CREATE user 'jenkins'@'localhost'"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
		ptb_cleanup 0
		return $rc
	fi
	
	ptb_sql $PTB_OPT_server_id "GRANT process, reload, super, replication client on *.* to 'jenkins'@'localhost'"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
		ptb_cleanup 0
		return $rc
	fi
	# END
	############################################################################################


	##########################################################################################
	# START
	# This is temporary fix for : https://github.com/Percona-QA/PTB/issues/13
	# Date: 25 october 2016
	# Adding compression column with pre-defined dictionary support
	if [ $COMPRESSED_COLUMN -ne 0 ]; then
		ptb_sql $PTB_OPT_server_id "CREATE COMPRESSION_DICTIONARY numbers ('08566691963-88624912351-16662227201-46648573979-64646226163-77505759394-75470094713-41097360717-15161106334-50535565977') 
"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
			ptb_cleanup 0
			return $rc
		fi
	
		ptb_sql $PTB_OPT_server_id "alter table sbtest.sbtest1 modify c varchar(250) column_format compressed with compression_dictionary numbers"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
			ptb_cleanup 0
			return $rc
		fi
	fi
	# END
	############################################################################################	

	local sysbench_cmd="sysbench --test=$sysbench_test --mysql-socket=${S_SOCKET[$PTB_OPT_server_id]} --mysql-user=root"

	local i
	for i in ${PTB_OPT_prepare_option[@]}; do
		sysbench_cmd="${sysbench_cmd} $i"
	done
	sysbench_cmd="${sysbench_cmd} run"

	if [ -n "$PTB_OPT_prepare_logfile" ]; then
		echo "Running: $sysbench_cmd" >> $PTB_OPT_prepare_logfile
		ptb_runcmdpathlog "$PTB_OPT_prepare_rootdir" "$PTB_OPT_prepare_logfile" $sysbench_cmd
	else
		echo "Running: $sysbench_cmd"
		ptb_runcmdpath "$PTB_OPT_prepare_rootdir" $sysbench_cmd
	fi
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - ptb_runcmdpath sysbench failed with $rc."
		ptb_cleanup 0
		return $rc
	fi

	ptb_cleanup 0
	return $rc
}
################################################################################
# Generates test descriptor based on passed sysbench arguments
function gen_descriptor()
{
	local oltp_tables_count="--oltp-tables-count="
	local oltp_table_size="--oltp-table-size="
	local oltp_secondary="--oltp-secondary="
	local oltp_auto_inc="--oltp-auto-inc="
	local mysql_table_engine="--mysql-table-engine="

	local tables=
	local rows=
	local secondary=
	local auto_inc=
	local i=
	for i in ${PTB_OPT_prepare_option[@]}; do
		if [ "${i:0:${#oltp_tables_count}}" = "$oltp_tables_count" ]; then
			tables=${i:${#oltp_tables_count}}
		elif [ "${i:0:${#oltp_table_size}}" = "$oltp_table_size" ]; then
			rows=${i:${#oltp_table_size}}
		elif [ "${i:0:${#oltp_secondary}}" = "$oltp_secondary" ]; then
			secondary=${i:${#oltp_secondary}}
		elif [ "${i:0:${#oltp_auto_inc}}" = "$oltp_auto_inc" ]; then
			auto_inc=${i:${#oltp_auto_inc}}
		elif [ "${i:0:${#mysql_table_engine}}" = "$mysql_table_engine" ]; then
			mysql_table_engine=${i:${#mysql_table_engine}}
		fi
	done
	local desc="sysbench_oltp_prepare_${tables}_${rows}_${secondary}_${auto_inc}_${mysql_table_engine}"
	if [ -n "$PTB_OPT_prepare_logfile" ]; then
		echo $desc > $PTB_OPT_prepare_logfile
	else
		echo $desc
	fi
	return 0
}
################################################################################
# Shows usage prolog
function usage()
{
	echo " RQG - prepare"
	echo "Options:"
	ptb_usage "PTB"
	exit 1
}
################################################################################
# Validation callback for options parsing
# $1 - option name
# $2 - proposed option value
function getoption()
{
	local a=
	# don't need any extra validation here as this should always be called from
	# another test script via ptb_run_test_prepare
}

PTB_OPTION_DESCRIPTORS=(\
	"server-id REQ 1 INT 0 9999 1 Server ID to put load on." \
	"vardir REQ 1 PATHEXISTS Directory where individual test and data results should be located." \
	"pidfile OPT 1 STR Name of pidfile." \
	"verbosity OPT 1 INT 0 4 2 Output verbosity filter: ERROR=4; WARNING=3; INFO=2; DEBUG=1; IDEBUG=0." \
	"gen-descriptor OPT 1 INT 0 1 0 Generate a descriptor for the database shape only or run prepare." \
	"prepare-rootdir OPT 1 STR Directory where sysbench binaries are located." \
	"prepare-logfile OPT 1 STR Log file name for prepare operation." \
	"prepare-option OPT 0 STR Extra options to pass to RQG." \
)

# parse general option descriptors and set up option array
ptb_parse_option_descriptors "PTB_OPTION_DESCRIPTORS" "PTB"

# parse command line args
ptb_parse_options getoption usage "PTB" $@

# validate command line args
ptb_validate_options usage "PTB"

# debugging
#ptb_show_option_values "PTB"

if [ -n "$PTB_OPT_pidfile" ]; then
	echo $$ > $PTB_OPT_pidfile
fi

if [ -n "$PTB_OPT_gen_descriptor" ] && [ $PTB_OPT_gen_descriptor -gt 0 ]; then
	gen_descriptor
else
	prepare
fi

rc=$?

if [ -n "$PTB_OPT_pidfile" ]; then
	rm -f $PTB_OPT_pidfile
fi

exit $rc
