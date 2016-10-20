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
. ./include/xtrabackup_common.inc

###############################################################################
# Percona Test Bench - xtrabackup_incremental_backup                          #
###############################################################################

###############################################################################
# Performs backups - backups will be stored in 
#		     ${PTB_OPT_vardir}/backup-<cycle>-<incnum>. If backups used a 
#		     streaming destination, there will be a 'backup' file 
#		     within the directory that contains the entire redirected
#		     backup output.
function backup()
{
	local rpt_prefix="backup()"

	ptb_init $PTB_OPT_vardir $PTB_OPT_verbosity "${PTB_OPT_backup_logfile}"
	local rc=0

	if [ -n "$PTB_OPT_statistics_manager" ]; then
		ptb_init_statistics_manager $PTB_OPT_statistics_manager
	fi

	# build out the proper path to use for callng xtrabackup
	local xtrabackup_path="${S_BINDIR[$PTB_OPT_server_id]}/bin:${S_BINDIR[$PTB_OPT_server_id]}/libexec:$PTB_OPT_backup_rootdir"
	# build out the sleep schedule
	local sleep_schedule[0]=$BACKUP_OPT_cycle_delay
	if [ -z "$BACKUP_OPT_incremental_schedule" ]; then
		local i=
		for ((i=1; i <= BACKUP_OPT_incremental_count; i++)); do
			sleep_schedule[$i]=$BACKUP_OPT_incremental_delay
		done
	else
		local i=1
		local startpos=0
		local colonpos=0
		while true; do
			colonpos=`expr index ${BACKUP_OPT_incremental_schedule:$startpos} :`
			colonpos=`expr $startpos + $colonpos`
			if [ $colonpos -eq $startpos ]; then
				sleep_schedule[$i]=${BACKUP_OPT_incremental_schedule:$startpos}
				break
			else
				sleep_schedule[$i]=${BACKUP_OPT_incremental_schedule:$startpos:`expr $colonpos - $startpos - 1`}
			fi
			i=`expr $i + 1`
			startpos=$colonpos
		done

		# if it is a random schedule, lets randomize it, format is
		# x:RND:mincycles:maxcycles:mindelay:maxdelay
		if [ "${sleep_schedule[1]}" = "RND" ]; then
			local rnd_mincycles=${sleep_schedule[2]}
			local rnd_maxcycles=${sleep_schedule[3]}
			local rnd_mindelay=${sleep_schedule[4]}
			local rnd_maxdelay=${sleep_schedule[5]}
			local rnd_cycles=`expr $rnd_maxcycles - $rnd_mincycles`
			rnd_cycles=`expr $RANDOM % $rnd_cycles`
			rnd_cycles=`expr $rnd_cycles + $rnd_mincycles`

			unset sleep_schedule
			local sleep_schedule[0]=$BACKUP_OPT_cycle_delay
			local i=
			for ((i=1; i <= rnd_cycles; i++)); do
				sleep_schedule[$i]=`expr $rnd_maxdelay - $rnd_mindelay`
				sleep_schedule[$i]=`expr $RANDOM % ${sleep_schedule[$i]}`
				sleep_schedule[$i]=`expr ${sleep_schedule[$i]} + $rnd_mindelay`
			done
			ptb_report_info "$rpt_prefix - Using randomized schedule with ${#sleep_schedule[@]} cycles and ${sleep_schedule[@]} delays"
		else
			ptb_report_info "$rpt_prefix - Using specified schedule with ${#sleep_schedule[@]} cycles and ${sleep_schedule[@]} delays"
		fi
	fi

	#parse the options and build out the correct xtrabackup command line
	xtrabackup_common_parse_command_options
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - xtrabackup_common_parse_command_options failed with $rc"
		return $rc
	fi

	local backup_command="--defaults-file=${S_DEFAULTSFILE[$PTB_OPT_server_id]}"
	backup_command="$backup_command --no-timestamp"
	backup_command="$backup_command --tmpdir=$PTB_OPT_vardir"
	backup_command="$backup_command $xb_backup_command_options"

	ptb_report_info "$rpt_prefix - Beginning backup test with basic options ${backup_command}"

	# loop through full backups
	local full_cycle=0
	while [ $full_cycle -lt $BACKUP_OPT_cycle_count ]; do
		# loop through 1 + incremental count making either a full or incremental backup for each cycle
		local current_cycle=0
		local previous_cycle=0
		local to_lsn=
		while [ $current_cycle -lt ${#sleep_schedule[@]} ]; do
			local backup_current_dir=${PTB_OPT_vardir}/backup-${full_cycle}.${current_cycle}
			local backup_previous_dir=${PTB_OPT_vardir}/backup-${full_cycle}.${previous_cycle}
			local cycle_logfile="${backup_current_dir}.log"

			if [ $BACKUP_OPT_dryrun -gt 0 ]; then
				previous_cycle=$current_cycle
				current_cycle=`expr ${current_cycle} + 1`
				continue
			fi

			# sleep according to schedule
			ptb_report_info "$rpt_prefix - Sleeping for ${sleep_schedule[$current_cycle]} seconds before starting cycle $current_cycle"
			sleep ${sleep_schedule[$current_cycle]}

			# add in some bread crumbs for validation
			local mark_data="${full_cycle}.${current_cycle}"
			xtrabackup_common_add_mark_data $PTB_OPT_server_id "$mark_data"
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - xtrabackup_common_add_mark_data(${PTB_OPT_server_id}, \"${mark_data}\") failed with $rc"
				break
			fi

			local inc_backup_command=""
			if [ $current_cycle -ne 0 ]; then
				ptb_report_info "Backup - estimated to_lsn $to_lsn"
				inc_backup_command="--incremental-lsn=$to_lsn"
			fi

			#ptb_sql $serverid "SHOW STATUS LIKE 'innodb_buffer_pool%'"

			# mark our start in the logfile
			ptb_report_info "$rpt_prefix - starting: full backup cycle=${full_cycle} incremental backup cycle=${current_cycle}"
			ptb_report_info "$rpt_prefix - options: ${PTB_OPT_backup_command_option[@]}"
			ptb_report_info "$rpt_prefix - path: ${xtrabackup_path}"
			ptb_report_info "$rpt_prefix - command: xtrabackup $backup_command $inc_backup_command"

			if [ $BACKUP_OPT_drop_caches -ne 0 ]; then
				ptb_report_info "$rpt_prefix - flushng caches..."
				sync
				echo 3 > /proc/sys/vm/drop_caches
			fi

			local backup_start_time=$SECONDS

			if [ $BACKUP_OPT_dev_null -ne 0 ]; then
				ptb_report_info "$rpt_ptrfix - ( PATH=${xtrabackup_path}:$PATH; xtrabackup $backup_command $inc_backup_command --stream=xbstream --target-dir=$backup_current_dir --backup 1>/dev/null )"
				( PATH=${xtrabackup_path}:$PATH; xtrabackup $backup_command $inc_backup_command --stream=xbstream --target-dir=$backup_current_dir --backup 1>/dev/null 2> $cycle_logfile )
			else
				ptb_report_info "$rpt_ptrfix - ( PATH=${xtrabackup_path}:$PATH; xtrabackup $backup_command $inc_backup_command --target-dir=$backup_current_dir  --backup )"
				( PATH=${xtrabackup_path}:$PATH; xtrabackup $backup_command $inc_backup_command --target-dir=$backup_current_dir --backup 2> $cycle_logfile )
			fi
			rc=$?
			local backup_total_time=`expr $SECONDS - $backup_start_time`

			# mark the completion in the logfile
			ptb_report_info "$rpt_prefix - completed: time ${backup_total_time} full backup cycle=${full_cycle} incremental backup cycle=${current_cycle} rc=$rc"

			# update statistics
			ptb_stat_register_row $backup_current_dir
			local row_id=$PTB_STAT_RESULT
			if [ $row_id -gt 0 ]; then # Trapped to https://github.com/Percona-QA/PTB/issues/12
				ptb_stat_set_cell_value_from_options "$row_id" ${PTB_OPT_server_option[@]} ${PTB_OPT_backup_command_option[@]} $xtrabackup_options "backup-time=${backup_total_time}"
			fi

			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - xtrabackup($backup_command $inc_backup_command --target-dir=$backup_current_dir --backup) failed with $rc"
				break
			fi

			# pull the lsn
			if [ $BACKUP_OPT_dev_null -eq 0 ]; then
				to_lsn=`grep 'The latest check point' $cycle_logfile`
				rc=$?
				if [ $rc -ne 0 ]; then
					ptb_report_error "$rpt_prefix - could not fild checkpoint lsn in log file[$cycle_logfile], failed with $rc."
					break
				fi
				to_lsn=${to_lsn:`expr match "${to_lsn}" '[^0-9]*'`}
				to_lsn=`expr match "${to_lsn}" '\([0-9]*\)'`

				ptb_report_info "$rpt_prefix - found to_lsn [${to_lsn}]"
			fi

			ptb_report_file_info $cycle_logfile
			ptb_runcmd rm -f $cycle_logfile

			previous_cycle=$current_cycle
			current_cycle=`expr ${current_cycle} + 1`
		done

		if [ $rc -ne 0 ]; then
			break
		fi

		full_cycle=`expr ${full_cycle} + 1`
	done

	ptb_cleanup 0

	return $rc
}
################################################################################
# Shows usage
function usage()
{
	echo "XtraBackup - incremental backup test"
	echo "Options:"
	ptb_usage "PTB"
	
	echo "--backup-option options:"
	ptb_usage "BACKUP"
	exit 1
}
################################################################################
# Validation callback for options parsing
# $1 - option name
# $2 - proposed option value
function getoption()
{
	case "$1" in
	"incremental_count" | "incremental_wait" )
		if [ -n "$BACKUP_OPT_incremental_schedule" ]; then
			ptb_report_error "incremental-schedule and $1 are mutually exclusive."
			return 2
		fi
		;;
	"incremental_schedule" )
		if [ -n "$BACKUP_OPT_incremental_count" ]; then
			ptb_report_error "incremental-count and $1 are mutually exclusive."
			return 2
		elif [ -n "$BACKUP_OPT_incremental_wait" ]; then
			ptb_report_error "incremental-wait and $1 are mutually exclusive."
			return 2
		fi
		;;
	* )
		;;
	esac
	return 0
}

