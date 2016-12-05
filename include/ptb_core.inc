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

#
# This is the root folder for the script 'sandbox', each server instance
# sandbox will be created beneath this directory. This will be initialized
# during ptb_init, which MUST be called BEFORE any other PTB functions.
#
PTB_DATADIR=

#
# Various verbosity levels of output reporting. Pass one into ptb_init to set
# the filtering level for imtr_report and pass one as the first parameter to
# any calls to ptb_report to note the importance of the reported content.
#
PTB_RPT_IDEBUG=0  # internal debugging of PTB core
PTB_RPT_DEBUG=1   # debugging of test cases
PTB_RPT_INFO=2    # general test case informational stuff
PTB_RPT_WARNING=3 # test case warning
PTB_RPT_ERROR=4   # test case error

PTB_RPT_ENTER=0
PTB_RPT_LEAVE=1
PTB_RPT_IN=2

#
# The actual filter level for reporting as described above. Any content reported
# that is < $PTB_REPORT_FILTER will be dumped. This is set by passing one of
# the above values into ptb_init.
#
PTB_RPT_FILTER=$PTB_RPT_INFO

#
# Optional logfile to report into
#
PTB_RPT_FILE=

# Return codes
PTB_RET_SUCCESS=0
PTB_RET_INVALID_INSTANCE_ID=1
PTB_RET_INVALID_BINDIR=2
PTB_RET_INVALID_PORT=3
PTB_RET_INVALID_ARGUMENT=4
PTB_RET_SERVER_RUNNING=5
PTB_RET_SERVER_NOT_RUNNING=6
PTB_RET_PARSE_ERROR=7
PTB_RET_FAILED=8
PTB_RET_TASK_RUNNING=9
PTB_RET_TASK_NOT_RUNING=10

#
# The name of a script function to call at the beginning of ptb_deinit. This
# is optional and should be passed to ptb_init.
#
PTB_PRECLEANUP=

#
# The name of a script function to call at the end of ptb_deinit. This is
# optional and should be passed to ptb_init.
#
PTB_POSTCLEANUP=

##########################
# Basic utility routines #
##########################


function ptb_report_low()
{
	if [ -n "$PTB_RPT_FILE" ]; then
		echo -e "`date '+%Y/%m/%d %H:%M:%S.%N'` $$ $@" >> $PTB_RPT_FILE
	else
		echo -e "`date '+%Y/%m/%d %H:%M:%S.%N'` $$ $@"
	fi
}
################################################################################
# PUBLIC - Used to 'report' anything to the standard output and is used to add
# some filtering and debugging.
#
# $1 - required, importance level of content to report as defined by PTB_RPT_*
#      above.
# $2 - required if $1=PTB_RPT_IDEBUG, $PTB_RPT_ENTER=function enter,
#      $PTB_RPT_LEAVE=function leave
# $@ - required, content to report.
function ptb_report()
{
	local filter=$1
	shift
	if [ $filter -lt $PTB_RPT_FILTER ]; then
		return
	else
		case "$filter" in
		"$PTB_RPT_IDEBUG" )
			local pos=
			case $1 in
			0 ) pos="enter ";;
			1 ) pos="leave ";;
			2 ) pos="in    ";;
			esac
			shift
			ptb_report_low "IDEBUG ${pos} : $@"
		;;
		"$PTB_RPT_DEBUG" ) ptb_report_low "DEBUG : $@";;
		"$PTB_RPT_INFO" ) ptb_report_low "INFO : $@";;
		"$PTB_RPT_WARNING" ) ptb_report_low "WARNING : $@";;
		"$PTB_RPT_ERROR" ) ptb_report_low "ERROR : $@";;
		* ) ptb_report_low "UNKNOWN : $@";;
		esac
	fi
}
function ptb_report_idebug()
{
	ptb_report $PTB_RPT_IDEBUG $@
}
function ptb_report_idebug_enter()
{
	ptb_report $PTB_RPT_IDEBUG $PTB_RPT_ENTER $@
}
function ptb_report_idebug_in()
{
	ptb_report $PTB_RPT_IDEBUG $PTB_RPT_IN $@
}
function ptb_report_idebug_leave()
{
	ptb_report $PTB_RPT_IDEBUG $PTB_RPT_LEAVE $@
}
function ptb_report_debug()
{
	ptb_report $PTB_RPT_DEBUG $@
}
function ptb_report_info()
{
	ptb_report $PTB_RPT_INFO $@
}
function ptb_report_warning()
{
	ptb_report $PTB_RPT_WARNING $@
}
function ptb_report_error()
{
	ptb_report $PTB_RPT_ERROR $@
}
################################################################################
# PUBLIC - Used to 'report' the contents of a file to the standard output and is
# used to add some filtering and debugging.
#
# $1 - required, importance level of content to report as defined by PTB_RPT_*
# $2 - required if $1=PTB_RPT_IDEBUG, $PTB_RPT_ENTER=function enter,
#      $PTB_RPT_LEAVE=function leave
#      above.
# $2/3 - required, file to capture and report.
function ptb_report_file()
{
	local filter=$1
	shift
	local catfile=
	if [ $filter -lt $PTB_RPT_FILTER ]; then
		return
	else
		case "$filter" in
		"$PTB_RPT_IDEBUG" )
			local pos=
			case $1 in
			0 ) pos="enter ";;
			1 ) pos="leave ";;
			2 ) pos="in    ";;
			esac
			shift
			ptb_report_low "IDEBUG ${pos} : file at $1"
			catfile=$1
		;;
		"$PTB_RPT_DEBUG" ) ptb_report_low "DEBUG : file at $1"; catfile=$1;;
		"$PTB_RPT_INFO" ) ptb_report_low "INFO : file at $1"; catfile=$1;;
		"$PTB_RPT_WARNING" ) ptb_report_low "WARNING : file at $1"; catfile=$1;;
		"$PTB_RPT_ERROR" ) ptb_report_low "ERROR : file at $1"; catfile=$1;;
		* ) ptb_report_low "UNKNOWN : file at $1"; catfile=$1;;
		esac
		if [ -n "$catfile" ]; then
			if [ -n "$PTB_RPT_FILE" ]; then
				cat $catfile >> $PTB_RPT_FILE
			else
				cat $catfile
			fi
		fi
	fi
}
function ptb_report_file_idebug()
{
	ptb_report_file $PTB_RPT_IDEBUG $@
}
function ptb_report_file_idebug_enter()
{
	ptb_report_file $PTB_RPT_IDEBUG $PTB_RPT_ENTER $@
}
function ptb_report_file_idebug_in()
{
	ptb_report_file $PTB_RPT_IDEBUG $PTB_RPT_IN $@
}
function ptb_report_file_idebug_leave()
{
	ptb_report_file $PTB_RPT_IDEBUG $PTB_RPT_LEAVE $@
}
function ptb_report_file_debug()
{
	ptb_report_file $PTB_RPT_DEBUG $@
}
function ptb_report_file_info()
{
	ptb_report_file $PTB_RPT_INFO $@
}
function ptb_report_file_warning()
{
	ptb_report_file $PTB_RPT_WARNING $@
}
function ptb_report_file_error()
{
	ptb_report_file $PTB_RPT_ERROR $@
}

################################################################################
# PUBLIC - Tests if input is a valid integer.
#
# $1 - required, variable to test.
# $? - 0=yes, !0=no.
function ptb_is_integer()
{
	[ $# -eq 1 ] || return 1

	case $1 in
	*[!0-9]*|"" ) return 1;;
        * ) return 0;;
	esac
}
################################################################################
# PUBLIC - Used to run an externel command.
# $@ - required, command and args.
# $? - result of external command
function ptb_runcmd()
{
	ptb_report_idebug_enter "ptb_runcmd($@)"
	( "$@" )
	local rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "ptb_runcmd($@) failed with code $rc"
	fi
	ptb_report_idebug_leave "ptb_runcmd($@) = $rc"
	return $rc
}
################################################################################
# PUBLIC - Used to run an externel command with a path specification.
# $1 - required, path string to prepend to existing path for subshell.
# #@ - required, command and args.
# $? - result of external command.
function ptb_runcmdpath()
{
	local path=$1
	shift
	ptb_report_idebug_enter "ptb_runcmdpath($path, $@)"
	( PATH=$path:$PATH; "$@" )
	local rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "ptb_runcmdpath($path, $@) failed with code $rc"
	fi
	ptb_report_idebug_leave "ptb_runcmdpath($path, $@) = $rc"
	return $rc
}
################################################################################
# PUBLIC - Used to run an externel command with a path specification and capture
#          output to  log file.
# $1 - required, path string to prepend to existing path for subshell.
# $2 - required, log file to capture stderr and stdout to.
# #@ - required, command and args.
# $? - result of external command.
function ptb_runcmdpathlog()
{
	local path=$1
	local logfile=$2
	shift 2
	ptb_report_idebug_enter "ptb_runcmdpathlog($path, $logfile, $@)"
	( PATH=$path:$PATH; "$@" &>> $logfile )
	local rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "ptb_runcmdpathlog($path, $logfile, $@) failed with code $rc"
	fi
	ptb_report_idebug_leave "ptb_runcmdpathlog($path, $logfile, $@) = $rc"
	return $rc
}


################################################################################
# INTERNAL - used to create SSL certificates and RSA keys.
# 
# $1 - required, server datadir path. Will store RSA keys there.
function ptb_creatersa()
{
	ptb_report_idebug_enter "ptb_creatersa()"	
	
	local rsa_path=$1
	# Generating RSA keys
	ptb_runcmd openssl genrsa -out ${rsa_path}/private_key.pem 2048
	ptb_runcmd openssl rsa -in ${rsa_path}/private_key.pem -pubout -out $rsa_path/public_key.pem
	ptb_runcmd chmod 400 ${rsa_path}/private_key.pem
	ptb_runcmd chmod 444 ${rsa_path}/public_key.pem
  
}
################################################################################
# INTERNAL - used to create SSL certificates.
# 
# $1 - required, path. Will store SSL things there.

function ptb_createssl()
{
	ptb_report_idebug_enter "ptb_createssl()"

	local ssl_path=$1/newcerts
	ptb_runcmd mkdir ${ssl_path}

 	# Create CA certificate
	ptb_runcmd openssl genrsa 2048 > ${ssl_path}/ca-key.pem 
	ptb_runcmd openssl req -new -x509 -nodes -days 3600 -key ${ssl_path}/ca-key.pem -out ${ssl_path}/ca.pem -subj '/CN=www.hero1.RAMESH/O=Database Performance./C=US'
	# Create server certificate, remove passphrase, and sign it
	# server-cert.pem = public key, server-key.pem = private key
	ptb_runcmd openssl req -newkey rsa:2048 -days 3600 -nodes -keyout ${ssl_path}/server-key.pem -out ${ssl_path}/server-req.pem -subj '/CN=www.hero2.RAMESH/O=Database Performance./C=AU'
  	ptb_runcmd openssl rsa -in ${ssl_path}/server-key.pem -out ${ssl_path}/server-key.pem
	ptb_runcmd openssl x509 -req -in ${ssl_path}/server-req.pem -days 3600 -CA ${ssl_path}/ca.pem -CAkey ${ssl_path}/ca-key.pem -set_serial 01 -out ${ssl_path}/server-cert.pem

	# Create client certificate, remove passphrase, and sign it
	# client-cert.pem = public key, client-key.pem = private key
	ptb_runcmd openssl req -newkey rsa:2048 -days 3600 -nodes -keyout ${ssl_path}/client-key.pem -out ${ssl_path}/client-req.pem -subj '/CN=www.hero3.RAMESH/O=Database Performance./C=AU'
	ptb_runcmd  openssl rsa -in ${ssl_path}/client-key.pem -out ${ssl_path}/client-key.pem
	ptb_runcmd openssl x509 -req -in ${ssl_path}/client-req.pem -days 3600 -CA ${ssl_path}/ca.pem -CAkey ${ssl_path}/ca-key.pem -set_serial 01 -out ${ssl_path}/client-cert.pem	
}
####################################
# MySQl server instance management #
####################################
S_BINDIR[0]=
S_BASEDIR[0]=
S_DATADIR[0]=
S_TMPDIR[0]=
S_PORT[0]=
S_SOCKET[0]=
S_PIDFILE[0]=
S_LOGFILE[0]=
S_QUERYLOGFILE[0]=
S_DEFAULTSFILE[0]=
S_IDFILE[0]=
S_OPTIONSFILE[0]=
# innodb_page_size for 5.7 testing, You can specify page size using the values 64k, 32k, 16k (the default), 8k, or 4k
S_INNODB_PAGE_SIZE[0]=
# Innodb tablespace encryption plugin
S_EARLY_PLUGIN_LOAD[0]=
S_KEYRING_FILE_DATA[0]=
# Starting server with SSL
S_SSL_CA[0]=
S_SSL_CERT[0]=
S_SSL_KEY[0]=
# Client SSL
S_SSL_CA_CLIENT[0]=
S_SSL_CERT_CLIENT[0]=
S_SSL_KEY_CLIENT[0]=
# Enabling GTID
S_MASTER_INFO_REPOSITORY[0]=
S_RELAY_LOG_INFO_REPOSITORY[0]=
S_GTID_MODE[0]=
#S_LOG_SLAVE_UPDATES[0]=
#S_ENFORCE_GTID_CONSISTENCY[0]=



