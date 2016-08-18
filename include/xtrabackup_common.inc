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


###############################################################################
# PUBLIC - Creates schema and table if it doesn't exist and adds specified
#          data marker into table.
#
# $1 - server id to add data to
# $2 - string to add to mark data table
function xtrabackup_common_add_mark_data()
{
	local server_id=$1
	local mark_data=$2
	local rc=0
	local prefix="xtrabackup_common_add_mark_data($1, $2)"

	ptb_report_idebug_enter "$prefix"

	local sql="CREATE DATABASE IF NOT EXISTS PERCONA_SCHEMA"
	ptb_sql $server_id "$sql"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$prefix - ptb_sql($server_id, \"${sql}\") failed with $rc"
		ptb_report_idebug_leave "$prefix = $rc"
		retrurn $rc
	fi
	
	sql="CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.backup_test_mark ( c1 VARCHAR(255) NOT NULL ) ENGINE=INNODB"
	ptb_sql $server_id "$sql"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$prefix - ptb_sql($server_id, \"${sql}\") failed with $rc"
		ptb_report_idebug_leave "$prefix = $rc"
		return $rc
	fi

	sql="INSERT INTO PERCONA_SCHEMA.backup_test_mark( c1 ) VALUES( '$mark_data' )"
	ptb_sql $server_id "$sql"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$prefix - ptb_sql($server_id, \"${sql}\") failed with $rc"
		ptb_report_idebug_leave "$prefix = $rc"
		return $rc
	fi
	ptb_report_idebug_leave "$prefix = $rc"
	return $rc
}
###############################################################################
# PUBLIC - Validates data marker
#
# $1 - server id to test
# $2 - string to validate
function xtrabackup_common_test_mark_data()
{
	local server_id=$1
	local mark_data=$2
	local rc=0
	local prefix="xtrabackup_common_test_mark_data($1, $2)"
	
	ptb_report_idebug_enter "$prefix"

	local query="SELECT c1 FROM PERCONA_SCHEMA.backup_test_mark WHERE c1='$mark_data'" 
	ptb_sql_query $server_id xbctmd_results "$query"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$prefix - \"$query\" failed with $rc."
	elif [ -z "${PTB_xbctmd_results_rowcount}" ] || [ ${PTB_xbctmd_results_rowcount} -eq 0 ];  then
		ptb_report_error "$prefix - \"$query\" produced no results [${PTB_xbctmd_results_rowcount}]."
	fi
	ptb_rowset_unset xbctmd_results

	ptb_report_idebug_leave "$prefix = $rc"
	return $rc
}

###############################################################################
# PUBLIC - Parses out PTB_OPT_backup_command_option into individual variables
#	   and creates backup and restore command option sets to be used
#	   on the command lines when invoking innobackupex
#
xb_opt_encrypt=
xb_opt_encrypt_threads=
xb_opt_encrypt_chunk_size=
xb_opt_encrypt_key=
xb_opt_compress=
xb_opt_compress_threads=
xb_opt_compress_chunk_size=
xb_opt_stream=
xb_opt_parallel=
xb_opt_throttle=
xb_opt_valgrind=
xb_opt_to_archived_lsn=
xb_backup_command_options=
xb_restore_command_options=
function xtrabackup_common_parse_command_options()
{
	local rpt_prefix="xtrabackup_common_parse_command_options()"

	#parse the options and build out the correct innobackupex command line
	local backup_option=
	for backup_option in ${PTB_OPT_backup_command_option[@]}; do
		if [ "${backup_option:0:2}" = "--" ]; then
			backup_option="${backup_option:2}"
		fi

		local backup_optname="`expr match \"${backup_option}\" '\([a-zA-Z0-9_\-]*\=\)'`"
		local backup_optvalue="${backup_option:${#backup_optname}}"
		if [ -z "$backup_optname" ]; then backup_optname="$backup_optvalue"; backup_optvalue=; fi
		backup_optname="${backup_optname%"="}"
	
		case "$backup_optname" in
		"encrypt" )
			xb_opt_encrypt=$backup_optvalue
			case $xb_opt_encrypt in
			"NONE" ) xb_opt_encrypt=;;
			"AES128" ) xb_opt_encrypt_key="1234567812345678";;
			"AES192" ) xb_opt_encrypt_key="123456781234567812345678";;
			"AES256" ) xb_opt_encrypt_key="12345678123456781234567812345678";;
			* )
				ptb_report_error "$rpt_prefix - invalid encryption algo. [$xb_opt_encrypt]"
				return 1
				;;
			esac
			;;
		"encrypt-threads" | "encrypt_threads" )
			xb_opt_encrypt_threads=$backup_optvalue
			;;
		"encrypt-chunk-size" | "encrypt_chunk_size" )
			xb_opt_encrypt_chunk_size=$backup_optvalue
			;;
		"compress" ) 
			if [ -z "$backup_optvalue" ]; then
				xb_opt_compress=1
			else
				xb_opt_compress=$backup_optvalue
			fi
			;;
		"compress-threads" | "compress_threads" )
			xb_opt_compress_threads=$backup_optvalue
			;;
		"compress-chunk-size" | "compress_chunk_size" )
			xb_opt_compress_chunk_size=$backup_optvalue
			;;
		"use-valgrind" | "use_valgrind" ) 
			if [ -z "$backup_optvalue" ]; then
				xb_opt_valgrind=1
			else
				xb_opt_valgrind=$backup_optvalue
			fi
			;;
		"parallel" )
			xb_opt_parallel=$backup_optvalue
			;;
		"stream" )
			if [ "$backup_optvalue" != "NONE" ]; then
				xb_opt_stream=$backup_optvalue
			fi
			;;
		"throttle" )
			xb_opt_throttle=$backup_optvalue
			;;
		"to-archived-lsn" | "to_archived_lsn" )
			xb_opt_to_archived_lsn=$backup_optvalue
			;;
		* )
			xb_backup_command_options="$xb_backup_command_options --${backup_option}"
			xb_restore_command_options="$xb_restore_command_options --${backup_option}"
			;;
		esac
	done

	if [ -n "$xb_opt_encrypt" ]; then
		xb_backup_command_options="$xb_backup_command_options --encrypt=$xb_opt_encrypt"
		xb_backup_command_options="$xb_backup_command_options --encrypt-key=$xb_opt_encrypt_key"
		if [ -n "$xb_opt_encrypt_threads" ]; then
			xb_backup_command_options="$xb_backup_command_options --encrypt-threads=$xb_opt_encrypt_threads"
		fi
		if [ -n "$xb_opt_encrypt_chunk_size" ]; then
			xb_backup_command_options="$xb_backup_command_options --encrypt-chunk-size=$xb_opt_encrypt_chunk_size"
		fi
	fi

	if [ -n "$xb_opt_compress" ] && [ "$xb_opt_compress" != "0" ]; then
		xb_backup_command_options="$xb_backup_command_options --compress"
		if [ -n "$xb_opt_compress_threads" ]; then
			xb_backup_command_options="$xb_backup_command_options --compress-threads=$xb_opt_compress_threads"
		fi
		if [ -n "$xb_opt_compress_chunk_size" ]; then
			xb_backup_command_options="$xb_backup_command_options --compress-chunk-size=$xb_opt_compress_chunk_size"
		fi
	fi

	if [ -n "$xb_opt_parallel" ] && [ "$xb_opt_parallel" != "0" ]; then
		xb_backup_command_options="$xb_backup_command_options --parallel=$xb_opt_parallel"
	fi

	if [ -n "$xb_opt_stream" ]; then
		xb_backup_command_options="$xb_backup_command_options --stream=$xb_opt_stream"
	fi

	if [ -n "$xb_opt_throttle" ] && [ "$xb_opt_throttle" != "0" ]; then
		xb_backup_command_options="$xb_backup_command_options --throttle=$xb_opt_throttle"
	fi

	if [ -n "$xb_opt_valgrind" ] && [ "$xb_opt_valgrind" != "0" ]; then
		xb_backup_command_options="$xb_backup_command_options --use-valgrind"
		xb_restore_command_options="$xb_restore_command_options --use-valgrind"
	fi

	if [ -n "$xb_opt_innodb_log_file_size" ]; then
		xb_backup_command_options="$xb_backup_command_options --innodb-log-file-size=$xb_opt_innodb_log_file_size"
	fi
	return 0
}

