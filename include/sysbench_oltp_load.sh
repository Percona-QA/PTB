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
# Percona Test Bench - sysbench_oltp_load               #
#########################################################

################################################################################
# Runs sysbench load
function load()
{
	local rpt_prefix="load()"

	ptb_init $PTB_OPT_vardir $PTB_OPT_verbosity "$PTB_OPT_load_logfile"

	local rc=1
	sysbench_test="${PTB_OPT_load_rootdir}/tests/db/oltp.lua" 
	if [ ! -r "$sysbench_test" ]; then
		ptb_report_error "sysbench_oltp_load.sh - can not access sysbench test $sysbench_test"
		rc=$PTB_RET_INVALID_ARGUMENT
		ptb_cleanup 0
		return $rc
	fi
	sysbench_cmd="sysbench --test=$sysbench_test --mysql-socket=${S_SOCKET[$PTB_OPT_server_id]} --mysql-user=root"
	local i
	for i in ${PTB_OPT_load_option[@]}; do
		sysbench_cmd="${sysbench_cmd} $i"
	done
	sysbench_cmd="${sysbench_cmd} run"

	if [ -n "$PTB_OPT_load_logfile" ]; then
		echo "Running: $sysbench_cmd" >> $PTB_OPT_load_logfile
		ptb_runcmdpathlog "$PTB_OPT_load_rootdir" "$PTB_OPT_load_logfile" $sysbench_cmd
	else
		echo "Running: $sysbench_cmd"
		ptb_runcmdpath "$PTB_OPT_load_rootdir" $sysbench_cmd
	fi

	ptb_cleanup 0

	return $rc
}
################################################################################
# Shows usage prolog
function usage()
{
	echo "sysbench - load"
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
	"load-rootdir OPT 1 STR Directory where sysbench binaries and tests are located." \
	"load-logfile OPT 1 STR Log file name for load operation." \
	"load-option OPT 0 STR Extra options to pass to sysbench." \
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

load
rc=$?

if [ -n "$PTB_OPT_pidfile" ]; then
	rm -f $PTB_OPT_pidfile
fi

exit $rc
