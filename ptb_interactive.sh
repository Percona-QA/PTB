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

########################################
# Percona Test Bench Interactive shell #
########################################

#
# The location of the saved history file
#
PTB_HISTORYFILE=

#
# The current history buffer
#
PTB_HISTORY[0]=


################################################################################
# Loads the saved history file for the current sandbox if there is one.
function ptb_load_history()
{
	PTB_HISTORYFILE="$PTB_DATADIR/.ptb.history"
	local line=
	if [ -e "$PTB_HISTORYFILE" ]; then
		while read line; do
			PTB_HISTORY[${#PTB_HISTORY[@]}]=$line
			history -s $line
		done < "$PTB_HISTORYFILE"
	fi
}
################################################################################
# Shows the current history
function ptb_show_history()
{
	local count=${#PTB_HISTORY[@]};
	for (( line=1; line < count; line++ )); do
		echo "$line: ${PTB_HISTORY[$line]}"
	done
}
################################################################################
# Saves the current history to the history file for the current sandbox.
function ptb_save_history()
{
	if [ -e "$PTB_HISTORYFILE" ]; then
		rm -f "$PTB_HISTORYFILE"
	fi
	local count="${#PTB_HISTORY[@]}"
	local line=
	for (( line=1; line < count; line++ )); do
		echo "${PTB_HISTORY[$line]}" >> "$PTB_HISTORYFILE"
	done
}
################################################################################
# Adds a command to the current history.
#
# $1 - required, command to add to history
function ptb_add_history()
{
	local count="${#PTB_HISTORY[@]}"
	local command="$@"
	if [ "$count" -le "1" ]; then
		PTB_HISTORY[0]=""
		PTB_HISTORY[1]="$command"
		history -s "$command"
	else
		if [ "${PTB_HISTORY[`expr $count - 1`]}" != "$command" ]; then
			PTB_HISTORY[$count]="$command"
			history -s "$command"
		fi
	fi
}
################################################################################
# Obtains the command stored at a specific history ordinal
#
# $1 - required, position to return
# $2 - required, variable to store it in
function ptb_history_at()
{
	eval "$2="
	local count="${#PTB_HISTORY[@]}"
	local max=`expr "$count" - 1`
	if [ "$max" -eq "0" ]; then
		ptb_report_error "There is no history."
	fi
	if [ -z "$1" ]; then
		ptb_report_error "You must specify a valid position from 1-$max..."
	fi

	if ! ptb_is_integer "$1"; then
		ptb_report_error "You must specify a valid position from 1-$max..."
	else
		if [[ "$1" -lt "1" || "$1" -gt "$max" ]]; then
			ptb_report_error "You must specify a valid position from 1-$max..."
		else
			eval "$2=\"${PTB_HISTORY[$1]}\""
		fi
	fi
}
################################################################################
# Clears the current history
function ptb_clear_history()
{
	unset PTB_HISTORY
	history -c
}
################################################################################
# Shows task status
#
# $1 - optional, instance id of task to display status, if NULL, tasks will be
#      enumerated and status shown for each
function ptb_show_task_status()
{
	local instanceid=$1
	local count=${#S_TASKPID[@]}

	if [ -z "$instanceid" ]; then
		echo "Showing task status for `expr ${count} -1` task instances:"
		local instance=
		for (( instance=1; instance < count; instance++ )); do
			ptb_show_task_status $instance
		done
	elif ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_show_task_status($1) - Invalid task id specified."
	else
		local res="Task[$instanceid] :"
		if [ -n "${T_PID[$instanceid]}" ]; then
			res="$res pid[${T_PID[$instanceid]}] command[${T_COMMAND[$instanceid]}]"
			if [ -n "`ps -p ${T_PID[$instanceid]} -o cmd --no-headers`" ]; then
				res="$res appears to be running."
			else
				res="$res appears to have stopped."
			fi
		else
			res="$res is empty."
		fi
		echo -e $res
	fi
	return $PTB_RET_SUCCESS

}
################################################################################
# Shows server status
#
# $1 - optional, instance id of server to display status, if NULL, servers will be
#      enumerated and status shown for each
function ptb_show_server_status()
{
	local instanceid=$1
	local count=${#S_BASEDIR[@]}

	if [ -z "$instanceid" ]; then
		echo "Showing server status for `expr ${count} - 1` server instances:"
		local instance=
		for (( instance=1; instance < count; instance++ )); do
			ptb_show_server_status $instance
		done
	elif ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_show_server_status($1) - Invalid server id specified."
	else
		local res="Server[${instanceid}] :"
		if [ -n "${S_BINDIR[$instanceid]}" ]; then
			res="${res} bindir[${S_BINDIR[$instanceid]}] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]"
			
			if [ -f "${S_PIDFILE[$instanceid]}" ]; then
				local pid=`cat ${S_PIDFILE[$instanceid]}`
				res="${res} pid[${pid}]"
				if [ -n "`ps -p ${pid} -o cmd --no-headers`" ]; then
					res="${res} appears to be running."
				else
					res="${res} appears to have crashed."
				fi
			else
				res="${res} is not running."
			fi

			local file="${S_OPTIONSFILE[$instanceid]}"
			local line=
			if [ -e "$file" ]; then
				res="${res}\n\tServer options:"
				while read line; do
					res="${res}\n\t\t$line"
				done < "$file"
			else
				res="${res}\n\tNo server options specified."
			fi
		else
			res="${res} is empty."
		fi
		echo -e $res
	fi
	return $PTB_RET_SUCCESS

}
################################################################################
# Shows usage
function ptb_show_usage()
{
	echo "Usage:"
	echo "  interactive (-bsv)"
	echo "Where: "
	echo "  -s (required) Path to sandbox root."
	echo "  -v (optional) Output verbosity filter: ERROR=4; WARNING=3; INFO=2; DEBUG=1; IDEBUG=0. Default=3 (INFO)."
	exit 1
}
################################################################################
# Executes command specified by user
function ptb_execute_command()
{
	local command=$1
	shift
	case "$command" in
	"quit" | "q"	) return 1;;
	*		) ptb_${command} $@;;
	esac
	ptb_add_history "${command} $@"
	return 0
}
################################################################################
# Interactive shell main
I_SANDBOXDIR=
I_VERBOSITY=$PTB_RPT_INFO

if [ $# -eq 0 ]; then
	ptb_show_usage
	exit 1
fi
while getopts ":s:v:" opt; do
	case $opt in
	s ) I_SANDBOXDIR=${OPTARG};;
	v ) I_VERBOSITY=${OPTARG};;
	* ) echo "ERROR: Unknown option."; usage;;
	esac
done

ptb_init $I_SANDBOXDIR $I_VERBOSITY "" ptb_save_history
if [ $? -ne 0 ]; then
	ptb_report_error "Unable to initialize ptb core with $?"
	exit 1
fi

ptb_load_history
I_COMMAND=
while read -e -p "> " I_COMMAND; do
	ptb_execute_command $I_COMMAND
	if [ $? -ne 0 ]; then
		break;
	fi
done

ptb_cleanup 1
