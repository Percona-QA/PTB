# Important options

xtrabackup_incremental_backup.sh
------------------------------------------------------------

```
./include/xtrabackup_incremental_backup.sh --help
XtraBackup - incremental backup test
Options:
     --server-id : Required, Server ID to backup from. Default=1
     --vardir : Required, Directory where individual test and data results should be located. 
     --statistics-manager : Optional, Name of pipe to communicate with statistics manager. 
     --pidfile : Optional, Name of pidfile. 
     --verbosity : Optional, Output verbosity filter: ERROR=4; WARNING=3; INFO=2; DEBUG=1; IDEBUG=0. Default=2
     --backup-rootdir : Optional, Directory where the XtraBackup binaries are located. 
     --backup-logfile : Optional, Log file name for backup operation. 
     --backup-option : Optional, Extra backup test options. 
     --backup-command-option : Optional, Extra backup command options. 
     --server-option : Optional, Server options. 
--backup-option options:
     --cycle-count : Optional, Number of full backups to perform. Default=1
     --cycle-delay : Optional, Amount of time (in seconds) to wait in before each full backup. Default=0
     --dev-null : Optional, Force backup output to /dev/null with xbstream format. No restore validation is possible. Default=0
     --drop-caches : Optional, Force linux to flush file caches before each backup. Default=0
     --incremental-count : Optional, Number of incremental backups to perform after each full backup. Can not be used with test-incremental-schedule. Default=0
     --incremental-delay : Optional, Amount of time (in seconds) to wait before performing each subsequent incremental backups. Can not be used with test-incremental-schedule. Default=0
     --incremental-schedule : Optional, Schedule of time delays to perform incremental backups after each full backup delimited by ':'. Use in place of test-incremental-count and test-incremental-wait. Ex: A value of 0:20:60:120 will perform the first incremental 0 seconds after completing a full backup, the second incremental will be performed 20 seconds after completion of the previous incremental, etc... A randomized schedule can be generated for each invocation by using the format RND:mincycles:maxcycles:mindelay:maxdelay 
     --dryrun : Optional, Parse options and report as if running tests but do not actually execute any test. Default=0
```

xtrabackup_restore_and_validate.sh
--------------------------------------------------------------

```
./include/xtrabackup_restore_and_validate.sh --help
XtraBackup - restore validation
Options:
     --server-id : Required, Server ID to restore to. Default=1
     --vardir : Required, Directory where individual test and data results should be located. 
     --statistics-manager : Optional, Name of pipe to communicate with statistics manager. 
     --pidfile : Optional, Name of pidfile. 
     --verbosity : Optional, Output verbosity filter: ERROR=4; WARNING=3; INFO=2; DEBUG=1; IDEBUG=0. Default=2
     --restore-rootdir : Optional, Directory where the XtraBackup binaries are located. 
     --restore-logfile : Optional, Log file name for backup operation. 
     --restore-option : Optional, Extra restore options. 
     --backup-command-option : Optional, Extra backup command options.
```