PTB_OPTION_DESCRIPTORS=(\
	"server-id REQ 1 INT 0 9999 1 Server ID to backup from." \
	"vardir REQ 1 PATHEXISTS Directory where individual test and data results should be located." \
	"statistics-manager OPT 1 STR Name of pipe to communicate with statistics manager." \
	"pidfile OPT 1 STR Name of pidfile." \
	"verbosity OPT 1 INT 0 4 2 Output verbosity filter: ERROR=4; WARNING=3; INFO=2; DEBUG=1; IDEBUG=0." \
	"backup-rootdir OPT 1 STR Directory where the XtraBackup binaries are located." \
	"backup-logfile OPT 1 STR Log file name for backup operation." \
	"backup-option OPT 0 STR Extra backup test options." \
	"backup-command-option OPT 0 STR Extra backup command options." \
	"server-option OPT 0 STR Server options." \
)
BACKUP_OPTION_DESCRIPTORS=(\
	"cycle-count OPT 1 INT 1 9999 1 Number of full backups to perform." \
	"cycle-delay OPT 1 INT 0 9999 0 Amount of time (in seconds) to wait in before each full backup." \
	"dev-null OPT 1 INT 0 1 0 Force backup output to /dev/null with xbstream format. No restore validation is possible." \
	"drop-caches OPT 1 INT 0 1 0 Force linux to flush file caches before each backup." \
	"incremental-count OPT 1 INT 0 9999 0 Number of incremental backups to perform after each full backup. Can not be used with test-incremental-schedule." \
	"incremental-delay OPT 1 INT 0 9999 0 Amount of time (in seconds) to wait before performing each subsequent incremental backups. Can not be used with test-incremental-schedule." \
	"incremental-schedule OPT 1 STR Schedule of time delays to perform incremental backups after each full backup delimited by ':'. Use in place of test-incremental-count and test-incremental-wait. Ex: A value of 0:20:60:120 will perform the first incremental 0 seconds after completing a full backup, the second incremental will be performed 20 seconds after completion of the previous incremental, etc... A randomized schedule can be generated for each invocation by using the format RND:mincycles:maxcycles:mindelay:maxdelay" \
	"dryrun OPT 1 INT 0 1 0 Parse options and report as if running tests but do not actually execute any test." \
)

# parse general option descriptors and set up option array
ptb_parse_option_descriptors "PTB_OPTION_DESCRIPTORS" "PTB"

#parse backup-option descriptors and set up option array
ptb_parse_option_descriptors "BACKUP_OPTION_DESCRIPTORS" "BACKUP"

# parse command line args
ptb_parse_options getoption usage "PTB" $@

# validate command line args
ptb_validate_options usage "PTB"

# parse command line args
ptb_parse_options getoption usage "BACKUP" ${PTB_OPT_backup_option[@]}

# validate command line args
ptb_validate_options usage "BACKUP"

# debugging
ptb_show_option_values "PTB"
ptb_show_option_values "BACKUP"
#exit 0
if [ -n "$PTB_OPT_pidfile" ]; then
	echo $$ > $PTB_OPT_pidfile
fi

backup
rc=$?

if [ -n "$PTB_OPT_pidfile" ]; then
	rm -f $PTB_OPT_pidfile
fi

exit $rc
