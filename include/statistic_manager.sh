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


################################################################################
# statistic_manager.sh
#
# Statistics are maintained in a series of global arrays named col_<name>.
#
# col_descriptors is the array of row descriptors with their array index being
# the row id, row 0 is a string/array of column names, all rows start numbering
# at 1.
#
# Ideal usage is to first create a fifo pipe from the controlling application:
#     mkfifo -m 600 /tmp/$$.stats.in
# Then launch this script as such:
#    ./include/statistic_manager.sh /tmp/$$.stats.in &
# Then write commands from any child script, through the pipe and into the 
# statstic_manager:
#    echo "register test-0.cycle-0.inc-0" > /tmp/$$.stats.in
#    echo "set_cell_value_by_descriptor test-0.cycle-0.inc-0 backuptime $backup_time" > /tmp/$$.stats.in
#    echo "set_cell_value_by_descriptor test-0.cycle-0.inc-0 restoretime $restore_time" > /tmp/$$.stats.in
#    echo "write_statistics $result_file" > /tmp/$$.stats.in
#    echo "exit 0" > /tmp/$$.stats.in
#
# Then make sure to remove the pipe:
#    rm -f /tmp/$$.stats.in
################################################################################
col_descriptor[0]="descriptor "
result=

################################################################################
# PUBLIC - registers a statistic row descriptor and returns the row id
#
# $1 - required, row descriptor. Must be a uniqe value that identifies a row,
#      can be any string value or just an incrementing number. If "" is passed,
#      function will use the next row id as the descriptor and return that.
function register()
{
	local row_descriptor=$1
	result=0
	if [ -n "$row_descriptor" ]; then
		get_row_id $row_descriptor
	fi
	if [ $result -eq 0 ]; then
		result=${#col_descriptor[@]}

		if [ "$row_descriptor" = "" ]; then
			row_descriptor="$result"
		fi
		col_descriptor[$result]=$row_descriptor
	fi
}
################################################################################
# PUBLIC - returns a row id for a given row descriptor. Returns 0 id no matching
#          row is found.
#
# $1 - required, row descriptor.
function get_row_id()
{
	local find_descriptor=$1
	local row_descriptor=
	for row_descriptor in "${col_descriptor[@]}"; do
		if [ "$row_descriptor" = "$find_descriptor" ]; then
			break
		else
			result=`expr $result + 1`
		fi
	done
	if [ $result -ge ${#col_descriptor[@]} ]; then
		result=0
	fi
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
function set_cell_value()
{
	local row_id=$1
	shift 1

	while true; do
		local find_name=$1
		local value=$2
		if [ "$find_name" = "" ]; then
			break
		fi

		# strip any leading '--'
		if [ "${find_name:0:2}" = "--" ]; then
			find_name=${find_name:2}
		fi

		# change any '-' in the name into '_'
		local pos=`expr index "$find_name" -`
		while [ $pos -ne 0 ]; do
			find_name="${find_name:0:`expr $pos - 1`}_${find_name:$pos}"
			pos=`expr index "$find_name" -`
		done

		local column_names="${col_descriptor[0]}"
		local column_name=

		local found=0
		for column_name in ${column_names}; do
			if [ "$column_name" = "$find_name" ]; then
				found=1
				break
			fi
		done
		if [ $found -eq 0 ]; then
			col_descriptor[0]="${col_descriptor[0]} $find_name"
			column_name="$find_name"
		fi

		temp="col_${column_name}[${row_id}]=\"$value\""
		eval ${temp}	
	
		shift 2
	done
	result=0
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
function set_cell_value_by_descriptor()
{
	local descriptor=$1
	shift 1

	get_row_id $descriptor
	local row_id=$result
	if [ $row_id -eq 0 ]; then
		register $descriptor
		row_id=$result
	fi
	if [ $row_id -eq 0 ]; then
		result=0
	else
		set_cell_value $row_id $@
	fi
}
################################################################################
# PUBLIC - writes current statistics out to the given file, overwriting any
#          existing file.
#
# $1 - required, file name
function write_statistics()
{
	local file_name=$1
	local column_names="${col_descriptor[0]}"
	
	rm -f $file_name

	# first, show the header
	local column_name=
	for column_name in ${column_names}; do
		echo -n "${column_name}, " >> $file_name
	done
	echo "" >> $file_name

	# next, walk the rows
	local row_id=1
	while [ $row_id -lt ${#col_descriptor[@]} ]; do
		for column_name in ${column_names}; do
			local value=
			local temp="value=\${col_${column_name}[${row_id}]}"
			eval ${temp}
			echo -n "${value}, " >> $file_name
		done
		echo "" >> $file_name
		row_id=`expr $row_id + 1`
	done
	result=0
}
################################################################################
# PUBLIC - echoes/returns data given
#
# $1 - required, data to return.
function ping()
{
	result=$1
}
function runit()
{
	local result_file=$1
	local command=$2
	shift 2
	$command $@
	if [ -n "$result_file" ]; then
		echo "$result" > $result_file
	fi
}
function on_die()
{
	rm -f $stat_pipe
	exit 1
}
trap on_die 1 2 3 9 15
stat_pipe=$1

line=
while true; do
	read line <$stat_pipe
	runit $line
done
rm -f $stat_pipe
