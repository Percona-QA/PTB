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
# Percona Test Bench - xtrabackup_cleanup               #
#########################################################

################################################################################
# Runs xtrabackup cleanup
function cleanup()
{
	local rpt_prefix="cleanup()"

	ptb_init $PTB_OPT_vardir $PTB_OPT_verbosity "$PTB_OPT_cleanup_logfile"

	local rc=0

	local dir=
	for dir in `find $PTB_OPT_vardir -maxdepth 1 -type d -iname "backup-*"`; do
		ptb_runcmd rm -rf "${dir}"
	done
	for dir in `find $PTB_OPT_vardir -maxdepth 1 -type d -iname "restore-*"`; do
		ptb_runcmd rm -rf "${dir}"
	done

	ptb_delete_server_data $PTB_OPT_server_id

	if [ -f "${S_QUERYLOGFILE[$PTB_OPT_server_id]}" ]; then
		ptb_runcmd rm -f ${S_QUERYLOGFILE[$PTB_OPT_server_id]}
	fi

	ptb_cleanup 0
	return $rc
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
	"cleanup-rootdir OPT 1 STR Directory where other binaries are located." \
	"cleanup-logfile OPT 1 STR Log file name for cleanup operation." \
	"cleanup-option OPT 0 STR Extra options to pass to ???." \
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

cleanup

rc=$?

if [ -n "$PTB_OPT_pidfile" ]; then
	rm -f $PTB_OPT_pidfile
fi

exit $rc
