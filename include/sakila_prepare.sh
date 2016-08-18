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
# Runs sakila prepare
function prepare()
{
	local rpt_prefix="prepare()"

	ptb_init $PTB_OPT_vardir $PTB_OPT_verbosity "$PTB_OPT_prepare_logfile"

	local rc=0

	ptb_sql $PTB_OPT_server_id ./include/sakila-schema.sql
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
		ptb_cleanup 0
		return $rc
	fi

	ptb_sql $PTB_OPT_server_id ./include/sakila-data.sql
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - ptb_sql failed with $rc."
		ptb_cleanup 0
		return $rc
	fi

	ptb_cleanup 0
	return $rc
}
################################################################################
# Generates test descriptor
function gen_descriptor()
{
	local desc="sakila"
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
	echo "sakila - prepare"
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
	"prepare-rootdir OPT 1 STR Directory where other binaries are located." \
	"prepare-logfile OPT 1 STR Log file name for prepare operation." \
	"prepare-option OPT 0 STR Extra options to pass to ???." \
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