################################################################################
# INTERNAL - used to create a mysql defaults file for a server instance
# from options specified through ptb_[add|set|delete]_option.
#
# $1 - required, server instance id.
function ptb_create_defaults_file()
{
	ptb_report_idebug_enter "ptb_create_defaults_file($1)"
	local instanceid=$1
	local file=${S_DEFAULTSFILE[$instanceid]}
	
	echo "[client]" > $file
	echo "port=${S_PORT[$instanceid]}" >> $file
	echo "socket=${S_SOCKET[$instanceid]}" >> $file
	
	#changed for Jenkins job sysbench fail
	echo "user=root" >> $file
	echo "ssl-ca=${S_SSL_CA_CLIENT[$instanceid]}" >> $file
	echo "ssl-cert=${S_SSL_CERT_CLIENT[$instanceid]}" >> $file
	echo "ssl-key=${S_SSL_KEY_CLIENT[$instanceid]}" >> $file
	
	echo "[mysqld]" >> $file
	echo "basedir=${S_BINDIR[$instanceid]}" >> $file
	echo "datadir=${S_DATADIR[$instanceid]}" >> $file
	echo "tmpdir=${S_TMPDIR[$instanceid]}" >> $file
	echo "port=${S_PORT[$instanceid]}" >> $file
	echo "socket=${S_SOCKET[$instanceid]}" >> $file
	echo "pid-file=${S_PIDFILE[$instanceid]}" >> $file
	if [ $(${S_BINDIR[$instanceid]}/bin/mysqld --version | grep -oe '5\.[567]' | head -n1) == "5.7" ]; then
	# Fix for https://github.com/Percona-QA/PTB/issues/27
	echo "innodb_page_size=${S_INNODB_PAGE_SIZE[$instanceid]}" >> $file
	# Fix for https://github.com/Percona-QA/PTB/issues/26
	echo "early_plugin_load=${S_EARLY_PLUGIN_LOAD[$instanceid]}" >> $file
	echo "keyring_file_data=${S_KEYRING_FILE_DATA[$instanceid]}" >> $file
	fi
	echo "console" >> $file
	
	#changed for Jenkins job
	echo "user=jenkins" >> $file 
	# ssl goodies
	echo "ssl-ca=${S_SSL_CA[$instanceid]}" >> $file
	echo "ssl-cert=${S_SSL_CERT[$instanceid]}" >> $file
	echo "ssl-key=${S_SSL_KEY[$instanceid]}" >> $file
	
	echo "server-id=$instanceid" >> $file
	echo "general-log-file=${S_QUERYLOGFILE[$instanceid]}" >> $file
	if [ $(${S_BINDIR[$PTB_OPT_server_id]}/bin/mysqld --version | grep -oe '5\.[567]' | head -n1) == "5.6" ] || [ $(${S_BINDIR[$PTB_OPT_server_id]}/bin/mysqld --version | grep -oe '5\.[567]' | head -n1) == "5.7" ]; then
	echo "master-info-repository=${S_MASTER_INFO_REPOSITORY[$instanceid]}" >> $file
	echo "relay-log-info-repository=${S_RELAY_LOG_INFO_REPOSITORY[$instanceid]}" >> $file
	echo "gtid_mode=${S_GTID_MODE[$instanceid]}" >> $file
	echo "log-slave-updates" >> $file
	echo "enforce-gtid-consistency" >> $file

	fi

	if [ -e "${S_OPTIONSFILE[$instanceid]}" ]; then
		local line=
		while read line; do
			echo "$line" >> $file
		done < ${S_OPTIONSFILE[$instanceid]}
	fi
	ptb_report_idebug_leave "ptb_create_defaults_file($1) = 0"
	return $PTB_RET_SUCCESS
}
################################################################################
# INTERNAL - initializes a specific server instance.
#
# $1 - required, server instance id initialize.
# $2 - required, BINDIR that is the root of the server install where server
#      binaries can be found in the $2/bin directory.
# $3 - required, PORT to run server on.
function ptb_init_server_instance()
{
	ptb_report_idebug_enter "ptb_init_server_instance($1, $2, $3)"

	local instanceid=$1
	local bindir=$2
	local port=$3
	local rc=$PTB_RET_SUCCESS
	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_init_server_instance($1, $2, $3) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "$bindir" ]; then
		ptb_report_error "ptb_init_server_instance($1, $2, $3) - Invalid server binary root directory specified."
		rc=$PTB_RET_INVALID_BINDIR
	elif [ ! -d "$bindir" ]; then
		ptb_report_error "ptb_init_server_instance($1, $2, $3) - Invalid server binary root directory specified, directory not found."
		rc=$PTB_RET_INVALID_BINDIR
	else
		ptb_report_info "ptb_init_server_instance($1, $2, $3) - Initializing server instance."
		S_BINDIR[$instanceid]="$bindir"
		S_BASEDIR[$instanceid]="${PTB_DATADIR}/mysql.${instanceid}"
		S_DATADIR[$instanceid]="${S_BASEDIR[$instanceid]}/data"
		S_TMPDIR[$instanceid]="${S_BASEDIR[$instanceid]}/tmp"
		S_PORT[$instanceid]="$port"
		# S_SOCKET[$instanceid]="${S_BASEDIR[$instanceid]}/mysql.sock"
		# Fix for https://github.com/Percona-QA/PTB/issues/7
		S_SOCKET[$instanceid]="/tmp/mysql.sock"
		S_PIDFILE[$instanceid]="${S_BASEDIR[$instanceid]}/mysql.pid"
		S_LOGFILE[$instanceid]="${S_BASEDIR[$instanceid]}/mysql.log"
		S_QUERYLOGFILE[$instanceid]="${S_BASEDIR[$instanceid]}/query.log"
		S_DEFAULTSFILE[$instanceid]="${S_BASEDIR[$instanceid]}/my.cnf"
		S_OPTIONSFILE[$instanceid]="${S_BASEDIR[$instanceid]}/.ptb.opts"
		S_IDFILE[$instanceid]="${S_BASEDIR[$instanceid]}/.ptb.id"
		if [ $(${S_BINDIR[$instanceid]}/bin/mysqld --version | grep -oe '5\.[567]' | head -n1) == "5.7" ]; then
		# Fix for https://github.com/Percona-QA/PTB/issues/27
		# Change here to use different innodb_page_size, default is 16k		
		S_INNODB_PAGE_SIZE[$instanceid]="64k"
		# Fix for https://github.com/Percona-QA/PTB/issues/26
		S_EARLY_PLUGIN_LOAD[$instanceid]="keyring_file.so"
		# Fix for https://github.com/Percona-QA/PTB/issues/42 -> keep keyring file outside datadir
		S_KEYRING_FILE_DATA[$instanceid]="${S_BASEDIR[$instanceid]}/keyring"
		fi
		S_SSL_CA[$instanceid]="${S_BASEDIR[$instanceid]}/newcerts/ca.pem"
		S_SSL_CERT[$instanceid]="${S_BASEDIR[$instanceid]}/newcerts/server-cert.pem"
		S_SSL_KEY[$instanceid]="${S_BASEDIR[$instanceid]}/newcerts/server-key.pem"
		# Client SSL
		S_SSL_CA_CLIENT[$instanceid]="${S_BASEDIR[$instanceid]}/newcerts/ca.pem"
		S_SSL_CERT_CLIENT[$instanceid]="${S_BASEDIR[$instanceid]}/newcerts/client-cert.pem"
		S_SSL_KEY_CLIENT[$instanceid]="${S_BASEDIR[$instanceid]}/newcerts/client-key.pem"
		
		if [ $(${S_BINDIR[$PTB_OPT_server_id]}/bin/mysqld --version | grep -oe '5\.[567]' | head -n1) == "5.6" ] || [ $(${S_BINDIR[$PTB_OPT_server_id]}/bin/mysqld --version | grep -oe '5\.[567]' | head -n1) == "5.7" ]; then		
		# GTID
		S_MASTER_INFO_REPOSITORY[$instanceid]="table"
		S_RELAY_LOG_INFO_REPOSITORY[$instanceid]="table"
		S_GTID_MODE[$instanceid]="ON"
		fi
			
		
	fi
	ptb_report_idebug_leave "ptb_init_server_instance($1, $2, $3) = $rc"
	return $rc
}
################################################################################
# INTERNAL - init all server instances.
#
function ptb_init_all_server_instances()
{
	ptb_report_idebug_enter "ptb_init_all_server_instances()"

	local rc=$PTB_RET_SUCCESS

	local dirs=`ls $PTB_DATADIR`
	local dir=
	for dir in $dirs; do
		if [ -d "$PTB_DATADIR/$dir" ] && [ -e "$PTB_DATADIR/$dir/.ptb.id" ]; then
			local instance=
			local bindir=
			local port=
			read instance bindir port <"$PTB_DATADIR/$dir/.ptb.id"
			ptb_init_server_instance "$instance" "$bindir" "$port"
		fi
	done
	ptb_report_idebug_leave "ptb_init_all_server_instances() = $rc"
	return $rc
}
################################################################################
# PUBLIC - creates a server instance at a specific server id.
#
# $1 - required, instance id.
# $2 - required, BINDIR that is the root of the server install where the ./bin
#      can be found.
# $3 - required, PORT for the server to run on.
function ptb_create_server_instance()
{
	ptb_report_idebug_enter "ptb_create_server_instance($1,$2,$3)"
	
	local instanceid=$1
	local bindir=$2
	local port=$3
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_create_server_instance($1, $2, $3) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif ! ptb_is_integer "$port"; then
		ptb_report_error "ptb_create_server_instance($1, $2, $3) - Invalid server port number."
		rc=$PTB_RET_INVALID_PORT
	elif [ ! -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_create_server_instance($1, $2, $3) - Invalid server instance id specified, an instance already exists there."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "$bindir" ]; then
		ptb_report_error "ptb_create_server_instance($1, $2, $3) - Invalid server binary root directory specified."
		rc=$PTB_RET_INVALID_BINDIR
	elif [ ! -d "$bindir" ]; then
		ptb_report_error "ptb_create_server_instance($1, $2, $3) - Invalid server binary root directory specified, directory not found."
		rc=$PTB_RET_INVALID_BINDIR
	else
		ptb_report_info "ptb_create_server_instance($1, $2, $3) - Creating server instance."
		# TODO, should walk all other known server instances looking for creation on same port
		if [ -e "$PTB_DATADIR/$instanceid" ]; then
			ptb_runcmd rm -rf $PTB_DATADIR/$instanceid
		fi
		ptb_init_server_instance "$instanceid" "$bindir" "$port"
		rc=$?
		if [ $rc -eq 0 ]; then
			ptb_runcmd mkdir ${S_BASEDIR[$instanceid]}
			ptb_runcmd mkdir ${S_DATADIR[$instanceid]}
			ptb_runcmd mkdir ${S_TMPDIR[$instanceid]}
			echo "$instanceid ${S_BINDIR[$instanceid]} ${S_PORT[$instanceid]}" > ${S_IDFILE[$instanceid]}
		
			ptb_create_defaults_file "$instanceid"
		fi
	fi
	ptb_report_idebug_leave "ptb_create_server_instance($1, $2, $3) = $rc"
	return $rc
}
################################################################################
# PUBLIC - deletes a specific server instance, data, options and everything.
#
# $1 - required, server instance id or NULL to delete all instances.
function ptb_delete_server_instance()
{
	ptb_report_idebug_enter "ptb_delete_server_instance($1)"

	local instanceid=$1
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_delete_server_instance($1) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_server_instance($1) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_server_instance($1) - Can not delete server instance $instanceid with the server still running as PID `cat ${S_PIDFILE[$instanceid]}` on port ${S_PORT[$instanceid]} and ${S_SOCKET[$instanceid]}."
		rc=$PTB_RET_SERVER_RUNNING
	else
		ptb_report_info "ptb_delete_server_instance($1) - Deleting server instance."
		ptb_runcmd rm -rf $PTB_DATADIR/$instanceid

		S_BINDIR[$instanceid]=
		S_BASEDIR[$instanceid]=
		S_DATADIR[$instanceid]=
		S_TMPDIR[$instanceid]=
		S_PORT[$instanceid]=
		S_SOCKET[$instanceid]=
		S_PIDFILE[$instanceid]=
		S_LOGFILE[$instanceid]=
		S_QUERYLOGFILE[$instanceid]=
		S_DEFAULTSFILE[$instanceid]=
		S_OPTIONSFILE[$instanceid]=
		S_IDFILE[$instanceid]=
		S_INNODB_PAGE_SIZE[$instanceid]=
		S_EARLY_PLUGIN_LOAD[$instanceid]=
		S_KEYRING_FILE_DATA[$instanceid]=
		S_SSL_CA[$instanceid]=
		S_SSL_CERT[$instanceid]=
		S_SSL_KEY[$instanceid]=
		S_SSL_CA_CLIENT[$instanceid]=
		S_SSL_CERT_CLIENT[$instanceid]=
		S_SSL_KEY_CLIENT[$instanceid]=
		S_MASTER_INFO_REPOSTORY[$instanceid]=
		S_RELAY_LOG_INFO_REPOSITORY[$instanceid]=
		S_GTID_MODE[$instanceid]=
		
	fi
	ptb_report_idebug_leave "ptb_delete_server_instance($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - deletes all server instances, data, options and everything.
function ptb_delete_all_server_instances()
{
	ptb_report_idebug_enter "ptb_delete_all_server_instances()"

	local count="${#S_BASEDIR[@]}"
	local rc=$PTB_RET_SUCCESS

	local instance=
	for (( instance=1; instance < count; instance++ )); do
		if [ -n "${S_BASEDIR[$instance]}" ]; then
			ptb_delete_server_instance "$instance"
		fi
	done
	ptb_report_idebug_leave "ptb_delete_all_server_instances() = $rc"
	return $rc
}
################################################################################
# PUBLIC - creates an initial database for a server instance by calling
# mysql_install_sb.
#
# $1 - required, instance id.
function ptb_create_server_data()
{
	ptb_report_idebug_enter "ptb_create_server_data($1)"
	
	# Creating SSL things
	ptb_createssl "${S_BASEDIR[$instanceid]}"

	local instanceid=$1
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_create_server_data($1) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_create_server_data($1) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_create_server_data($1) - Can not create server data for $instanceid with the server still running as PID `cat ${S_PIDFILE[$instanceid]}` on port ${S_PORT[$instanceid]} and ${S_SOCKET[$instanceid]}."
		rc=$PTB_RET_SERVER_RUNNING
	else
		ptb_report_info "ptb_create_server_data($1) - Creating server data in ${S_DATADIR[$instanceid]}."
		ptb_runcmd rm -rf ${S_DATADIR[$instanceid]}
		ptb_runcmd mkdir ${S_DATADIR[$instanceid]}
		if [ $(${S_BINDIR[$instanceid]}/bin/mysqld --version | grep -oe '5\.[567]' | head -n1) == "5.7" ]; then
			ptb_runcmdpathlog ${S_BINDIR[$instanceid]}/scripts:${S_BINDIR[$instanceid]}/bin ${S_BASEDIR[$instanceid]}/installdb.log mysqld --defaults-file=${S_DEFAULTSFILE[$instanceid]} --initialize-insecure --basedir=${S_BINDIR[$instanceid]}
			# Generating RSA keys inside datadir
			ptb_creatersa "${S_DATADIR[$instanceid]}"
		else
			ptb_runcmdpathlog ${S_BINDIR[$instanceid]}/scripts:${S_BINDIR[$instanceid]}/bin ${S_BASEDIR[$instanceid]}/installdb.log mysql_install_db --defaults-file=${S_DEFAULTSFILE[$instanceid]} --basedir=${S_BINDIR[$instanceid]}
			# Generating RSA keys inside datadir
			ptb_creatersa "${S_DATADIR[$instanceid]}"
		fi
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "ptb_create_server_data($1) - mysql_install_db failed with $rc, logfile:"
			ptb_report_file_error ${S_BASEDIR[$instanceid]}/installdb.log
			rc=$PTB_RET_FAILED
		fi
	fi
	ptb_report_idebug_leave "ptb_create_server_data($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - creates initial server databases for all servers by calling
# mysql_install_sb.
function ptb_create_all_servers_data()
{
	ptb_report_idebug_enter "ptb_create_all_servera_data()"

	local count="${#S_BASEDIR[@]}"
	local rc=$PTB_RET_SUCCESS

	local instance=
	for (( instance=1; instance < count; instance++ )); do
		if [ -n "${S_BASEDIR[$instance]}" ]; then
			ptb_create_server_data "$instance"
		fi
	done
	ptb_report_idebug_leave "ptb_create_all_servers_data() = $rc"
	return $rc
}
################################################################################
# PUBLIC - deletes a specific server database for a server instance.
#
# $1 - required, server instance id to delete data.
function ptb_delete_server_data()
{
	ptb_report_idebug_enter "ptb_delete_server_data($1)"

	local instanceid=$1
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_delete_server_data($1) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_server_data($1) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_server_data($1) - Can not delete server data $instanceid with the server still running as PID `cat ${S_PIDFILE[$instanceid]}` on port ${S_PORT[$instanceid]} and ${S_SOCKET[$instanceid]}."
		rc=$PTB_RET_SERVER_RUNNING
	else
		ptb_report_info "ptb_delete_server_data($1) - Deleting server data in ${S_DATADIR[$instanceid]}"
		ptb_runcmd rm -rf ${S_DATADIR[$instanceid]}
		ptb_runcmd mkdir ${S_DATADIR[$instanceid]}
	fi
	ptb_report_idebug_leave "ptb_delete_server_data($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - deletes server databases for all server instances.
function ptb_delete_all_servers_data()
{
	ptb_report_idebug_enter "ptb_delete_all_servers_data()"

	local rc=$PTB_RET_SUCCESS

	local instance=
	for (( instance=1; instance < count; instance++ )); do
		ptb_delete_server_data "$instance"
	done
	ptb_report_idebug_leave "ptb_delete_all_servers_data() = $rc"
	return $rc
}
################################################################################
# PUBLIC - used to add a new server option to a server defaults file, does not
# overwrite or replce an previous option by the same name.
#
# $1 - required, instance id.
# $2 - required, option in toe form or <name>=<value> or just <name>=.
function ptb_add_server_option()
{
	local instanceid="$1"
	shift
	local option_arg="$@"
	local rc=$PTB_RET_SUCCESS

	ptb_report_idebug_enter "ptb_add_server_option($instanceid,$option_arg)"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_add_server_option($instanceid,$option_arg) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_OPTIONSFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_add_server_option($instanceid,$option_arg) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "$option_arg" ]; then
		ptb_report_error "ptb_add_server_option($instanceid,$option_arg) - Invalid server option argument."
		rc=$PTB_RET_INVALID_ARGUMENT
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_set_server_option($instanceid,$option_arg) - Can not add server option with the server still running as pid[`cat ${S_PIDFILE[$instanceid]}`] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]."
		rc=$PTB_RET_SERVER_RUNNING
	else
		local file="${S_OPTIONSFILE[$instanceid]}"

		local opt_name="`expr match \"${option_arg}\" '\([a-zA-Z0-9_\-]*\=\)'`"
		local opt_value="${option_arg:${#opt_name}}"
		if [ -z "$opt_name" ]; then opt_name="$opt_value"; opt_value=; fi
		opt_name="${opt_name%"="}"

		if [ -z "${opt_value}" ]; then
			echo "$opt_name" >> $file
		else
			echo "$opt_name=$opt_value" >> $file
		fi
		sort ${file} > ${file}.1
		rm -f $file
		mv ${file}.1 ${file}
		ptb_create_defaults_file "$instanceid"
		rc=$?
	fi
	ptb_report_idebug_leave "ptb_add_server_option($instanceid,$option_arg) = $rc"
	return $rc
}
################################################################################
# PUBLIC - sets/replaces a server option, adding a new value to the first
# instance of a found option or adding a new option/value pair if none are
# found.
#
# $1 - required, instance id.
# $2 - required, option in the form or <name>=<value> or just <name>=.
function ptb_set_server_option()
{
	local instanceid=$1
	shift
	local option_arg=$@
	local rc=$PTB_RET_SUCCESS

	ptb_report_idebug_enter "ptb_set_server_option($instanceid,$option_arg)"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_set_server_option($instanceid,$option_arg) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_OPTIONSFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_set_server_option($instanceid,$option_arg) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "$option_arg" ]; then
		ptb_report_error"ptb_set_server_option($instanceid,$option_arg) - Invalid server option argument."
		rc=$PTB_RET_INVALID_ARGUMENT
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_set_server_option($instanceid,$option_arg) - Can not set server option with the server still running as pid[`cat ${S_PIDFILE[$instanceid]}`] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]."
		rc=$PTB_RET_SERVER_RUNNING
	else
		local file="${S_OPTIONSFILE[$instanceid]}"
		local line=

		local opt_name="`expr match \"${option_arg}\" '\([a-zA-Z0-9_\-]*\=\)'`"
		local opt_value="${option_arg:${#opt_name}}"
		if [ -z "$opt_name" ]; then opt_name="$opt_value"; opt_value=; fi
		opt_name="${opt_name%"="}"
			
		local values[0]=
		local found=0
		if [ -e $file ]; then
			while read line; do
				local cfg_opt_name="`expr match \"${line}\" '\([a-zA-Z0-9_\-]*\=\)'`"
				local cfg_opt_value="${line:${#cfg_opt_name}}"
				if [ -z "$cfg_opt_name" ]; then cfg_opt_name="$opt_value"; cfg_opt_value=; fi
				cfg_opt_name="${cfg_opt_name%"="}"

				if [ "$opt_name" = "$cfg_opt_name" ]; then
					if [ -z "$opt_value" ]; then
						values[${#values[@]}]="$opt_name"
					else
						values[${#values[@]}]="$opt_name=$opt_value"
					fi
					found=1
				else
					values[${#values[@]}]="$line"
				fi
			done < $file
			ptb_runcmd rm -f $file
		fi
		if [ $found -eq 0 ]; then
			values[${#values[@]}]="$opt_name=$opt_value"
		fi

		count=${#values[@]}
		for (( line=0; line < count; line++ )); do
			echo "${values[$line]}" >> $file
		done
		sort ${file} > ${file}.1
		rm -f $file
		mv ${file}.1 ${file}

		ptb_create_defaults_file "$instanceid"
		rc=$?
	fi
	ptb_report_idebug_leave "ptb_set_server_option($instanceid,$option_arg) = $rc"
	return $rc
}
################################################################################
# PUBLIC - deletes all then sets server options from a list of options in the
#          form of "name=value name=value name=value".
# $1 - required, instance id.
# $2 - required, options.
function ptb_set_all_server_options()
{
	local instanceid=$1
	shift
	local option_arg=$@
	local rc=$PTB_RET_SUCCESS

	ptb_report_idebug_enter "ptb_set_all_server_options($instanceid,$option_arg)"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_set_all_server_options($instanceid,$option_arg) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_OPTIONSFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_set_all_server_options($instanceid,$option_arg) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "$option_arg" ]; then
		ptb_report_error"ptb_set_all_server_options($instanceid,$option_arg) - Invalid server option argument."
		rc=$PTB_RET_INVALID_ARGUMENT
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_set_all_server_options($instanceid,$option_arg) - Can not set server option with the server still running as pid[`cat ${S_PIDFILE[$instanceid]}`] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]."
		rc=$PTB_RET_SERVER_RUNNING
	else
		ptb_delete_all_server_options $instanceid
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "ptb_set_all_server_options($instanceid,$option_arg) - ptb_delete_all_server_options($instanceid) failed with $rc."
		else
			local opt=
			for opt in $option_arg; do
				ptb_set_server_option $instanceid $opt
				rc=$?
				if [ $rc -ne 0 ]; then
					ptb_report_error "ptb_set_all_server_options($instanceid,$option_arg) - ptb_set_server_option($instanceid, $opt) failed with $rc."
					break
				fi
			done
		fi
	fi
	ptb_report_idebug_leave "ptb_set_all_server_options($instanceid,$option_arg) = $rc"
	return $rc
}
################################################################################
# PUBLIC - removes all instances of a server option.
#
# $1 - required, instance id.
# $2 - required, option name.
function ptb_delete_server_option()
{
	local instanceid=$1
	local option_arg=$2

	local rc=$PTB_RET_SUCCESS

	ptb_report_idebug_enter "ptb_delete_server_option($instanceid,$option_arg)"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_delete_server_option($instanceid,$option_arg) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_OPTIONSFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_server_option($instanceid,$option_arg) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "$option_arg" ]; then
		ptb_report_error "ptb_delete_server_option($instanceid,$option_arg) - Invalid server option name."
		rc=$PTB_RET_INVALID_ARGUMENT
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_server_option($instanceid,$option_arg) - Can not delete server option with the server still running as pid[`cat ${S_PIDFILE[$instanceid]}`] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]."
		rc=$PTB_RET_SERVER_RUNNING
	else
		local file="${S_OPTIONSFILE[$instanceid]}"
		local line=

		local values[0]=
		if [ -e $file ]; then
			while read line; do
				local cfg_opt_name="`expr match \"${line}\" '\([a-zA-Z0-9_\-]*\=\)'`"
				local cfg_opt_value="${line:${#cfg_opt_name}}"
				if [ -z "$cfg_opt_name" ]; then cfg_opt_name="$opt_value"; cfg_opt_value=; fi
				cfg_opt_name="${cfg_opt_name%"="}"
			
				if [ "$cfg_opt_name" != "$option_arg" ]; then
					values[${#values[@]}]="$line"
				fi
			done < $file
			ptb_runcmd rm -f $file
		fi
	
		count=${#values[@]}
		for (( line=1; line < count; line++ )); do
			echo "${values[$line]}" >> "$file"
		done

		sort ${file} > ${file}.1
		rm -f $file
		mv ${file}.1 ${file}

		ptb_create_defaults_file "$instanceid"
		rc=$?
	fi
	ptb_report_idebug_leave "ptb_delete_server_option($instanceid,$option_arg) = $rc"
	return $rc
}
#################################################################################
# PUBLIC - removes all of a servers options.
#
# $1 - required, instance id.
function ptb_delete_all_server_options()
{
	local instanceid=$1

	local rc=$PTB_RET_SUCCESS

	ptb_report_idebug_enter "ptb_delete_all_server_options($1)"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_delete_all_server_options($1) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_OPTIONSFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_all_server_options($1) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_delete_all_server_options($1) - Can not delete all server options with the server still running as pid[`cat ${S_PIDFILE[$instanceid]}`] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]."
		rc=$PTB_RET_SERVER_RUNNING
	else
		ptb_runcmd rm -f ${S_OPTIONSFILE[$instanceid]}
		ptb_create_defaults_file "$instanceid"
		rc=$?
	fi
	ptb_report_idebug_leave "ptb_delete_all_server_options($1) = $rc"
	return $rc
}
###############################################################################
# PUBLIC - parses an array named in $1 of:
#	   "option1=value1 value2"
#	   "option2=value1 value2"
#	    into the array named in $1 which will result in an array of:
#	   "option1=value1 option2=value1"
#	   "option1=value1 option2=value2"
#	   "option1=value2 option2=value1"
#	   "option1=value2 option2=value2"
#
# $1 - required, the name of the aray in which individual name values are stored
# $2 - required, the name of the array in which to store individual options
#
function ptb_parse_options_matrix()
{
	local sourcename=$1
	local resultname=$2
	local rc=$PTB_RET_SUCCESS
	local curlist=( ' ' )
	local options="${sourcename}[@]"
	for option in "${!options}"; do
		local opt_name="`expr match \"${option}\" '\([a-zA-Z0-9_\-]*\=\)'`"
		local opt_value="${option:${#opt_name}}"
		if [ -z "$opt_name" ]; then opt_name="$opt_value"; opt_value=; fi
		opt_name="${opt_name%"="}"
		local newoptionslist=( )
		if [ -n "$opt_value" ]; then
			local newoptionslist_idx=0
			for optionvalue in ${opt_value}; do
				if [ -z "$optionvalue" ]; then
					newoptionslist[$newoptionslist_idx]="$opt_name"
				else
					newoptionslist[$newoptionslist_idx]="$opt_name=$optionvalue"
				fi
				newoptionslist_idx=`expr $newoptionslist_idx + 1`
	 		done
		else
			opt_name="$option"
			opt_value=
			newoptionslist[$newoptionslist_idx]="$opt_name"
		fi
		
		local newcomboslist=( )
		local newcomboslist_idx=0
		
		local optionspec
		for optionspec in "${newoptionslist[@]}"; do
			local optionset
			for optionset in "${curlist[@]}"; do 
				newcomboslist[$newcomboslist_idx]="$optionspec $optionset"
				newcomboslist_idx=`expr $newcomboslist_idx + 1`
			done
		done
		curlist=("${newcomboslist[@]}")
	done
	local optionset
	local optionset_idx=0
	for optionset in "${curlist[@]}"; do
		eval "$resultname[$optionset_idx]=\"$optionset\""
		optionset_idx=`expr $optionset_idx + 1`
	done
}
###############################################################################
# PUBLIC - starts a server instance.
#
# $1 - required, server instance id to start.
function ptb_start_server()
{
	ptb_report_idebug_enter "ptb_start_server($1)"

	local instanceid=$1
	local pid=
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_start_server($1) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_start_server($1) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		pid=`cat ${S_PIDFILE[$instanceid]}`
		if [ ! -z "$pid" ] && [ ! -z "`ps -p ${pid} -o cmd --no-headers`" ]; then  
			ptb_report_error "ptb_start_server($1) - Can not start server with the server still running as pid[`cat ${S_PIDFILE[$instanceid]}`] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]."
			rc=$PTB_RET_SERVER_RUNNING
		else
			ptb_runcmd rm -f ${S_PIDFILE[$instanceid]}
		fi
	fi

	if [ $rc -ne $PTB_RET_SUCCESS ]; then
		ptb_report_idebug_leave "ptb_start_server($1) = $rc"
		return $rc
	fi

	local servercmd="( PATH=${S_BINDIR[$instanceid]}/bin:${S_BINDIR[$instanceid]}/libexec:$PATH; mysqld --defaults-file=${S_DEFAULTSFILE[$instanceid]} &> ${S_LOGFILE[$instanceid]} & )"

	ptb_report_info "ptb_start_server($1) - Starting server with command $servercmd."
	( PATH=${S_BINDIR[$instanceid]}/bin:${S_BINDIR[$instanceid]}/libexec:$PATH; mysqld --defaults-file=${S_DEFAULTSFILE[$instanceid]} --core-file &> ${S_LOGFILE[$instanceid]} & ) 
	
	rc=$?
	if [ $rc -eq 0 ]; then
		local i=0
		while [ ! -e ${S_SOCKET[$instanceid]} ] && [ $i -lt 120 ]; do
			sleep 1
			i=`expr $i + 1`
		done

		if [ -e ${S_PIDFILE[$instanceid]} ]; then
			pid=`cat ${S_PIDFILE[$instanceid]}`
			if [ ! -z "`ps -p ${pid} -o cmd --no-headers`" ]; then
				ptb_report_info "ptb_start_server($1) - Started server as pid[${pid}] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]"
				ptb_report_idebug_leave "ptb_start_server($1) = $rc"
				return $rc
			fi
		fi
	fi
	ptb_report_error "ptb_start_server($1) - server failed to start, with $rc and command $servercmd logfile:"
	ptb_report_file_error ${S_LOGFILE[$instanceid]}
	rc=$PTB_RET_FAILED
	ptb_report_idebug_leave "ptb_start_server($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - starts all server instances.
function ptb_start_all_servers()
{
	ptb_report_idebug_enter "ptb_start_all_servers()"

	local count="${#S_BASEDIR[@]}"
	local rc=$PTB_RET_SUCCESS
		
	local instance=
	for (( instance=1; instance < count; instance++ )); do
		if [ -n "${S_BASEDIR[$instance]}" ]; then
			ptb_start_server "$instance"
		fi
	done

	ptb_report_idebug_leave "ptb_start_all_servers() = $rc"
	return $rc
}
################################################################################
# PUBLIC - cleanly stops a server instance.
#
# $1 - required, server instance id to stop.
function ptb_stop_server()
{
	ptb_report_idebug_enter "ptb_stop_server($1)"

	local instanceid=$1
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_stop_server($1) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_stop_server($1) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_stop_server($1) - Can not stop server instance, server appears stopped already."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	else
		local pid=`cat ${S_PIDFILE[$instanceid]}`
		if [ ! -z "$pid" ] && [ -z "`ps -p ${pid} -o cmd --no-headers`" ]; then
			ptb_report_error "ptb_stop_server($1) - Can not stop server, server appears stopped already."
			ptb_runcmd rm -f "${S_PIDFILE[$instanceid]}"
			rc=$PTB_SERVER_NOT_RUNNING
		else
			ptb_report_info "ptb_stop_server($1) - Stopping server as pid[${pid}] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]"
			ptb_runcmdpath ${S_BINDIR[$instanceid]}/bin mysqladmin --defaults-file=${S_DEFAULTSFILE[$instanceid]} shutdown
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_stop_server($1) - mysqladmin shutdown failed with $rc"
				rc=$PTB_RET_FAILED
			fi
		fi
	fi
	
	ptb_report_idebug_leave "ptb_stop_server($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - stops all server instances.
function ptb_stop_all_servers()
{
	ptb_report_idebug_enter "ptb_stop_all_servers()"

	local count="${#S_BASEDIR[@]}"
	local rc=$PTB_RET_SUCCESS
		
	local instance=
	for (( instance=1; instance < count; instance++ )); do
		if [ -n "${S_BASEDIR[$instance]}" ] && [ -e "${S_PIDFILE[$instance]}" ];then
			ptb_stop_server "$instance"
		fi
	done

	ptb_report_idebug_leave "ptb_stop_all_servers() = $rc"
	return $rc
}
################################################################################
# PUBLIC - abruptly terminates a specific running servery.
#
# $1 - required, server instance id to kill.
function ptb_kill_server()
{
	ptb_report_idebug_enter "ptb_kill_server($1)"
	local instanceid=$1
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_kill_server($1) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_kill_server($1) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_kill_server($1) - Can not stop server instance, server appears stopped already."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	else
		local pid=`cat ${S_PIDFILE[$instanceid]}`
		if [ ! -z "$pid" ] && [ -z "`ps -p ${pid} -o cmd --no-headers`" ]; then
			ptb_report_error "ptb_kill_server($1) - Can not kill server, server appears stopped already."
			ptb_runcmd rm -f "${S_PIDFILE[$instanceid]}"
			rc=$PTB_SERVER_NOT_RUNNING
		else
			ptb_report_info "ptb_kill_server($1) - Killing server as pid[${pid}] port[${S_PORT[$instanceid]}] socket[${S_SOCKET[$instanceid]}]"
			ptb_runcmd kill -s SIGKILL $pid 2>/dev/null
		fi

		if [ -e ${S_PIDFILE[$instanceid]} ]; then
			ptb_runcmd rm -f ${S_PIDFILE[$instanceid]}
		fi
		if [ -e ${S_SOCKET[$instanceid]} ]; then
			ptb_runcmd rm -f ${S_SOCKET[$instanceid]}
		fi
	fi

	ptb_report_idebug_leave "ptb_kill_server($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - kills all server instances.
function ptb_kill_all_servers()
{
	ptb_report_idebug_enter "ptb_kill_all_servers()"

	local count="${#S_BASEDIR[@]}"
	local rc=$PTB_RET_SUCCESS
		
	local instance=
	for (( instance=1; instance < count; instance++ )); do
		if [ -n "${S_BASEDIR[$instance]}" ] && [ -e "${S_PIDFILE[$instance]}" ];then
			ptb_kill_server $instance
		fi
	done

	ptb_report_idebug_leave "ptb_kill_all_servers() = $rc"
	return $rc
}
################################################################################
# PUBLIC - send sql to a specific server
# $1 required, instance id
# $2 required, raw sql or the name of  file which contains sql
function ptb_sql()
{
	local instanceid="$1"
	shift
	local sql="$*"
	local rc=$PTB_RET_SUCCESS
	
	ptb_report_idebug_enter "ptb_sql($instanceid, $sql)"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_sql($instanceid, $sql) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_sql($instanceid, $sql) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_sql($instanceid, $sql) - Can not send SQL to server, server appears stopped."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	elif [ -z "$sql" ]; then
		ptb_report_error "ptb_sql($instanceid, $sql) - Invalid sql."
		rc=$PTB_RET_INVALID_ARGUMENT
	elif [ -f "$sql" ]; then
		ptb_report_info "ptb_sql($instanceid, $sql) - Sending sql from file to server:"
		ptb_report_file_info $sql
		ptb_runcmdpath ${S_BINDIR[$instanceid]}/bin mysql --defaults-file=${S_DEFAULTSFILE[$instanceid]} < $sql
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "ptb_sql($instanceid, $sql) - Sending sql from file to server failed with $rc:"
			ptb_report_file_error $sql
			rc=$PTB_RET_FAILED
		fi
	else
		ptb_report_info "ptb_sql($instanceid, $sql) - Sending sql to server."
		ptb_runcmdpath ${S_BINDIR[$instanceid]}/bin mysql --defaults-file=${S_DEFAULTSFILE[$instanceid]} -e "$sql"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "ptb_sql($instanceid, $sql) - Sending sql to server failed with $rc."
			rc=$PTB_RET_FAILED
		fi
	fi
	ptb_report_idebug_leave "ptb_sql($instanceid, $sql) = $rc"
	return $rc
}

################################################################################
# PUBLIC - ptb_rowset API for digging through query results.
# Stored in the following values
#  PTB_${arrayname}_rowcount
#  PTB_${arrayname}_columnnames[]
#  PTB_${arrayname}_${columnname}[] # array of row/cell data
function ptb_rowset_init()
{
	local arrayname=$1

	ptb_rowset_unset $arrayname

	eval "PTB_${arrayname}_rowcount=0"
}
function ptb_rowset_unset()
{
	local arrayname=$1

	eval "rs_unset_columncount=\${#PTB_${arrayname}_columnnames[@]}"
	local columnnumber=0
	while [ $columnnumber -lt $rs_unset_columncount ]; do
		eval "rs_unset_columnname=\${PTB_${arrayname}_columnnames[${columnnumber}]}"
		eval "unset PTB_${arrayname}_${rs_unset_columnname}"
		columnnumber=`expr $columnnumber + 1`
	done
	eval "unset PTB_${arrayname}_rowcount"
	eval "unset PTB_${arrayname}_columnnames"
}
function ptb_rowset_setcellvalue()
{
	local arrayname=$1
	local columnname=$2
	local rownumber=$3
	local value=$4

	# see if the cell is in a new row, if so, increase the row count
	# to encompass the new cell
	eval "rs_scv_rowcount=\${PTB_${arrayname}_rowcount}"
	if [ -z "$rs_scv_rowcount" ]; then
		ptb_rowset_init $arrayname
		rs_scv_rowcount=0
	fi
	if [ $rownumber -ge $rs_scv_rowcount ]; then
		eval "PTB_${arrayname}_rowcount=`expr $rownumber + 1`"
	fi

	# see if the column name is in the columns list, if not,
	# add it to the end
	eval "rs_scv_columncount=\${#PTB_${arrayname}_columnnames[@]}"
	local columnnumber=0
	while [ $columnnumber -lt $rs_scv_columncount ]; do
		eval "rs_scv_columnname=\${PTB_${arrayname}_columnnames[${columnnumber}]}"
		if [ "$columnname" = "$rs_scv_columnname" ]; then
			break
		fi
		columnnumber=`expr $columnnumber + 1`
	done
	if [ $columnnumber -ge $rs_scv_columncount ]; then
		eval "PTB_${arrayname}_columnnames[${rs_scv_columncount}]=$columnname"
	fi

	# set the value
	local temp="PTB_${arrayname}_${columnname}[${rownumber}]=\"$value\""
	eval $temp 
}
function ptb_rowset_getcolumnname()
{
	local arrayname=$1
	local columnnumber=$2
	local resultname=$3
	local temp="${resultname}=\${PTB_${arrayname}_columnnames[${columnnumber}]}"
	eval $temp
}
function ptb_rowset_getcolumncount()
{
	local arrayname=$1
	local temp="rs_gcc=\${#PTB_${arrayname}_columnnames[@]}"
	eval $temp
	if [ -z "$rs_gcc" ]; then
		return 0
	fi
	return $rs_gcc
}
function ptb_rowset_getrowcount()
{
	local arrayname=$1
	local temp="rs_grc=\$PTB_${arrayname}_rowcount"
	eval $temp
	if [ -z "$rs_grc" ]; then
		return 0
	fi
	return $rs_grc
}
function ptb_rowset_getcellvalue()
{
	local arrayname=$1
	local columnname=$2
	local rownumber=$3
	local resultname=$4

	local temp="${resultname}=\${PTB_${arrayname}_${columnname}[${rownumber}]}"
	eval $temp
}
################################################################################
# PUBLIC - send sql to a specific server and parse the result set into a rowset
# $1 required, instance id
# $2 required, name of rowset to place results in
# $3 required, raw sql
function ptb_sql_query()
{
	local instanceid=$1
	shift
	local rowset=$1
	shift
	local sql="$*"
	local rc=$
	local prefix="ptb_sql_query($instanceid, $rowset, \"$sql\")"

	ptb_report_idebug_enter "$prefix"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "$prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "$prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "$prefix - Can not send SQL to server, server appears stopped."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	elif [ -z "$sql" ]; then
		ptb_report_error "$prefix - Invalid sql."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		ptb_report_info "$prefix - Sending sql to server."
		local sqlresultfile="${S_BASEDIR[$instanceid]}/${rowset}.txt"

		ptb_runcmdpath ${S_BINDIR[$instanceid]}/bin mysql --defaults-file=${S_DEFAULTSFILE[$instanceid]} -e "$sql\\G" &> $sqlresultfile
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$prefix - Sending sql to server failed with $rc."
			rc=$PTB_RET_FAILED
		else
			ptb_rowset_init $rowset
			local line=
			local rownumber=0
			while read line; do
				local match=`expr match "$line" '[*]* '`
				if [ $match -ne 0 ]; then
					line=${line:${match}}
					match=`expr match "$line" '[0-9]*'`
					line=${line:0:${match}}
					rownumber=`expr $line - 1`
					continue
				fi
				match=`expr match "$line" '[a-zA-Z_]*'`
				local name=${line:0:$match}
				local value=${line:`expr $match + 2`}
				ptb_rowset_setcellvalue $rowset $name $rownumber "$value"
			done < "$sqlresultfile"

		fi
	fi
	ptb_report_idebug_leave "$prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - configures a server to be a slave of another and starts replication
# $1 - required, slave instance id
# $2 - required, master instance id
function ptb_configure_slave_to_master()
{
	local slaveid=$1
	local masterid=$2
	local rc=$PTB_RET_SUCCESS
	local prefix="ptb_configure_slave_to_master($1, $2)"

	ptb_report_idebug_enter "$prefix"

	if ! ptb_is_integer "$slaveid" || ! ptb_is_integer "$masterid"; then
		ptb_report_error "$prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$slaveid]}" ] || [ -z "${S_BASEDIR[$masterid]}" ]; then
		ptb_report_error "$prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -e "${S_PIDFILE[$slaveid]}" ] || [ ! -e "${S_PIDFILE[$masterid]}" ]; then
		ptb_report_error "$prefix - Can not send SQL to server, server appears stopped."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	else
		ptb_sql_query $masterid cs2m_results "FLUSH TABLES WITH READ LOCK; SHOW MASTER STATUS"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$prefix - Failed to get master status with $rc."
		else
			ptb_sql $slaveid "CHANGE MASTER TO "\
"MASTER_HOST='127.0.0.1', "\
"MASTER_PORT=${S_PORT[$masterid]}, "\
"MASTER_USER='root', "\
"MASTER_LOG_FILE='${PTB_cs2m_results_File[0]}', "\
"MASTER_LOG_POS=${PTB_cs2m_results_Position[0]}"
			rc=$?
			ptb_rowset_unset cs2m_results
			if [ $rc -ne 0 ]; then
				ptb_report_error "$prefix - Failed to change master to with $rc."
			else
				ptb_sql $slaveid "START SLAVE"
				rc=$?
				if [ $rc -ne 0 ]; then
					ptb_report_error "$prefix - Failed to start slave with $rc."
				fi
			fi
		fi
	fi
	ptb_report_idebug_leave "$prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - waits for a slave to execute up to a certain position
# $1 - required, slave instance id
# $2 - required, log file name
# $3 - required log position
function ptb_sync_to_master_position()
{
	local prefix="ptb_sync_to_master_position($1, $2, $3)"
	ptb_report_idebug_enter "$prefix"
	local slaveid=$1
	local master_log_name=$2
	local master_log_position=$3
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$slaveid"; then
		ptb_report_error "$prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$slaveid]}" ]; then
		ptb_report_error "$prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -e "${S_PIDFILE[$slaveid]}" ]; then
		ptb_report_error "$prefix - Can not send SQL to server, server appears stopped."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	else
		ptb_sql_query $slaveid s2mp_results "SHOW SLAVE STATUS"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$prefix - Failed to get slaver status with $rc."
		else
			while [ "${PTB_s2mp_results_Relay_Master_Log_File[0]}" != $master_log_name ] && \
		      [ "${PTB_s2mp_results_Exec_Master_Log_Pos[0]}" != $master_log_position ]; do
				sleep 1
				ptb_sql_query $slaveid s2mp_results "SHOW SLAVE STATUS"
			done
		fi
		ptb_rowset_unset s2mp_results
	fi
	ptb_report_idebug_leave "$prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - waits for a slave to catch up to its' master.
# $1 - required, slave instance id
# $2 - required, master instance id
# $3 - optional, timeout
function ptb_sync_to_master()
{
	local slaveid=$1
	local masterid=$2
	local timeout=$3
	local rc=$PTB_RET_SUCCESS
	local prefix="ptb_sync_to_master($1, $2, $3)"
	ptb_report_idebug_enter "$prefix"

	if ! ptb_is_integer "$slaveid" || ! ptb_is_integer "$masterid"; then
		ptb_report_error "$prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$slaveid]}" ] || [ -z "${S_BASEDIR[$masterid]}" ]; then
		ptb_report_error "$prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -e "${S_PIDFILE[$slaveid]}" ] || [ ! -e "${S_PIDFILE[$masterid]}" ]; then
		ptb_report_error "$prefix - Can not send SQL to server, server appears stopped."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	else
		ptb_sql_query $masterid s2m_results "FLUSH TABLES WITH READ LOCK; SHOW MASTER STATUS"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$prefix - Failed to get master status with $rc."
		else
			ptb_sync_to_master_position $slaveid ${PTB_s2m_results_File[0]} ${PTB_s2m_results_Position[0]}
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$prefix - Failed to get sync to master position with $rc."
			fi
		fi
		ptb_rowset_unset s2m_results
	fi
	ptb_report_idebug_leave "$prefix = $rc"
	return $rc
}
###############################################################################
# Validates that tables, indexes and secondary indexes can be read
#
# $1 required, server id
function ptb_check_all_databases()
{
	local server_id=$1
	local rc=0
	local prefix="ptb_check_all_databases($server_id)"

	ptb_report_idebug_enter "$prefix"

	local query="SELECT table_schema, table_name, column_name "\
"FROM information_schema.columns "\
"WHERE table_schema != 'information_schema' AND table_schema != 'performance_schema' AND table_schema != 'mysql' AND column_key != '' "\
"ORDER BY table_schema, table_name"

	ptb_sql_query $server_id cad_results "$query"
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$prefix - \"$query\" failed with $rc."
	elif [ -z "${PTB_cad_results_rowcount}" ] || [ ${PTB_cad_results_rowcount} -eq 0 ];  then
		ptb_report_error "$prefix - \"$query\" produced no results [${PTB_cad_results_rowcount}]."
	fi

	local rownum=0
	while [ $rownum -lt $PTB_cad_results_rowcount ]; do
		query="SELECT * FROM ${PTB_cad_results_table_schema[$rownum]}.${PTB_cad_results_table_name[$rownum]}"
		ptb_sql $server_id "$query" > /dev/null
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$prefix - \"$query\" failed with $rc."
		else
			query="SELECT AVG( ${PTB_cad_results_column_name[$rownum]} ) FROM ${PTB_cad_results_table_schema[$rownum]}.${PTB_cad_results_table_name[$rownum]}"
			ptb_sql $server_id "$query" > /dev/null
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$prefix - \"$query\" failed with $rc."
			fi
		fi

		if [ $rc -ne 0 ]; then
			ptb_rowset_unset cad_results
			ptb_report_idebug_leave "$prefix = $rc"
			return $rc;
		fi
		rownum=`expr $rownum + 1`
	done
	
	ptb_rowset_unset cad_results
	ptb_report_idebug_leave "$prefix = 0"
	return 0
}
################################################################################
# PUBLIC - runs a 'prepare' type script
#
# $1 - required, server instance id
# $2 - required, prepare script to run
# $3 - required, path to tool or "" if already within path
# $4 - required, log file
# $5 - required, generate descriptor only
# $@ - extra options to pass to tool
function ptb_run_prepare()
{
	local instanceid=$1
	local prepare=$2
	local extrapath=$3
	local logfile=$4
	local gendescriptor=$5
	shift 5
	local prepare_options=$@

	local rc=$PTB_RET_SUCCESS

	local rpt_prefix="ptb_run_prepare($instanceid, $prepare, $extrapath, $logfile, $gendescriptor, $prepare_options)"
	
	ptb_report_idebug_enter $rpt_prefix

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -f "$prepare" ]; then
		ptb_report_error "$rpt_prefix - Invalid \'prepare\' script."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		local prepare_command="$prepare"
		prepare_command="${prepare_command} --server-id=$instanceid"
		prepare_command="${prepare_command} --vardir=$PTB_DATADIR"
		prepare_command="${prepare_command} --verbosity=$PTB_RPT_FILTER"
		prepare_command="${prepare_command} --prepare-logfile=$logfile"
		if [ -n "$extrapath" ]; then
			prepare_command="${prepare_command} --prepare-rootdir=$extrapath"
		fi
		if [ $gendescriptor -gt 0 ]; then
			prepare_command="${prepare_command} --gen-descriptor=1"
		fi
		local i=
		for i in ${prepare_options[@]}; do
			prepare_command="${prepare_command} --prepare-option=$i"
		done
		ptb_report_info "$rpt_prefix - Runing \"${prepare_command}\""
		ptb_runcmd ${prepare_command}
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - Command \"$prepare_command}\" failed with $rc."
		fi
	fi
	ptb_report_idebug_leave "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - launches a 'load' type script
#
# $1 - required, server instance id
# $2 - required, task id to launch at
# $3 - required, prepare script to run
# $4 - required, path to tool or "" if already within path
# $5 - required, log file
# $@ - extra options to pass to tool
function ptb_start_load()
{
	local instanceid=$1
	local taskid=$2
	local load=$3
	local extrapath=$4
	local logfile=$5
	shift 5
	local load_options=$@
	local rpt_prefix="ptb_start_load($instanceid, $taskid, $load, $extrapath, $logfile, $load_options)"

	local rc=$PTB_RET_SUCCESS
	
	ptb_report_idebug_enter "$rpt_prefix"

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif ! ptb_is_integer "$taskid"; then
		ptb_report_error "$rpt_prefix - Invalid task instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -f "$load" ]; then
		ptb_report_error "$rpt_prefix - Invalid \'load\' script."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		local load_command="$load"
		load_command="${load_command} --server-id=$instanceid"
		load_command="${load_command} --vardir=$PTB_DATADIR"
		load_command="${load_command} --verbosity=$PTB_RPT_FILTER"
		load_command="${load_command} --load-logfile=$logfile"
		if [ -n "$extrapath" ]; then
			load_command="${load_command} --load-rootdir=$extrapath"
		fi
		
		local i=
		for i in ${load_options[@]}; do
			load_command="${load_command} --load-option=$i"
		done

		ptb_report_info "$rpt_prefix - Starting \"${load_command}\""
		ptb_start_task $taskid 1 ${load_command}
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - Command \"${load_command}\" failed to start with $rc."
		fi
	fi
	ptb_report_idebug_leave "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - runs a 'backup' type script
#
# $1 - required, server instance id
# $2 - required, backup script to run
# $3 - required, path to tool or "" if already within path
# $4 - required, log file
# $5 - required, stats pipe
# $6 - required, server options
# $7 - required, backup options
# $8 - required, backup command options
function ptb_run_backup()
{
	local instanceid=$1
	local backup=$2
	local extrapath=$3
	local logfile=$4
	local stat_pipe=$5
	local server_options="$6"
	local backup_options="$7"
	local backup_command_options="$8"

	local rc=$PTB_RET_SUCCESS

	local rpt_prefix="ptb_run_backup($instanceid, $backup, $extrapath, $logfile, $stat_pipe, $server_options, $backup_options, $backup_command_options)"
	
	ptb_report_idebug_enter $rpt_prefix

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -f "$backup" ]; then
		ptb_report_error "$rpt_prefix - Invalid \'backup\' script."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		local backup_command="$backup"
		backup_command="${backup_command} --server-id=$instanceid"
		backup_command="${backup_command} --vardir=$PTB_DATADIR"
		backup_command="${backup_command} --verbosity=$PTB_RPT_FILTER"
		backup_command="${backup_command} --backup-logfile=$logfile"
		if [ -n "$extrapath" ]; then
			backup_command="${backup_command} --backup-rootdir=$extrapath"
		fi
		if [ -n "$stat_pipe" ]; then
			backup_command="${backup_command} --statistics-manager=$stat_pipe"
		fi

		local i=
		for i in ${server_options[@]}; do
			backup_command="${backup_command} --server-option=$i"
		done
		for i in ${backup_options[@]}; do
			backup_command="${backup_command} --backup-option=$i"
		done
		for i in ${backup_command_options[@]}; do
			backup_command="${backup_command} --backup-command-option=--$i"
		done

		ptb_report_info "$rpt_prefix - Runing \"${backup_command}\""
		ptb_runcmd ${backup_command}
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - Command \"${backup_command}\" failed with $rc"
		fi
	fi
	ptb_report_idebug_leave "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - runs a 'restore' type script
#
# $1 - required, server instance id
# $2 - required, restore script to run
# $3 - required, path to tool or "" if already within path
# $4 - required, log file
# $5 - required, stats pipe
# $6 - required, restore options
# $7 - required, backup command options
function ptb_run_restore()
{
	local instanceid=$1
	local restore=$2
	local extrapath=$3
	local logfile=$4
	local stat_pipe=$5
	local restore_options="$6"
	local backup_command_options="$7"

	local rc=$PTB_RET_SUCCESS

	local rpt_prefix="ptb_run_restore($instanceid, $restore, $extrapath, $logfile, $stat_pipe, $restore_options, $backup_command_options)"
	
	ptb_report_idebug_enter $rpt_prefix

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -f "$restore" ]; then
		ptb_report_error "$rpt_prefix - Invalid \'restore\' script."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		local restore_command="$restore"
		restore_command="${restore_command} --server-id=$instanceid"
		restore_command="${restore_command} --vardir=$PTB_DATADIR"
		restore_command="${restore_command} --verbosity=$PTB_RPT_FILTER"
		restore_command="${restore_command} --restore-logfile=$logfile"
		if [ -n "$extrapath" ]; then
			restore_command="${restore_command} --restore-rootdir=$extrapath"
		fi
		if [ -n "$stat_pipe" ]; then
			restore_command="${restore_command} --statistics-manager=$stat_pipe"
		fi

		local i=
		for i in ${restore_options[@]}; do
			restore_command="${restore_command} --restore-option=$i"
		done
		for i in ${backup_command_options[@]}; do
			restore_command="${restore_command} --backup-command-option=--$i"
		done

		ptb_report_info "$rpt_prefix - Runing \"${restore_command}\"" 
		ptb_runcmd ${restore_command} 
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - Command \"${restore_command}\" failed with $rc"
		fi
	fi
	ptb_report_idebug_leave "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - runs a 'cleanup' type script
#
# $1 - required, server instance id
# $2 - required, cleanup script to run
# $3 - required, path to tool or "" if already within path
# $4 - required, log file
# $5 - required, cleanup options
function ptb_run_cleanup()
{
	local instanceid=$1
	local cleanup=$2
	local extrapath=$3
	local logfile=$4
	local cleanup_options="$5"

	local rc=$PTB_RET_SUCCESS

	local rpt_prefix="ptb_run_cleanup($instanceid, $cleanup, $extrapath, $logfile, $cleanup_options)"
	
	ptb_report_idebug_enter $rpt_prefix

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "$rpt_prefix - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -f "$cleanup" ]; then
		ptb_report_error "$rpt_prefix - Invalid \'cleanup\' script."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		local cleanup_command="$cleanup"
		cleanup_command="${cleanup_command} --server-id=$instanceid"
		cleanup_command="${cleanup_command} --vardir=$PTB_DATADIR"
		cleanup_command="${cleanup_command} --verbosity=$PTB_RPT_FILTER"
		cleanup_command="${cleanup_command} --cleanup-logfile=$logfile"
		if [ -n "$extrapath" ]; then
			clenaup_command="${cleanup_command} --cleanup-rootdir=$extrapath"
		fi

		local i=
		for i in ${cleanup_options[@]}; do
			cleanup_command="${cleanup_command} --cleanup-option=$i"
		done
		
		ptb_report_info "$rpt_prefix - Runing \"${cleanup_command}\""
		ptb_runcmd ${cleanup_command}
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - Command \"${cleanup_command}\" failed with $rc"
		fi
	fi
	ptb_report_idebug_leave "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - call this to cache a database after initial preparation.
#
# $1 - required, server id to restore to
# $2 - required, cache root directory
# $3 - required, database descriptor name that must be valid to exist within
#      the filesystem.
function ptb_cache_database()
{
	ptb_report_idebug_enter "ptb_cache_database($1, $2, $3)"

	local instanceid=$1
	local cachedir=$2
	local dbdescriptor=$3

	local rc=0

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_cache_database($1, $2, $3) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_cache_database($1, $2, $3) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_cache_database($1, $2, $3) - Can not cache server data, server still running."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	elif [ -z "$cachedir" ]; then
		ptb_report_error "ptb_cache_database($1, $2, $3) - Invalid cache directory."
		rc=$PTB_RET_INVALID_ARGUMENT
	elif [ -z "$dbdescriptor" ]; then
		ptb_report_error "ptb_cache_database($1, $2, $3) - Invalid database descriptor."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		while true; do
			if [ ! -d "$cachedir" ]; then
				ptb_runcmd mkdir -p $cachedir
				rc=$?
				if [ $rc -ne 0 ]; then
					ptb_report_error "ptb_cache_database($1, $2, $3) - Could not create cache directory, failed with $rc."
					rc=$PTB_RET_FAILED
					break
				fi
			fi

			# for extra safety, look to see if an existing one already exists and if so, remove it
			local found=
			for found in $( find ${cachedir} -type d -name ${dbdescriptor}.* ); do
				if [ ! -d "$found" ]; then
					continue
				elif [ ! -f "${found}/my.cnf" ]; then
					# must have had a bad/cancelled copy earlier, delete it
					rc=0
				else
					diff ${S_DEFAULTSFILE[$instanceid]} ${found}/my.cnf
					rc=$?
				fi
				if [ $rc -eq 0 ]; then
					ptb_report_warning "ptb_cache_database($1, $2, $3) - found matching database at $found, removing."
					ptb_runcmd rm -rf $found
					rc=$?
					if [ $rc -ne 0 ]; then
						ptb_report_error "ptb_cache_database($1, $2, $3) - Could not remove matching cached database directory $found, failed with $rc."
						rc=$PTB_RET_FAILED
						break
					fi
				else
					rc=0
				fi
			done
			if [ $rc -ne 0 ]; then
				break
			fi

			# now, look for a free 'slot'
			local idx=0
			local newcachedb=
			while [ $rc -eq 0 ]; do
				newcachedb="${cachedir}/${dbdescriptor}.${idx}"
				if [ -d "$newcachedb" ]; then
					idx=`expr $idx + 1`
				else
					break
				fi
			done

			# ok, $newcachedb is it, now copy the server data and options file
			ptb_report_idebug_in "ptb_cache_database($1, $2, $3) - Copying donor server data from ${S_DATADIR[$instanceid]} to ${newcachedb}"
			ptb_runcmd mkdir -p "$newcachedb"
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_cache_database($1, $2, $3) - Could not create cache directory ${newcachedb}, failed with $rc."
				rc=$PTB_RET_FAILED
				break
			fi

			ptb_runcmd cp -r ${S_DATADIR[$instanceid]} $newcachedb
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_cache_database($1, $2, $3) - Could not copy data from donor server ${S_DATADIR[$instanceid]} to cache directory ${newcachedb}, failed with $rc."
				ptb_runcmd rm -rf $newcachedb
				rc=$PTB_RET_FAILED
				break
			fi
			ptb_runcmd cp ${S_OPTIONSFILE[$instanceid]} ${newcachedb}/cache.cnf
			break
		done
	fi

	ptb_report_idebug_leave "ptb_cache_database($1, $2, $3) = $rc"
	return $rc
}
################################################################################
# PUBLIC - call this with a server id and a name unique to a preparation script.
# It will look into the local cache of databases for one that matches both the
# name and the my.cnf used to initially create it. If a match is found, the
# server will be stopped, the database copied over that servers existing database
# and the server restarted. It will return 0 on a successful cache hit, 1 for a
# miss and > 1 for an error.
#
# $1 - required, server id to restore to
# $2 - required, cache root directory
# $3 - required, database descriptor name that must be valid to exist within
#      the filesystem that should match a previously cached database.
function ptb_restore_cached_database()
{
	ptb_report_idebug_enter "ptb_restore_cached_database($1, $2, $3)"

	local instanceid=$1
	local cachedir=$2
	local dbdescriptor=$3

	local rc=0

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_restore_cached_database($1, $2, $3) - Invalid server instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${S_BASEDIR[$instanceid]}" ]; then
		ptb_report_error "ptb_restore_cached_database($1, $2, $3) - Invalid server instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -e "${S_PIDFILE[$instanceid]}" ]; then
		ptb_report_error "ptb_restore_cached_database($1, $2, $3) - Can not cache server data, server still running."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	elif [ -z "$cachedir" ]; then
		ptb_report_error "ptb_restore_cached_database($1, $2, $3) - Invalid cache directory."
		rc=$PTB_RET_INVALID_ARGUMENT
	elif [ -z "$dbdescriptor" ]; then
		ptb_report_error "ptb_restore_cached_database($1, $2, $3) - Invalid database descriptor."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		while true; do
			if [ ! -d "$cachedir" ]; then
				rc=1
				break
			fi

			# look to see if an existing one already exists
			rc=1 # nothing found
			local found=
			for found in $( find ${cachedir} -type d -name ${dbdescriptor}.* ); do
				if [ ! -f "${found}/cache.cnf" ]; then
					# must have had a bad/cancelled copy earlier, skip it
					rc=1
				else
					diff ${S_OPTIONSFILE[$instanceid]} ${found}/cache.cnf
					rc=$?
				fi
				if [ $rc -eq 0 ]; then
					ptb_report_idebug_in "ptb_restore_cached_database($1, $2, $3) - found matching database at $found."
					break
				else
					rc=1
				fi
			done
			if [ $rc -ne 0 ]; then
				break
			fi

			# remove any existing server datadir
			if [ -d "${S_DATADIR[$instanceid]}" ]; then
				ptb_runcmd rm -rf ${S_DATADIR[$instanceid]}
				rc=$?
				if [ $rc -ne 0 ]; then
					ptb_report_error "ptb_restore_cached_database($1, $2, $3) - Could not remove target servers existing datadir ${S_DATADIR[$instanceid]}, failed with $rc."
					rc=$PTB_RET_FAILED
					break
				fi
			fi


			# copy the contents of the cache hit over to the server datadir
			ptb_report_info "ptb_restore_cached_database($1, $2, $3) - Copying cached database from $found to ${S_DATADIR[$instanceid]}"
			ptb_runcmd cp -r ${found}/data ${S_BASEDIR[$instanceid]}
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_restore_cached_database($1, $2, $3) - Could not copy cached data from $found to server ${S_DATADIR[$instanceid]}, failed with $rc."
				ptb_runcmd rm -rf ${S_BASEDIR[$instanceid]}
				rc=$PTB_RET_FAILED
				break
			fi
			break
		done
	fi
	
	ptb_report_idebug_leave "ptb_restore_cached_database($1, $2, $3) = $rc"
	return $rc
}
################################################################################
# PUBLIC - This is a higher level function that manages a cache of prepared
#          databases. Call this in place of trying to manually call the above
#          functions.
#
# Before calling this function, make sure you have created a server instance
# and set any options that you want on that server. You DO NOT need to call
# ptb_create_server_data prior to calling this nor should you have started the
# server. Upon return, this will leave the server in a fully prepared and
# running  state.
#
# $1 - required, server id
# $2 - required, cache directory location, can be empty ("") if no caching of 
#      prepared databases is desired.
# $3 - required, prepare type script that will operate on an running server
#      initialized with the default mysql database, can be empty ("") if no
#      extra database prep is required beyond a default database.
# $4 - required, extra path to prepare binaries that may be required to 
#      execute $3 (may be "")
# $5 - required, log file
# $@ - options to pass to prepare script
function ptb_prepare_server_data()
{
	instanceid=$1
	cache_dir="$2"
	test_prepare="$3"
	test_rootdir="$4"
	test_logfile="$5"
	shift 5
	prepare_opts="$@"
	ptb_report_idebug_enter "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts)"

	local rc=1 # set to cache miss
	local prepare_descriptor=

	while true; do
		# look to see if we are caching and if there is a possible matching cached database
		if [ "$test_prepare" != "" ] && [ "$cache_dir" != "" ]; then
			# get a descriptor from the prepare script
			ptb_run_prepare $instanceid $test_prepare "$test_rootdir" /tmp/ptbdesc 1 "$prepare_opts"
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_run_test_prepare($instanceid, $test_prepare, $test_rootdir, /tmp/ptbdesc, --gen-descriptor=1 $@) failed with $rc."
				break
			fi
			prepare_descriptor=`cat /tmp/ptbdesc`
			ptb_restore_cached_database $instanceid $cache_dir $prepare_descriptor
			rc=$?
		fi

		if [ $rc -gt 1 ]; then # error
			ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_restore_cached_database($instanceid, $cache_dir, $prepare_descriptor) failed with $rc."
			break
		elif [ $rc -eq 1 ]; then # cache miss
			# create the server base dataset
			ptb_create_server_data $instanceid
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_create_server_data($instanceid) failed with $rc."
				break
			fi

			# light 'er up
			ptb_start_server $instanceid
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_start_server($instanceid) failed with $rc."
				break
			fi
			# run the prepare if there is any
			if [ "$test_prepare" != "" ]; then
				ptb_run_prepare $instanceid $test_prepare "$test_rootdir" "$test_logfile" 0 "$prepare_opts"
				rc=$?
				if [ $rc -ne 0 ]; then
					ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_run_test_prepare($instanceid, $test_prepare, $prepare_opts) failed with $rc."
					break
				fi

				# cache the result if we are caching
				if [ "$cache_dir" != "" ]; then
					# stop the server
					ptb_stop_server $instanceid
					rc=$?
					if [ $rc -ne 0 ]; then
						ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_stop_server($instanceid) failed with $rc."
						break
					fi

					# cache the database
					ptb_cache_database $instanceid $cache_dir $prepare_descriptor
			
					# light 'er up
					ptb_start_server $instanceid
					rc=$?
					if [ $rc -ne 0 ]; then
						ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_start_server($instanceid) failed with $rc."
						break
					fi
				fi
			fi
		else # cache hit
			# light 'er up
			ptb_start_server $instanceid
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts) - ptb_start_server($instanceid) failed with $rc."
				break
			fi
		fi
		break
	done
	ptb_report_idebug_leave "ptb_prepare_server_data($instanceid, $cache_dir, $test_prepare, $test_rootdir, $prepare_opts)= $rc"
	return $rc
}

#####################################
# Parallel task instance management #
#####################################
T_COMMAND[0]=
T_PID[0]=

#TODO: need to do something so that task pids are sharable down to child scripts


################################################################################
# PUBLIC - launches a task and tracks its pid.
#
# $1 - required, instance id of background task
# $2 - required, comfirm whether the task has started
# $@ - optional, task command and arguments
function ptb_start_task()
{
	local taskid=$1
	local confirm=$2
	shift 2
	local command="$@"

	local rc=$PTB_RET_SUCCESS

	ptb_report_idebug_enter "ptb_start_task($taskid, $confirm, $command)"
	
	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_start_task($taskid, $confirm, $command) - Invalid task instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ ! -z "${T_PID[$taskid]}" ]; then
		ptb_report_error "ptb_start_task($taskid, $confirm, $command) - Can not start parallel task, a previous task has been started there \"${t_COMMAND[$taskid]}\""
		rc=$PTB_RET_TASK_RUNNING
	elif [ -z "$confirm" ]; then
		ptb_report_error "ptb_start_task($taskid, $confirm, $command) - Invalid confirm argument."
		rc=$PTB_RET_INVALID_ARGUMENT
	else
		ptb_report_info "ptb_start_task($taskid, $confirm, $command) - Starting task"
		$command &
		local rc=$?
		local pid=$!
		if [ $rc -ne 0 ]; then
			ptb_report_error "ptb_start_task($taskid, $confirm, $command) - Task failed to start with $rc"
			rc=$PTB_RET_FAILED
		else
			T_PID[$taskid]=$pid
			T_COMMAND[$taskid]="$command"
			if [ $confirm -eq 1 ] && [ -z "`ps -p ${T_PID[$taskid]} -o cmd --no-headers`" ]; then
				ptb_report_error "ptb_start_task($taskid, $confirm, $command) - Task failed start confirmation as pid[${pid}]."
				rc=$PTB_RET_FAILED
			else
				ptb_report_info "ptb_start_task($taskid, $confirm, $command) - Task started as pid[${pid}]."
			fi
		fi
	fi
	ptb_report_idebug_leave "ptb_start_task($taskid, $confirm, $command) = $rc"
	return $rc
}
################################################################################
# PUBLIC - kills a task and clears it's command and pid slots
#
# $1 - required, instance id of background task
function ptb_kill_task()
{
	ptb_report_idebug_enter "ptb_kill_task($1)"
	local instanceid=$1
	local rc=$PTB_RET_SUCCESS

	if ! ptb_is_integer "$instanceid"; then
		ptb_report_error "ptb_kill_task($1) - Invalid task instance id specified."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "${T_PID[$instanceid]}" ]; then
		ptb_report_error "ptb_kill_task($1) - Invalid task instance id specified, no instance exists."
		rc=$PTB_RET_INVALID_INSTANCE_ID
	elif [ -z "`ps -p ${T_PID[$instanceid]} -o cmd --no-headers`" ]; then
		ptb_report_error "ptb_kill_task($1) - Can not stop task instance, task appears stopped already."
		rc=$PTB_RET_SERVER_NOT_RUNNING
	else
		ptb_report_info "ptb_kill_task($1) - Killing background task as pid[${T_PID[$instanceid]}]"
		while [ ! -z "`ps -p ${T_PID[$instanceid]} -o cmd --no-headers`" ]; do
			kill -s SIGKILL "${T_PID[$instanceid]}" 2>/dev/null
			sleep 1
		done
		T_PID[$instanceid]=
		T_COMMAND[$instanceid]=
	fi
	ptb_report_idebug_leave "ptb_kill_task($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - kills all running tasks
#
function ptb_kill_all_tasks()
{
	ptb_report_idebug_enter "ptb_kill_all_tasks()"

	local count="${#T_PID[@]}"
	local rc=$PTB_RET_SUCCESS
		
	local instance=
	for (( instance=1; instance < count; instance++ )); do
		if [ -n "${T_PID[$instance]}" ] && [ -n "`ps -p ${T_PID[$instance]} -o cmd --no-headers`" ]; then
			ptb_kill_task $instance
		fi
	done

	ptb_report_idebug_leave "ptb_kill_all_tasks() = $rc"
	return $rc

}


##################################################
# Percona Test Bench statistics manager routines #
##################################################


PTB_STAT_PIPE=
PTB_STAT_PID=
PTB_STAT_TIMEOUT=5
PTB_STAT_SEQUENCE=0
PTB_STAT_RESULT=0

################################################################################
# INTERNAL - waits for result file from statistics_manager for timeout
#
# $1 - required, file to wait for
function ptb_stat_wait_for_result()
{
	local result_file=$1
	local wait_for=$PTB_STAT_TIMEOUT
	while [ ! -r "${result_file}" ] && [ $wait_for -ne 0 ]; do
		sleep 1
		wait_for=`expr $wait_for - 1`
	done
	if [ -r "${result_file}" ]; then
		return 0
	else
		return 1
	fi
	return 0
}
################################################################################
# INTERNAL - waits for rsult file from statistics_manager for timeout
#
# $1 - required, pipe name to send on
# $2 - required, command string to send
# sets response in PTB_STAT_RESULT
function ptb_stat_send_command()
{
	local pipe_name=$1
	local command=$2
	local rc=0

	local result_file="${pipe_name}.${$}.${PTB_STAT_SEQUENCE}.result"
	PTB_STAT_SEQUENCE=`expr $PTB_STAT_SEQUENCE + 1`

	local rpt_prefix="ptb_stat_send_command($pipe_name, $command)"

	ptb_report_idebug_enter "$rpt_prefix"

	ptb_report_info "$rpt_prefix - Sending \"${command}\" to statistics manager."
	echo "${result_file} $command" > $pipe_name
	ptb_stat_wait_for_result ${result_file}
	rc=$?
	if [ $rc -ne 0 ]; then
		ptb_report_error "$rpt_prefix - timed out waiting for response from statistics_manager on file ${result_file}"
		PTB_STAT_RESULT= # Pushing Test Fix for https://github.com/Percona-QA/PTB/issues/12
	else
		PTB_STAT_RESULT=`cat ${result_file}`
		rm -f $result_file
	fi
	ptb_report_idebug_leave "$rpt_prefix = $PTB_STAT_RESULT"
}
################################################################################
# PUBLIC - initializes the statistics manager. This is being done apart from
#          ptb_init as not all tests desire to record statistics and may not
#          need the overhead and extra maintenance that statistics require.
#
# $1 - required, unique pipe name for pipe to statistic_manager job. Usually
#      can be something like /tmp/$$.ptb_stats.in and can be passed down to
#      child jobs so they may write to the statistics manager.
function ptb_init_statistics_manager()
{
	local stat_pipe=$1

	local rpt_prefix="ptb_init_statistics_manager($stat_pipe)"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	local pid=
	local result=
	local create=0
	
	if [ -e "$stat_pipe" ]; then
		if [ ! -p "$stat_pipe" ]; then
			ptb_report_error "$rpt_prefix - pipe name exists but is not an actual pipe."
		fi
	else
		created=1
		ptb_runcmd mkfifo -m 600 "$stat_pipe"
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - command \"mkfifo -m 600 $stat_pipe\" failed with $rc."
		else
			./include/statistic_manager.sh "$stat_pipe" &
			rc=$?
			pid=$!
			
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - unable to start statistics_manager.sh, failed with $rc".
			elif [ -z "`ps -p ${pid} -o cmd --no-headers`" ]; then
				ptb_report_error "$rpt_prefix - unable to start statistics_manager, could not find process with pid ${pid}"
				rc=1
			fi
		fi
	fi
	
	#test it
	ptb_stat_send_command $stat_pipe "ping ${$}"
	if [ $PTB_STAT_RESULT -ne ${$} ]; then
		ptb_report_error "$rpt_prefix - able to ping statistic_manager but got wrong response, expecting \"${$}\" but got \"${rc}\"."
		rc=1
	fi

	if [ $rc -eq 0 ]; then
		PTB_STAT_PIPE=$stat_pipe
		PTB_STAT_PID=$pid # will be empty if we did not create it here
	else
		if [ -n "$pid" ]; then
			ptb_runcmd kill -s SIGTERM $pid
		fi
		if [ $created -ne 0 ]; then
			rm -f $stat_pipe
		fi
	fi

	ptb_report_idebug_leave "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - deinitializes the statistics manager and terminates the job if the
#          pid is known (meaning it was launched from this script instance. It
#          is not necessary to explicitly call this as it woll be called from
#          ptb_cleanup.
#
function ptb_deinit_statistics_manager()
{
	local rpt_prefix="ptb_deinit_statistics_manager()"

	ptb_report_idebug_enter "$rpt_prefix"

	if [ -n "$PTB_STAT_PIPE" ] && [ -n "$PTB_STAT_PID" ]; then
		echo "/dev/null exit 0" > $PTB_STAT_PIPE
		sleep 2
		while [ -n "`ps -p ${PTB_STAT_PID} -o cmd --no-headers`" ]; do
			ptb_runcmd kill -s SIGKILL $PTB_STAT_PID
			sleep 2
		done
		rm -f $PTB_STAT_PIPE
		PTB_STAT_PID=
		PTB_STAT_PIPE=
	fi
	ptb_report_idebug_leave "$rpt_prefix = 0"
	return 0
}
################################################################################
# PUBLIC - touches the statistics manager job to ensure it is properly running
#          and synced
#
function ptb_stat_ping()
{
	local rpt_prefix="ptb_stat_ping()"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	
	if [ -n "$PTB_STAT_PIPE" ]; then
		local command="ping ${$}"
		ptb_stat_send_command $PTB_STAT_PIPE $command
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - ptb_stat_send_command \"$command\" failed with ${rc}."
		elif [ "$PTB_STAT_RESULT" != "${$}" ]; then
			ptb_report_error "$rpt_prefix - able to ping statistic_manager but got wrong response, expecting \"${$}\" but got \"${rc}\"."
			rc=1
		fi
	fi	
	ptb_report_idebug_enter "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - registers a statistic row descriptor with the statistic_manager and
#          returns the row id for later use. If there was an error, returns 0.
#
# $1 - required, row descriptor. Must be a uniqe value that identifies a row,
#      can be any string value or just an incrementing number.
function ptb_stat_register_row()
{
	local row_descriptor=$1

	local rpt_prefix="ptb_stat_register_row($1)"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	
	if [ -z "$row_descriptor" ]; then
		ptb_report_error "$rpt_prefix - invalid row descriptor passed."
		rc=1
	elif [ -n "$PTB_STAT_PIPE" ]; then
		ptb_stat_send_command $PTB_STAT_PIPE "register ${row_descriptor}"
		rc=$?
	fi	
	ptb_report_idebug_enter "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - returns a row id for a given row descriptor. Returns 0 id no matching
#          row is found.
#
# $1 - required, row descriptor.
function ptb_stat_get_row_id()
{
	local row_descriptor=$1

	local rpt_prefix="ptb_stat_get_row_id($1)"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	
	if [ -z "$row_descriptor" ]; then
		ptb_report_error "$rpt_prefix - invalid row descriptor passed."
		rc=0
	elif [ -n "$PTB_STAT_PIPE" ]; then
		ptb_stat_send_command $PTB_STAT_PIPE "get_row_id ${row_descriptor}"
		rc=$?
	fi	
	ptb_report_idebug_enter "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - sets given named cell values for the specified row id and column
#          names. If no column by that name exists, a new one is created.
#
# $1 - required, row id
# $2 - required, column name
# $3 - required, cell value
# $4 - required, column name
# $5 - required, cell value
# $@ and so on...
function ptb_stat_set_cell_value()
{
	local row_id=$1
	shift

	local rpt_prefix="ptb_stat_set_cell_value($1, $@)"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	
	if [ -z "$row_id" ]; then
		ptb_report_error "$rpt_prefix - invalid row id passed."
		rc=0
	elif [ -n "$PTB_STAT_PIPE" ]; then
		local command="set_cell_value $row_id"
		while true; do
			local column_name=$1
			local cell_value=$2
			if [ "$column_name" = "" ]; then
				break
			fi
			command="$command $column_name $cell_value"
			shift 2
		done

		ptb_stat_send_command $PTB_STAT_PIPE "$command"
		rc=$?
	fi	
	ptb_report_idebug_leave "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - sets given named cell values for the specified row descriptor and
#          column names. If no column by that name exists, a new one is created.
#
# $1 - required, row descriptor
# $2 - required, column name
# $3 - required, cell value
# $4 - required, column name
# $5 - required, cell value
# $@ and so on...
function ptb_stat_set_cell_value_by_descriptor()
{
	local row_descriptor=$1
	shift

	local rpt_prefix="ptb_stat_set_cell_value_by_descriptor($1, $@)"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	
	if [ -z "$row_descriptor" ]; then
		ptb_report_error "$rpt_prefix - invalid row descriptor passed."
		rc=0
	elif [ -n "$PTB_STAT_PIPE" ]; then
		local command="set_cell_value_by_descriptor $row_descriptor"
		while true; do
			local column_name=$1
			local cell_value=$2
			if [ "$column_name" = "" ]; then
				break
			fi
			command="$command $column_name $cell_value"
			shift 2
		done

		ptb_stat_send_command $PTB_STAT_PIPE "$command"
		rc=$?
	fi	
	ptb_report_idebug_enter "$rpt_prefix = $rc"
	return $rc
}

################################################################################
# PUBLIC - higher level helper function that takes a set of "a=b" and sets
#          columns of 'a' to cells of 'b' in the statistics manager. Used to log
#          server and other tool options for a specific row.
#
# $1 - required, row id
# $@ - required, a series of 'a=b' or option=value pairs to be parsed with the
#      option name becoming a column name and the value being the cell value for
#      that (row id, column name) location.
function ptb_stat_set_cell_value_from_options()
{
	local row_id=$1
	shift
	
	local rpt_prefix="ptb_stat_set_cell_value_from_options($row_id, $@)"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	
	if [ -z "$row_id" ]; then
		ptb_report_error "$rpt_prefix - invalid row id passed."
		rc=0
	elif [ -n "$PTB_STAT_PIPE" ]; then
		local command="set_cell_value $row_id"
		while true; do
			local full_option=$1
			
			if [ "$full_option" = "" ]; then
				break
			fi

			local opt_name="`expr match \"${full_option}\" '\([a-zA-Z0-9_\-]*\=\)'`"
			local opt_value="${full_option:${#opt_name}}"
			if [ -z "$opt_name" ]; then opt_name="$opt_value"; opt_value=; fi
			opt_name="${opt_name%"="}"

			command="$command $opt_name $opt_value"
			shift 1
		done

		ptb_stat_send_command $PTB_STAT_PIPE "$command"
		rc=$?
	fi	
	ptb_report_idebug_enter "$rpt_prefix = $rc"
	return $rc
}
################################################################################
# PUBLIC - writes current statistics out to the given file, overwriting any
#          existing file.
#
# $1 - required, file to write stats to
function ptb_stat_write_to_file()
{
	local data_file=$1
	local rpt_prefix="ptb_stat_write_to_file($file_name)"

	ptb_report_idebug_enter "$rpt_prefix"

	local rc=0
	
	if [ -z "$data_file" ]; then
		ptb_report_error "$rpt_prefix - invalid data file passed."
		rc=0
	elif [ -n "$PTB_STAT_PIPE" ]; then
		ptb_stat_send_command $PTB_STAT_PIPE "write_statistics $data_file"
		rc=$?
	fi	
	ptb_report_idebug_enter "$rpt_prefix = $rc"
	return $rc
}
####################################
# Percona Test Bench core routines #
####################################

################################################################################
# PUBLIC - initializes the PTB core
# $1 - required, root directory to use for a test cycle. All server and task
#      related data will be stored under this directory.
# $2 - optional, $PTB_RPT_x verbosity filter level
# $3 - optional, output log file
# $4 - optional, pre cleanup function to call at beginning of cleanup during
#      ptb_cleanup
# $5 - optional,  post cleanup function to call at ennd of cleanup during
#      ptb_cleanup
function ptb_init()
{
	ptb_report_idebug_enter "ptb_init($1, $2, $3, $4, $5)"

	PTB_DATADIR=$1
	PTB_RPT_FILTER=$2
	PTB_PRECLEANUP=$4
	PTB_POSTCLEANUP=$5

	local rc=$PTB_RET_SUCCESS

	if [ -z "$PTB_DATADIR" ]; then
		ptb_report_error "ptb_init($1, $2, $3, $4, $5) - No data directory specified."
		exit 1
	fi

	if [ ! -e "$PTB_DATADIR" ]; then
		ptb_runcmd mkdir -p $PTB_DATADIR
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "ptb_init($1, $2, $3, $4, $5) - Unable to create DATADIR $PTB_DATADIR, rc=$rc."
			exit 1
		fi
	fi
	PTB_RPT_FILE=$3
	ptb_init_all_server_instances
	rc=$?

	ptb_report_idebug_leave "ptb_init($1, $2, $3, $4, $5) = $rc"
	return $rc
}
################################################################################
# PUBLIC - cleans up PTB core
# $1 - optional, 1 = stop server and tasks, NULL or 0 = don't
function ptb_cleanup()
{
	ptb_report_idebug_enter "ptb_cleanup($1)"

	local rc=$PTB_RET_SUCCESS

	if [ ! -z "$PTB_PRECLEANUP" ]; then
		$PTB_PRECLEANUP
	fi

	if [ -n "$1" ] && [ $1 -eq 1 ]; then
		ptb_kill_all_tasks
		ptb_stop_all_servers
	fi

	unset S_BINDIR
	unset S_BASEDIR
	unset S_DATADIR
	unset S_TMPDIR
	unset S_PORT
	unset S_SOCKET
	unset S_PIDFILE
	unset S_LOGFILE
	unset S_QUERYLOGFILE
	unset S_DEFAULTSFILE
	unset S_IDFILE
	unset S_OPTIONSFILE
	unset T_COMMAND
	unset T_PID


	if [ ! -z "$PTB_POSTCLEANUP" ]; then
		$PTB_POSTCLEANUP
	fi

	ptb_report_idebug_leave "ptb_cleanup($1) = $rc"
	return $rc
}
################################################################################
# PUBLIC - called by ptb_parse_options to display option names and
#          descriptions but can be called from anywhere.
# $1 - retuired, prefix of option parsed descriptors
function ptb_usage()
{
	local prefix=$1
	if [ -n "$prefix" ]; then
		prefix="${prefix}_"
	fi
	shift 1

	local _names="${prefix}OPT_NAMES"
	local _reqs="${prefix}OPT_REQS"
	local _counts="${prefix}OPT_COUNTS"
	local _types="${prefix}OPT_TYPES"
	local _mins="${prefix}OPT_MINS"
	local _maxs="${prefix}OPT_MAXS"
	local _defs="${prefix}OPT_DEFS"
	local _descs="${prefix}OPT_DESCS"
	local _helps="${prefix}OPT_HELPS"

	local temp=

	local desc_index=0
	local desc_count=0
	temp="desc_count=\${#${_names}[@]}"
	eval ${temp}
	while [ $desc_index -lt $desc_count ]; do
		local desc_helps=
		temp="desc_helps=\${$_helps[${desc_index}]}"
		eval ${temp}
		echo "     $desc_helps"
		desc_index=`expr $desc_index + 1`
	done
}
################################################################################
# PUBLIC - Parses option descriptors list into a series of parallel arrays named
#	   by $1. Reqiires that a global array has been declared prior to being
#	   called in the form of:
#          <arrayname>=("name Description" ... )
#	   "Description" is a string that starts out defining the expected type
#	   and possible limits, followed by a textual description of the option.
#	   Format:
#		"OPT/REQ COUNT TYPE (MIN) (MAX) (DEF) description"
#		OPT - optional | REQ - required
#		COUNT - number of times option may be specified and built up in
#			internal value array, 0 for unlimited.
#		TYPE :
#			INT for signed integer and MUST be followed by
#				MIN - minimum allowable value
#				MAX - maximum allowable value
#				DEF - default value
#			STR for string with no default
#			STRDEF for string with a default and MUST be followed by
#				DEF - default value
#			PATHEXISTS for the name of a directory that must exist
#			PATHCREATE for the name of a directory that must exist
#				or will be created
#			FILEEXISTS for the name of a file that must be readable
#	   So for example:
#		"REQ 1 INT 0 1 0 This value is for..." is a required, integer
#		value with a range of 0-1 and a default of 0 (which if the
#		option is required, default doesn't mean much but it is here
#		anyway to make life a little easier on the guy writing the
#		parser) which may only be specified 1 time.
#
# $1 - required, name of descriptor array
# $2 - optional, name to use as array prefix for the resulting arrays of:
#      OPT_NAMES=	# array of normalized option names
#      OPT_REQS=	# array of option REQ/OPT
#      OPT_COUNTS=	# array of option COUNT
#      OPT_TYPES=	# array of option TYPE
#      OPT_MINS=	# array of option MIN
#      OPT_MAXS=	# array of option MAX
#      OPT_DEFS=	# array of option DEF
#      OPT_DESCS=	# array of option descriptions
#      For example, if "BACKUP" is specified then OPT_NAMES will be
#      BACKUP_OPT_NAMES. If $1 is NULL then the names above will be used.
#      If called multiple times witht he same prefix, parsed descriptors will
#      be added to then of the existing array.
function ptb_parse_option_descriptors()
{
	local rpt_prefix="ptb_parse_option_descriptors($1)"

	ptb_report_idebug_enter "$rpt_prefix"
	
	local descriptor_name=$1
	local prefix=$2
	if [ -n "$prefix" ]; then
		prefix="${prefix}_"
	fi
	shift 2

	local _names="${prefix}OPT_NAMES"
	local _reqs="${prefix}OPT_REQS"
	local _counts="${prefix}OPT_COUNTS"
	local _types="${prefix}OPT_TYPES"
	local _mins="${prefix}OPT_MINS"
	local _maxs="${prefix}OPT_MAXS"
	local _defs="${prefix}OPT_DEFS"
	local _descs="${prefix}OPT_DESCS"
	local _helps="${prefix}OPT_HELPS"

	local temp=
	# first, we walk, validatate and parse the option descriptors
	local option_desc=
	local desc_name=
	local cur_index=
	temp="cur_index=\${#$_names[@]}"
	eval ${temp}
	local descriptors="${descriptor_name}[@]"
	for opt_desc in "${!descriptors}"; do
		# parse the description into its parts
		unset opt_parsed
		local opt_parsed=
		local opt_part=
		local opt_parsed_index=0
		local opt_requirement=
		local opt_default=
		local opt_description=
		for opt_part in $opt_desc; do
			opt_parsed[$opt_parsed_index]=$opt_part
			opt_parsed_index=`expr $opt_parsed_index + 1`
		done
		local desc_name=${opt_parsed[0]}
		local opt_name=$desc_name

		# change any '-' in the descriptor name into '_'
		local pos=`expr index "$opt_name" -`
		while [ $pos -ne 0 ]; do
			opt_name="${opt_name:0:`expr $pos - 1`}_${opt_name:$pos}"
			pos=`expr index "$opt_name" -`
		done
		temp="$_names[$cur_index]=\"$opt_name\""
		eval ${temp}

		# validate the REQ/OPT
		if [ -z "${opt_parsed[1]}" ]; then
			ptb_report_error "$rpt_prefix - Invalid option description, missing REQ/OPT for option $opt_name : $opt_desc"
			exit 1
		elif [ "${opt_parsed[1]}" != "REQ" ] && [ "${opt_parsed[1]}" != "OPT" ]; then
			ptb_report_error "$rpt_prefix - Invalid option description, invalid REQ/OPT value for option $opt_name : $opt_desc. [${opt_parsed[1]}]"
			exit 1
		elif [ "${opt_parsed[1]}" = "REQ" ]; then
			opt_requirement="Required"
		else
			opt_requirement="Optional"
		fi
		temp="$_reqs[$cur_index]=\"${opt_parsed[1]}\""
		eval ${temp}

		# validate the COUNT
		if [ -z "${opt_parsed[2]}" ]; then
			ptb_report_error "$rpt_prefix - Invalid option description, missing COUNT for option $opt_name : $opt_desc"
			exit 1
		fi
		ptb_is_integer ${opt_parsed[2]}
		rc=$?
		if [ $rc -ne 0 ]; then
			ptb_report_error "$rpt_prefix - Invalid option description, invalid COUNT for option $opt_name : $opt_desc. [${opt_parsed[2]}]"
			exit 1
		fi
		temp="$_counts[$cur_index]=${opt_parsed[2]}"
		eval ${temp}

		# validate the TYPE
		if [ -z "${opt_parsed[3]}" ]; then
			ptb_report_error "$rpt_prefix - Invalid option description, missing TYPE for option $opt_name : $opt_desc"
			exit 1
		elif [ "${opt_parsed[3]}" != "INT" ] \
		  && [ "${opt_parsed[3]}" != "STR" ] \
		  && [ "${opt_parsed[3]}" != "STRDEF" ] \
		  && [ "${opt_parsed[3]}" != "PATHEXISTS" ] \
		  && [ "${opt_parsed[3]}" != "PATHCREATE" ] \
		  && [ "${opt_parsed[3]}" != "FILEEXISTS" ]; then
			ptb_report_error "$rpt_prefix - Invalid option description, invalid TYPE value for option $opt_name : $opt_desc. [${opt_parsed[3]}]"
			exit 1
		fi
		temp="$_types[$cur_index]=\"${opt_parsed[3]}\""
		eval ${temp}

		case "${opt_parsed[3]}" in
		"INT" )
			# validate the MIN
			if [ -z "${opt_parsed[4]}" ]; then
				ptb_report_error "$rpt_prefix - Invalid option description, missing MIN for option $opt_name : $opt_desc"
				exit 1
			fi
			ptb_is_integer ${opt_parsed[4]}
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - Invalid option description, invalid MIN for option $opt_name : $opt_desc. [${opt_parsed[4]}]"
				exit 1
			fi
			temp="$_mins[$cur_index]=${opt_parsed[4]}"
			eval ${temp}

			# validate the MAX
			if [ -z "${opt_parsed[5]}" ]; then
				ptb_report_error "$rpt_prefix - Invalid option description, missing MAX for option $opt_name : $opt_desc"
				exit 1
			fi
			ptb_is_integer ${opt_parsed[5]}
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - Invalid option description, invalid MAX for option $opt_name : $opt_desc. [${opt_parsed[5]}]"
				exit 1
			fi
			temp="$_maxs[$cur_index]=${opt_parsed[5]}"
			eval ${temp}

			# validate the DEF
			if [ -z "${opt_parsed[6]}" ]; then
				ptb_report_error "$rpt_prefix - Invalid option description, missing DEF for option $opt_name : $opt_desc"
				exit 1
			fi
			ptb_is_integer ${opt_parsed[6]}
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - Invalid option description, invalid DEF for option $opt_name : $opt_desc. [${opt_parsed[6]}]"
				exit 1
			fi
			temp="$_defs[$cur_index]=${opt_parsed[6]}"
			eval ${temp}
			temp="$_descs[$cur_index]=\"${opt_parsed[@]:7}\""
			eval ${temp}
			opt_default="Default=${opt_parsed[6]}"
			opt_description="${opt_parsed[@]:7}"
		;;
		"STRDEF" ) 
			# validate the DEF
			if [ -z "${opt_parsed[4]}" ]; then
				ptb_report_error "$rpt_prefix - Invalid option description, missing DEF for option $opt_name : $opt_desc"
				exit 1
			fi
			temp="$_defs[$cur_index]=\"${opt_parsed[4]}\""
			eval ${temp}
			temp="$_descs[$cur_index]=\"${opt_parsed[@]:5}\""
			eval ${temp}
			opt_default="Default=${opt_parsed[4]}"
			opt_description="${opt_parsed[@]:5}"
		;;
		* )
			temp="$_descs[$cur_index]=\"${opt_parsed[@]:4}\""
			eval ${temp}
			opt_default=
			opt_description="${opt_parsed[@]:4}"
		;;
		esac

		temp="$_helps[$cur_index]=\"--${desc_name} : ${opt_requirement}, ${opt_description} ${opt_default}\""
		eval ${temp}

		cur_index=`expr $cur_index + 1`
	done

	return 0

	# debugging stuff, ignore it
	local desc_index=0
	local desc_count=0
	temp="desc_count=\${#${_names}[@]}"
	eval ${temp}
	while [ $desc_index -lt $desc_count ]; do
		local desc_names=
		temp="desc_names=\${$_names[${desc_index}]}"
		eval ${temp}
		
		local desc_reqs=
		temp="desc_reqs=\${$_reqs[${desc_index}]}"
		eval ${temp}

		local desc_counts=
		temp="desc_counts=\${$_counts[${desc_index}]}"
		eval ${temp}

		#echo "[$desc_index] $desc_names $desc_reqs $desc_count"
		desc_index=`expr $desc_index + 1`
	done

	return $rc
}
################################################################################
# PUBLIC - parses options list in the form of --name=value or --name. Reqiires
#          that a glbal array has been declared prior to being called in the
#          form of:
#          declare -A PTB_OPTION_DESCRIPTORS=([name]="Description" ... )
#          For each found option, a global variable will be set as ${prefix}_OPT_name
#          to the parsed value. If no value was specified the variable will be
#          set to " ".
#	   "Description" is a string that starts out defining the expected type
#	   and possible limits, followed by a textual description of the option.
#	   Format:
#		"OPT/REQ COUNT TYPE (MIN) (MAX) (DEF) description"
#		OPT - optional | REQ - required
#		COUNT - number of times option may be specified and built up in
#			internal value array, 0 for unlimited.
#		TYPE :
#			INT for signed integer and MUST be followed by
#				MIN - minimum allowable value
#				MAX - maximum allowable value
#				DEF - default value
#			STR for string with no default
#			STRDEF for string with a default and MUST be followed by
#				DEF - default value
#			PATHEXISTS for the name of a directory that must exist
#			PATHCREATE for the name of a directory that must exist
#				or will be created
#			FILEEXISTS for the name of a file that must be readable
#	   So for example:
#		"REQ 1 INT 0 1 0 This value is for..." is a required, integer
#		value with a range of 0-1 and a default of 0 (which if the
#		option is required, default doesn't mean much but it is here
#		anyway to make life a little easier on the guy writing the
#		parser) which may only be specified 1 time.
#
# $1 - required, function to call for each option for validation. This function
#      will be given the raw option name as $1 (no leading --) and will be given 
#      the option value if present as $2. This function will be called before
#      the ${prefix}_OPT_${name} global is set for each option passed in $@.
#      This function should return:
#      0 to indicate that value looks good and ptb_parse_options should set it.
#      1 to indicate that the validation function has handled setting the value.
#      2 to indicate that the value is invalid and usage/error should be handled.
# $2 - required, function to call to display basic usage prelude without option
#      descriptions
# $3 - required, option descriptor prefix, may be "" for default options
# $@ - options list to parse 
function ptb_parse_options()
{
	local fn_getoption=$1
	local fn_usage=$2
	local raw_prefix=$3
	shift 3

	local rpt_prefix="ptb_parse_options($fn_getoption, $fn_usage, $raw_prefix, $@)"
	if [ -n "$raw_prefix" ]; then
		prefix="${raw_prefix}_"
	fi
	local _names="${prefix}OPT_NAMES"
	local _reqs="${prefix}OPT_REQS"
	local _counts="${prefix}OPT_COUNTS"
	local _types="${prefix}OPT_TYPES"
	local _mins="${prefix}OPT_MINS"
	local _maxs="${prefix}OPT_MAXS"
	local _defs="${prefix}OPT_DEFS"
	local _descs="${prefix}OPT_DESCS"
	local temp=

	ptb_report_idebug_enter "$rpt_prefix"

	local fullvalue=
	for opt in "$@"; do
		# check to see if the option is well formed with a leading "--"
		if [ "${opt:0:2}" != "--" ]; then
			ptb_report_error "$rpt_prefix - Invalid option specification syntax at $opt [$clean]"
			$fn_usage
		fi

		opt="${opt:2}"

		# parse the option into its name and value
		local opt_name="`expr match \"${opt}\" '\([a-zA-Z0-9_\-]*\=\)'`"
		local opt_value="${opt:${#opt_name}}"
		if [ -z "$opt_name" ]; then opt_name="$opt_value"; opt_value=; fi
		opt_name="${opt_name%"="}"

		# change any '-' in the name into '='
		opt_name=${opt_name//-/_}

		# break out if they are only looking for help
		if [ "$opt_name" = "help" ] || [ "$opt_name" = "usage" ] || [ "$opt_name" = "?" ]; then
			$fn_usage
		# incorporate defaults-file and send each line down through this parser
		elif [ "$opt_name" = "defaults_file" ]; then
			ptb_report_info "Reading options from $opt_value"
			local line=
			while read line; do
				if [ -n "$line" ] && [ "${line:0:1}" != "#" ]; then
					ptb_parse_options $fn_getoption $fn_usage "$raw_prefix" "--${line}"
				fi
			done < ${opt_value}
			continue
		fi

		# call the validation function, passing the option name and value
		$fn_getoption $opt_name $opt_value
		case $? in
		0 )
			# validate below and set the global value
			;;
		1 )
			# validation fn handled the setting, do nothing
			continue
			;;
		* )
			# error
			$fn_usage
			;;
		esac

		# look up the name in the option descriptors
		local desc_index=0
		local desc_count=0
		temp="desc_count=\${#${_names}[@]}"
		eval ${temp}
		while [ $desc_index -lt ${desc_count} ]; do
			local desc_names=
			temp="desc_names=\${${_names}[${desc_index}]}"
			eval ${temp}
			if [ "$desc_names" = "$opt_name" ]; then
				break
			fi
			desc_index=`expr $desc_index + 1`
		done
		if [ $desc_index -ge $desc_count ]; then
			ptb_report_error "$rpt_prefix - Unknown option specification $opt_name"
			$fn_usage
		fi
		
		# attempt to validate COUNT
		temp="desc_counts=\${${_counts}[$desc_index]}"
		eval ${temp}
		if [ $desc_counts -gt 0 ]; then
			local opt_count=
			eval "opt_count=\${#${prefix}OPT_${opt_name}[@]}"
			if [ $opt_count -ge $desc_counts ]; then
				ptb_report_error "$rpt_prefix - $opt_name is specified more than the allowable number of times."
				$fn_usage
			fi
		fi

		# attempt to validate TYPE
		local desc_types=
		temp="desc_types=\${#${_types}[$desc_index]}"
		eval ${temp}
		case "$desc_types" in
		"INT" )
			# validate INT ranges
			local desc_mins=
			temp="desc_mins=\${#${_mins}[$desc_index]}"
			eval ${temp}

			local desc_maxs=
			temp="desc_maxs=\${#${_maxs}[$desc_index]}"
			eval ${temp}

			ptb_is_integer $opt_value
			rc=$?
			if [ $rc -ne 0 ]; then
				ptb_report_error "$rpt_prefix - $opt_name must be a valid integer value between ${desc_mins} and ${desc_maxs}. [$opt_value]"
				$fn_usage
			fi

			if [ $opt_value -lt $desc_mins ] || [ $opt_value -gt $desc_maxs ]; then
				ptb_report_error "$rpt_prefix - $opt_name must be a valid integer value between ${desc_mins} and ${desc_maxs}. [$opt_value]"
				$fn_usage
			fi
		;;
		"PATHEXISTS" )
			if [ ! -d "$opt_value" ]; then
				ptb_report_error "$rpt_prefix - $opt_name must refer to a valid, accessible directory. [$opt_value]"
				$fn_usage
			fi
		;;
		"PATHCREATE" )
			if [ ! -d "$opt_value" ]; then
				ptb_runcmd mkdir -p $opt_value
				rc=$?
				if [ $rc -ne 0 ]; then
					ptb_report_error "$rpt_prefix - $opt_name must refer to a valid, accessible directory, failed to create with $rc. [$opt_value]"
					$fn_usage
				fi
			fi
		;;
		"FILEEXISTS" )
			if [ ! -r "$opt_value" ]; then
				ptb_report_error "$rpt_prefix - $opt_name must refer to a valid, readable file. [$opt_value]"
				$fn_usage
			fi
		;;
		esac

		local temp="${prefix}OPT_${opt_name}[\${#${prefix}OPT_${opt_name}[@]}]=\"${opt_value}\""
		eval "${temp}"
	done

	ptb_report_idebug_leave "$rpt_prefix = 0"
}
################################################################################
# PUBLIC - Validates all parsed and set options. Reqiires that option 
#	   descriptors have been parsed and named.
#
# $1 - required, function to call to display basic usage prelude without option
#      descriptions
# $2 - required, option descriptor prefix, may be "" for default options
function ptb_validate_options()
{
	local fn_usage=$1
	local prefix=$2
	shift 2

	local rpt_prefix="ptb_validate_options($fn_usage, $prefix)"

	if [ -n "$prefix" ]; then
		prefix="${prefix}_"
	fi
	local _names="${prefix}OPT_NAMES"
	local _reqs="${prefix}OPT_REQS"
	local _counts="${prefix}OPT_COUNTS"
	local _types="${prefix}OPT_TYPES"
	local _mins="${prefix}OPT_MINS"
	local _maxs="${prefix}OPT_MAXS"
	local _defs="${prefix}OPT_DEFS"
	local _descs="${prefix}OPT_DESCS"
	local temp=

	ptb_report_idebug_enter "$rpt_prefix"

	# walk the option descriptors and see if all required options have
	# been given and set defaults for those optionals not given, obviously 
	# default values will not be in use for types such as PATHCREATE, etc
	# above.
	local desc_index=0
	local desc_count=0
	local temp="desc_count=\${#${_names}[@]}"
	eval ${temp}
	while [ $desc_index -lt $desc_count ]; do
		local desc_names=
		temp="desc_names=\${$_names[${desc_index}]}"
		eval ${temp}
		
		local desc_reqs=
		temp="desc_reqs=\${$_reqs[${desc_index}]}"
		eval ${temp}

		local desc_types=
		temp="desc_types=\${$_types[${desc_index}]}"
		eval ${temp}

		local opt_count=
		temp="opt_count=\${#${prefix}OPT_${desc_names}[@]}"
		eval ${temp}
		if [ "$desc_reqs" = "REQ" ]; then
			if [ $opt_count -eq 0 ]; then
				ptb_report_error "$rpt_prefix - Required option $desc_names missing."
				$fn_usage
			fi 
		elif [ "$desc_types" = "INT" ]; then
			if [ $opt_count -eq 0 ]; then
				temp="${prefix}OPT_${desc_names}=\${${_defs}[${desc_index}]}"
				eval "${temp}"
			fi 
		fi
		desc_index=`expr $desc_index + 1`
	done

	ptb_report_idebug_leave "$rpt_prefix = 0"
}
################################################################################
# DEBUG - Used to display currently set options based on PTB_OPTION_DESCRIPTORS
# $1 - required, prefix of option parsed descriptors
function ptb_show_option_values()
{
	local prefix=$1
	if [ -n "$prefix" ]; then
		prefix="${prefix}_"
	fi
	shift 1

	local _names="${prefix}OPT_NAMES"
	local _reqs="${prefix}OPT_REQS"
	local _counts="${prefix}OPT_COUNTS"
	local _types="${prefix}OPT_TYPES"
	local _mins="${prefix}OPT_MINS"
	local _maxs="${prefix}OPT_MAXS"
	local _defs="${prefix}OPT_DEFS"
	local _descs="${prefix}OPT_DESCS"

	local temp=

	local desc_index=0
	local desc_count=0
	temp="desc_count=\${#${_names}[@]}"
	eval ${temp}
	while [ $desc_index -lt $desc_count ]; do
		local desc_names=
		temp="desc_names=\${$_names[${desc_index}]}"
		eval ${temp}
		local opt_count=0
		temp="opt_count=\${#${prefix}OPT_${desc_names}[@]}"
		eval ${temp}
		local i=0
		while [ $i -lt $opt_count ]; do
			local opt_value=
			temp="opt_value=\${${prefix}OPT_${desc_names}[$i]}"
			eval ${temp}
			echo "$temp   ${desc_names}[${i}]=${opt_value}"
			i=`expr $i + 1`
		done
		desc_index=`expr ${desc_index} + 1`
	done
}
################################################################################
# PUBLIC - catches termination signals and cleans up PTB core
function ptb_on_die()
{
	ptb_report_idebug_enter "ptb_cleanup()"
	ptb_cleanup 1
	ptb_report_idebug_leave "ptb_cleanup()"
	exit 1
}
trap ptb_on_die SIGINT SIGTERM
