#!/bin/bash
set -f
set +e

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

###############################################################################
# Percona Test Bench - XtraBackup performance matrix                          #
###############################################################################
# This script implements a basic backup pattern with the following optional
# details:
#    - prepare: database prepoaration/preload phase
#    - load: database work load / parallel execution with backups
#    - backup: database backup
#    - restore: database restore and validation
#    - cleanup: test result cleanup
#
# This script allows for the specification of both server and backup options.
# Each option may have multiple values. This script will multiply out all of
# the possible combinations of values for both the server and backup and then
# run a complete test cycle for each resulting combination.
#
# This script allows for the passing of extra options to each of detail scripts
# to help customize and manage the complete test.
###############################################################################

###############################################################################
# Runs a single test with a single combination of server and xtrabackup
# options
#
# $1 - required, test number
# $2 - required, test base dir
# $3 - required, base port
# $4 - required, statistics manager pipe
# $5 - required, server options
# $6 - required, xtrabackup options
function runonetest()
{
	local test_number=$1
	local test_dir=$2
	local base_port=$3
	local stat_pipe=$4
	local server_options="$5"
	local xtrabackup_options="$6"

	local rpt_prefix="runonetest($1, $2, $3, $4, $5, $6)"
	local server_id=1
	local load_id=1
	local server_port=$base_port
	local prepare_logfile=${test_dir}/prepare.log
	local load_logfile=${test_dir}/load.log
	local backup_logfile=${test_dir}/backup.log
	local restore_logfile=${test_dir}/restore.log
	local rc=0

	# initialize the sandbox and variables
	ptb_init $test_dir $PTB_OPT_verbosity

	ptb_report_info "$rpt_prefix - starting test"

	# if there is something useful to do with a server instance
	if [ -n "$PTB_OPT_prepare" ] || [ -n "$PTB_OPT_load" ] || [ -n "$PTB_OPT_backup" ]; then
		# create a server instance
		ptb_create_server_instance $server_id $PTB_OPT_mysql_rootdir $server_port
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_create_server_instance($server_id, $PTB_OPT_mysql_rootdir, $server_port) failed with $rc"
			return $rc
		fi

		# set the server options
		ptb_set_all_server_options $server_id "$server_options"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_set_all_server_options($server_id, $server_options) failed with $rc"
			return $rc
		fi

		# prepare the database
		ptb_prepare_server_data $server_id "$PTB_OPT_cachedir" "$PTB_OPT_prepare" "$PTB_OPT_prepare_rootdir" "$prepare_logfile" "${PTB_OPT_prepare_option[@]}"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_prepare_server_data(${server_id}, ${PTB_OPT_cachedir}, ${PTB_OPT_prepare}, ${PTB_OPT_prepare_rootdir}, ${prepare_logfile}, ${PTB_OPT_prepare_options[@]}) failed with $rc."
			ptb_cleanup 1
			return $rc
		fi
	fi
	
	# start any parallel load
	if [ -n "$PTB_OPT_load" ]; then
		local load_options="${PTB_OPT_load_option[@]}"
		ptb_start_load $server_id $load_id $PTB_OPT_load $PTB_OPT_load_rootdir "$load_logfile" "$load_options" 
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_start_test_load(${server_id}, ${load_id}, ${PTB_OPT_test_load}, ${PTB_OPT_test_rootdir}, ${load_logfile}, ${PTB_OPT_test_load_opt[@]}) failed with $rc"
			ptb_cleanup 1
			return $rc
		fi
	fi

	# if we are supposed to backup, do it!
	if [ -n "$PTB_OPT_backup" ]; then
		# sleep while load loads up
		ptb_report_info "$rpt_prefix - Sleeping for ${PTB_OPT_backup_wait} seconds before starting backups."
		sleep $PTB_OPT_backup_wait

		# do the backup
		local backup_options="${PTB_OPT_backup_option[@]}"
		ptb_report_info "$rpt_prefix - backup_options[$backup_options] xtrabackup_options[${xtrabackup_options}]"
		ptb_run_backup $server_id "$PTB_OPT_backup" "$PTB_OPT_backup_rootdir" "$backup_logfile" "$stat_pipe" "$server_options" "$backup_options" "$xtrabackup_options"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_run_backup(${server_id}, ${PTB_OPT_backup}, ${PTB_OPT_backup_rootdir}, ${backup_logfile}, ${stat_pipe}, ${server_options}, ${backup_options}, ${xtrabackup_options}) failed with $rc"
			ptb_cleanup 1
			return $rc
		fi

	fi

	if [ -n "$PTB_OPT_restore" ]; then
		# sleep while load loads up
		ptb_report_info "$rpt_prefix - Sleeping for ${PTB_OPT_restore_wait} seconds before starting restores."
		sleep $PTB_OPT_restore_wait
	fi

	# shut 'er down
	if [ -n "$PTB_OPT_load" ]; then
		ptb_kill_task $load_id
	fi

	if [ -n "$PTB_OPT_prepare" ] || [ -n "$PTB_OPT_load" ] || [ -n "$PTB_OPT_backup" ]; then
		ptb_stop_server $server_id
	fi

	# if we are supposed to validate, go though and do the restores
	if [ -n "$PTB_OPT_restore" ]; then
		local restore_options="${PTB_OPT_restore_option[@]}"
		ptb_run_restore $server_id $PTB_OPT_restore "$PTB_OPT_restore_rootdir" $restore_logfile "$stat_pipe" "$restore_options" "$xtrabackup_options"
		rc=$?
	fi

	# cleanup server data and backups on success
	if [ $rc -eq 0 ] && [ -n "$PTB_OPT_cleanup" ]; then
		ptb_run_cleanup $server_id $PTB_OPT_cleanup "" "" ""
	fi

	# cleanup the sandbox and variables
	ptb_cleanup 1

	return $rc
}
###############################################################################
# The main event
function main()
{
	local rpt_prefix="main()"
	local failures=0

	# set up working directory
	if [ -z "$PTB_OPT_vardir" ]; then
		PTB_OPT_vardir="$PWD/var"
	fi
	if [ -d "$PTB_OPT_vardir" ] && [ $PTB_OPT_keep_data -eq 0 ]; then
		local newdiridx=1
		local newdirname="$PTB_OPT_vardir"
		while [ -d "${newdirname}" ]; do
			newdirname="${PTB_OPT_vardir}.${newdiridx}"
			newdiridx=`expr $newdiridx + 1`
		done
		ptb_runcmd mv "$PTB_OPT_vardir" "$newdirname"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - unable to move old vardir \"${PTB_OPT_vardir}\" out of the way to \"${newdirname}\", failed with $rc."
			return $rc
		fi
	fi
	if [ ! -d "$PTB_OPT_vardir" ]; then
		ptb_runcmd mkdir -p $PTB_OPT_vardir
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - unable to create vardir \"${PTB_OPT_vardir}\", failed with $rc."
			return $rc
		fi
	fi

	#set up statistics manager
	local stat_pipe="${PTB_OPT_vardir}/stat.man.in"
	ptb_init_statistics_manager $stat_pipe	
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - unable to initialize statistics_manager on \"${stat_pipe}\", failed with $rc."
		return $rc
	fi

	#parse server options into individual sets
	ptb_parse_options_matrix PTB_OPT_mysql_option mysql_options

	#parse xtrabackup options into individual sets
	ptb_parse_options_matrix PTB_OPT_xtrabackup_option xtrabackup_options

	ptb_report_info "Total: `expr ${#mysql_options[@]} * ${#xtrabackup_options[@]}` combinations"

	# loop through the combinations and run each test
	local test_number=0
	local base_port=10000
	local mysql_option_set=
	for mysql_option_set in "${mysql_options[@]}"; do
		local xtrabackup_option_set=
		for xtrabackup_option_set in "${xtrabackup_options[@]}"; do
			local test_dir=${PTB_OPT_vardir}/test-${test_number}
			local test_logfile="${test_dir}/test.log"
			local trys_remaining=`expr $PTB_OPT_retry + 1`
			local rc=1
			ptb_report_info "$rpt_prefix - Starting test $test_number with [$mysql_option_set] and [$xtrabackup_option_set]"
			if [ $PTB_OPT_dryrun -gt 0 ]; then
				continue
			fi

			while [ $rc -ne 0 ] && [ $trys_remaining -gt 0 ]; do
				if [ ! -d "$test_dir" ]; then
					ptb_runcmd mkdir -p $test_dir
					rc=$?
					if [ $rc -ne 0 ]; then
						ptb_report_error "$rpt_prefix - unable to create test directory $test_dir, failed with $rc."
						return $rc
					fi
				fi

				runonetest $test_number $test_dir $base_port $stat_pipe "$mysql_option_set" "$xtrabackup_option_set" > $test_logfile
				rc=$?

				# dump the stats now before any retry
				ptb_stat_write_to_file "${PTB_OPT_vardir}/results.csv"

				if [ $rc -ne 0 ]; then
					local newdiridx=2
					local newdirname="${test_dir}.failure.1"
					while [ -d "${newdirname}" ]; do
						newdirname="${test_dir}.failure.${newdiridx}"
						newdiridx=`expr $newdiridx + 1`
					done
					ptb_report_info "$rpt_prefix - Test $test_num with [$mysql_option_set] and [$xtrabackup_option_set] FAILED with $rc."
					ptb_runcmd mv "$test_dir" "$newdirname"
					rc=$?
					if [ $rc -ne 0 ]; then
						ptb_report_error "$rpt_prefix - unable to move old testdir \"${test_dir}\" out of the way to \"${newdirname}\", failed with $rc."
						return $rc
					fi
				fi
				trys_remaining=`expr $trys_remaining - 1`
			done
			failures=`expr $failures + $rc`
			if [ $rc -ne 0 ] && [ $PTB_OPT_force -ne 0 ]; then
				return 1
			fi
			test_number=`expr $test_number + 1`
		done
	done
	if [ $failures -ne 0 ]; then
		return 1
	else
		return 0
	fi
}
################################################################################
# Shows usage prolog
function usage()
{
	echo "XtraBackup performance matrix test."
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
	case "$1" in
	* )
		;;
	esac
	return 0
}