###############################################################################
# PUBLIC - decrypts an encrypted set of backup files in place
#
# $1 - backup directory to be decrypted
# $2 - path to xtrabackup binaries
# $3 - logfile
function xtrabackup_common_decrypt_in_place()
{
	local working_dir=$1
	local xtrabackup_path=$2
	local logfile="$3"

	local rc=0
	local backup_file=
	for backup_file in `find ${working_dir} -iname "*.xbcrypt"`; do
		local restore_file=$(dirname $backup_file)/$(basename $backup_file .xbcrypt)
		local decrypt_cmd="xbcrypt -d --encrypt-key=${xb_opt_encrypt_key} --encrypt-algo=${xb_opt_encrypt} -i ${backup_file} -o ${restore_file}"
		ptb_report_info "$rpt_prefix - ( PATH=${xtrabackup_path}:$PATH; ${decrypt_cmd} )"
		if [ -z $logfile ]; then
			( PATH=${xtrabackup_path}:$PATH; ${decrypt_cmd} ) 
		else
			( PATH=${xtrabackup_path}:$PATH; ${decrypt_cmd} &>> $logfile ) 
		fi
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ${decrypt_cmd} failed with $rc"
			break
		fi
		ptb_runcmd rm -f $backup_file
	done
	return $rc
}

###############################################################################
# PUBLIC - decompresses a compressed set of backup files in place
#
# $1 - backup directory to be decompressed
# $2 - path to xtrabackup binaries
# $3 - logfile
function xtrabackup_common_decompress_in_place()
{
	local working_dir=$1
	local xtrabackup_path=$2
	local logfile="$3"

	local backup_file=
	for backup_file in `find ${working_dir} -iname "*.qp"`; do
		local restore_file=$(dirname $backup_file)
		local decompress_cmd="qpress -d ${backup_file} ${restore_file}"
		ptb_report_info "$rpt_prefix - ( PATH=${xtrabackup_path}:$PATH; ${decompress_cmd} )"
		if [ -z $logfile ]; then
			( PATH=${xtrabackup_path}:$PATH; ${decompress_cmd} ) 
		else
			( PATH=${xtrabackup_path}:$PATH; ${decompress_cmd} &>> $logfile ) 
		fi
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ${decompress_cmd} failed with $rc"
			break
		fi
		ptb_runcmd rm -f $backup_file
	done
	return $rc
}
