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
# Percona Test Bench - xtrabackup_log_archive_restore_and_validate            #
###############################################################################


##############################################################################
# Performs restores - expects backups to be stored in
#		     ${PTB_OPT_vardir}/backup-<cycle>-<incnum>.
function restore()
{
	local rpt_prefix="restore()"

	ptb_init $PTB_OPT_vardir $PTB_OPT_verbosity "$PTB_OPT_restore_logfile"
	local rc=0
	
	if [ -n "$PTB_OPT_statistics_manager" ]; then
		ptb_init_statistics_manager $PTB_OPT_statistics_manager
	fi

	# build out the proper path to use for callng innobackupex
	local xtrabackup_path="${S_BINDIR[$PTB_OPT_server_id]}/bin:${S_BINDIR[$PTB_OPT_server_id]}/libexec:$PTB_OPT_restore_rootdir"

	#parse the options and build out the correct innobackupex command line
	xtrabackup_common_parse_command_options
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - xtrabackup_common_parse_command_options failed with $rc"
		return $rc
	fi

	local restore_base_command="--defaults-file=${S_DEFAULTSFILE[${PTB_OPT_server_id}]}"
	restore_base_command="$restore_base_command $xb_restore_command_options"

	# loop through full backups
	local full_cycle=0
	while true; do
		local current_cycle=0

		local restore_source_base=${PTB_OPT_vardir}/backup-${full_cycle}.${current_cycle}
		if [ ! -d $restore_source_base ]; then
			break;
		fi

		# loop through making the restore for each cycle 
		while true; do
			local restore_source_current=${PTB_OPT_vardir}/backup-${full_cycle}.${current_cycle}
			if [ ! -d $restore_source_current ]; then
				break;
			fi
			local restore_working_base=${PTB_OPT_vardir}/restore-base
			local restore_working_logs=${PTB_OPT_vardir}/restore-logs
			local cycle_logfile=${PTB_OPT_vardir}/restore-${full_cycle}.${current_cycle}.log
			local restore_start_time=0
			local restore_command=
	
			# mark our start in the logfile
			ptb_report_info "$rpt_prefix - starting: full backup cycle=${full_cycle} incremental backup cycle=${current_cycle} "
			ptb_report_info "$rpt_prefix - options: ${PTB_OPT_backup_command_option[@]}"
			ptb_report_info "$rpt_prefix - path: ${xtrabackup_path}"
			ptb_report_info "$rpt_prefix - base command: innobackupex $restore_base_command"
			
			# first, copy the base backup into the working dir so that
			# the original remains unmolested by the restore process
			if [ -d $restore_working_base ]; then
				ptb_run_cmd rm -rf $restore_working_base
			fi
			ptb_runcmd cp -r $restore_source_base $restore_working_base
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - cp -r $restore_source_base $restore_working_base, failed with $rc"
				break
			fi

			# if it was an encrypted backup, decrypt in place
			if [ -n "$xb_opt_encrypt" ]; then
				ptb_report_info "$rpt_prefix - encryption used, decrypting backup"
				xtrabackup_common_decrypt_in_place "${restore_working_base}" "${xtrabackup_path}" "$PTB_OPT_restore_logfile"
				if [ $rc -ne 0 ]; then
					break;
				fi
			fi 

			# if it was a compressed backup, decompress in place
			if [ -n "$xb_opt_encrypt" ]; then
				ptb_report_info  "$rpt_prefix - compression used, decompressing backup"
				xtrabackup_common_decompress_in_place "${restore_working_base}" "${xtrabackup_path}" "$PTB_OPT_restore_logfile"
				if [ $rc -ne 0 ]; then
					break;
				fi
			fi 

			# if we are preparing the base only...
			if [ $current_cycle -eq 0 ]; then
				restore_command="$restore_base_command --prepare --target-dir=${restore_working_base}"

				# make sure to clean any logs on first cycle
				if [ -d $restore_working_logs ]; then
					ptb_runcmd rm -rf $restore_working_logs
				fi
				ptb_runcmd mkdir $restore_working_logs
			# else, we are preparing an incremental, apply the archived logs
			else
				restore_command="$restore_base_command --prepare --target-dir=${restore_working_base} --innodb-log-arch-dir=${restore_working_logs}"

				# copy over the next batch of logs
				local logfile=
				for logfile in `find ${restore_source_current} -iname 'ib_log_archive_*' | sort`; do
					ptb_runcmd cp $logfile $restore_working_logs
				done

				if [ -n "$xb_opt_to_archived_lsn" ] && [ $xb_opt_to_archived_lsn -ne 0 ]; then
					# grab the lsn and add it to the command
					local lsn=`cat ${restore_source_current}/lsn`
					restore_command="$restore_command --to-archived-lsn=$lsn"
				fi
			fi

			restore_start_time=$SECONDS
			ptb_report_info "$rpt_prefix - ( PATH=${xtrabackup_path}:$PATH; xtrabackup $restore_command )"
			( PATH=${xtrabackup_path}:$PATH; xtrabackup $restore_command &> $cycle_logfile ) 
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - xtrabackup($restore_command) failed with $rc, see ${cycle_logfile}"
				break
			fi
			ptb_report_file_info $cycle_logfile

			local restore_total_time=`expr $SECONDS - $restore_start_time`
			local validate_start_time=$SECONDS
			ptb_report_info "$rpt_prefix - prepare completed: time ${restore_total_time} full backup cycle=${full_cycle} incremental backup cycle=${current_cycle} rc=$rc"

			# empty the server data dir
			ptb_delete_server_data $PTB_OPT_server_id
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - ptb_delete_server_data($PTB_OPT_server_id) failed with $rc"
				break
			fi

			# copy back
			restore_command="$restore_base_command --copy-back"
			ptb_report_info "$rpt_prefix - ( PATH=${xtrabackup_path}:$PATH; innobackupex $restore_command $restore_working_base )"
			( PATH=${xtrabackup_path}:$PATH; innobackupex $restore_command $restore_working_base &> $cycle_logfile ) 
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - innobackupex($restore_command) failed with $rc"
				break
			fi
			ptb_report_file_info $cycle_logfile

			# start the server
			ptb_start_server $PTB_OPT_server_id
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - ptb_start_server($PTB_OPT_server_id) failed with $rc"
				break
			fi

			# some validation
			local mark_data="${full_cycle}.${current_cycle}"
			xtrabackup_common_test_mark_data $PTB_OPT_server_id "$mark_data"
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - xtrabackup_common_test_mark_data(${PTB_OPT_server_id}, \"${mark_data}\") failed with $rc" 
				ptb_stop_server $PTB_OPT_server_id
				break
			fi

			ptb_check_all_databases $PTB_OPT_server_id
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - ptb_check_all_databases($PTB_OPT_server_id) failed with $rc" 
				ptb_stop_server $PTB_OPT_server_id
				break
			fi

			# stop the server
			ptb_stop_server $PTB_OPT_server_id
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - ptb_stop_server($PTB_OPT_server_id) failed with $rc"
				ptb_stop_server $PTB_OPT_server_id
				break
 			fi

			local validate_total_time=`expr $SECONDS - $validate_start_time`
			ptb_report_info "$rpt_prefix - validation completed: time ${validate_total_time} full backup cycle=${full_cycle} incremental backup cycle=${current_cycle} rc=$rc"

			ptb_report_file_info ${S_LOGFILE[${PTB_OPT_server_id}]}

			ptb_stat_register_row $restore_source_dir
			local row_id=$PTB_STAT_RESULT
			if [ $row_id -gt 0 ]; then
				ptb_stat_set_cell_value $row_id "restore-time" "${restore_total_time}" "validate-time" "${validate_total_time}"
			fi

			# clean up
			ptb_runcmd rm -rf $restore_working_base
			ptb_runcmd rm -f $cycle_logfile

			ptb_report_info "$rpt_prefix - done: full backup cycle=${full_cycle} incremental backup cycle=${current_cycle} "
			current_cycle=`expr ${current_cycle} + 1`
		done

		if [ $rc -ne 0 ]; then
			break
		fi
		
		ptb_runcmd rm -rf $restore_working_logs

		ptb_report_info "$rpt_prefix - done: full backup cycle=${full_cycle}"
		full_cycle=`expr ${full_cycle} + 1`
	done

	ptb_report_info "$rpt_prefix - done, rc=$rc"
	return $rc
}
################################################################################
# Shows usage
function usage()
{
	echo "XtraBackup - restore validation"
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
	"server-id REQ 1 INT 0 9999 1 Server ID to restore to." \
	"vardir REQ 1 PATHEXISTS Directory where individual test and data results should be located." \
	"statistics-manager OPT 1 STR Name of pipe to communicate with statistics manager." \
	"pidfile OPT 1 STR Name of pidfile." \
	"verbosity OPT 1 INT 0 4 2 Output verbosity filter: ERROR=4; WARNING=3; INFO=2; DEBUG=1; IDEBUG=0." \
	"restore-rootdir OPT 1 STR Directory where the XtraBackup binaries are located." \
	"restore-logfile OPT 1 STR Log file name for backup operation." \
	"restore-option OPT 0 STR Extra restore options." \
	"backup-command-option OPT 0 STR Extra backup command options." \
)

# parse general option descriptors and set up option array
ptb_parse_option_descriptors "PTB_OPTION_DESCRIPTORS" "PTB"

# parse command line args
ptb_parse_options getoption usage "PTB" $@

# validate command line args
ptb_validate_options usage "PTB"

# debugging
ptb_show_option_values "PTB"
#exit 0
if [ -n "$PTB_OPT_pidfile" ]; then
	echo $$ > $PTB_OPT_pidfile
fi

restore
rc=$?

if [ -n "$PTB_OPT_pidfile" ]; then
	rm -f $PTB_OPT_pidfile
fi

exit $rc