PTB_OPTION_DESCRIPTORS=(\
	"mysql-rootdir REQ 1 PATHEXISTS Parent or install directory where mysql client and server binaries can be found. Ex: /usr/bin/mysql" \
	"mysql-option OPT 0 STR Option name and values to be used to generate server option matrix. Ex: --mysql-option=innodb_file_per_table=0 1" \
	"include OPT 0 FILEEXISTS Script(s) to source after options have been processed and before testing starts that will declare and populate the required internal variables (PTB_mysql_matrix variable, PTB_xtrabackup_matrix). Ex: ps55-xb21-bitmap.cfg" \
	"cachedir OPT 1 PATHCREATE Directory where server databases may be cached to save time during prepare phase." \
	"vardir OPT 1 STRDEF ./var Directory where individual test data and results should be located." \
	"prepare OPT 1 FILEEXISTS Script to call that will prepare the database server prior to each test. Ex: ./include/sysbench_oltp_prepare.sh" \
	"prepare-rootdir OPT 1 PATHEXISTS Directory where test prepare tool binaries can be found (RQG, sysbench, tpcc, etc..)." \
	"prepare-option OPT 0 STR Option to be passed to prepare tool. Ex: --prepare-option=--oltp-table-count=16 --prepare-option=--oltp-table-size=100000" \
	"load OPT 1 FILEEXISTS Script to call that will run load on the server during each test. Ex: ./include/sysbench_oltp_load.sh" \
	"load-rootdir OPT 1 PATHEXISTS Directory where test load tool binaries can be found (RQG, sysbench, tpcc, etc..)." \
	"load-option OPT 0 STR Option to be passed to load tool. Ex: --load-option=--oltp-table-count=16 --load-option=--oltp-table-size=100000" \
	"backup OPT 1 STR Script to call that will perform backup. Ex: ./include/xtrabackup_incremental_backup.sh" \
	"backup-rootdir OPT 1 PATHEXISTS Directory where backup binaries can be found." \
	"backup-wait OPT 1 INT 0 9999 0 Amount of time (in seconds) to wait after load has started before starting backup cycles." \
	"backup-option OPT 0 STR Option to be passed to backup script. Ex: --backup-option=--cycle-count=3 --backup-option=--cycle-delay=30 --backup-option=--incremental-count=6 --backup-option=--incremental-delay=60" \
	"xtrabackup-option OPT 0 STR Option name and values to be used to generate backup option matrix. Ex: --backup-command-option=--parallel=1 2 4 8" \
	"restore OPT 1 STR Script to call to perform restore. Ex: ./include/xtrabackup_incremental_restore_and_validate.sh" \
	"restore-rootdir OPT 1 PATHEXISTS Directory where restore binaries can be found." \
	"restore-wait OPT 1 INT 0 9999 0 Amount of time (in seconds) to wait after the backup cycle before starting restore cycles." \
	"restore-option OPT 0 STR Option to be passed to restore script." \
	"cleanup OPT 1 STR Script to call to perform data cleanup after each test. Ex: ./include/xtrabackup_cleanup.sh" \
	"retry OPT 1 INT 0 9999 0 Number of attempts to retry each test after initial failure." \
	"force OPT 1 INT 0 1 0 Force continuation of other tests after retry has been exhausted." \
	"dryrun OPT 1 INT 0 1 0 Parse options and report as if running tests but do not actually execute any test." \
	"verbosity OPT 1 INT 0 4 2 Output verbosity filter: ERROR=4; WARNING=3; INFO=2; DEBUG=1; IDEBUG=0." \
	"keep-data OPT 1 INT 0 1 0 Keeps existing var and test dirs in place and executes test on top of them." \
)
# parse option descriptors and set up option array
ptb_parse_option_descriptors "PTB_OPTION_DESCRIPTORS" "PTB"

# parse command line args
ptb_parse_options getoption usage "PTB" "$@"

# validate command line args
ptb_validate_options usage "PTB" $@

# debugging
ptb_show_option_values "PTB"
#exit 0

# source in our include configurations 
for i in "${PTB_OPT_include[@]}"; do
	. $i
done

main
main_failed=$?
ptb_deinit_statistics_manager
exit $main_failed
