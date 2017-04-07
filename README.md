# Small-Shell
This custom shell will run command line instructions and return the results similar to other shells, but without many of their fancier features.

Utilizes process management, zombie reaping, signals and UNIX I/O.

The general syntax of a command line is:
command [arg1 arg2 ...] [< input_file] [> output_file] [&]
…where items in square brackets are optional.

*****************************************************************************************
Built in commands:

exit: The exit command exits the shell. It takes no arguments. When this command is run, the shell kills any other processes or jobs that it has started before it terminates itself.

cd: The cd command changes directories.  By itself, it changes to the directory specified in the HOME environment variable (not to the location where smallsh was executed from, unless the shell is located in the HOME directory).  It can also take one argument, the path of the directory to change to. Note that this is a working directory: when smallsh exits, the pwd will be the original pwd when smallsh was launched. The cd command supports both absolute and relative paths.

status: The status command prints out either the exit status or the terminating signal of the last foreground process (not both, processes killed by signals do not have exit statuses!).

*****************************************************************************************
Signals

A CTRL-C command from the keyboard will send a SIGINT signal to the parent process and all children at the same time. SIGINT does not terminate the shell, but only terminates the foreground command if one is running.

If a child foreground process is killed by a signal, the parent immediately prints out the number of the signal that killed it's foreground child process (see the example) before prompting the user for the next command.

Background processes are not terminated by a SIGINT signal. They will terminate themselves, continue running, or be terminated when the shell exits (see below).

A CTRL-Z command from the keyboard will send a SIGTSTP signal to the shell. When this signal is received, the shell will display an informative message (see below) and then enter a state where new commands can no longer be run in the background. In this state, the & operator is simply ignored - all commands are run as if they were foreground processes. If the user sends SIGTSTP again, another informative message is displayed (see below), then the shell returns back to the normal condition where the & operator is once again honored, allowing commands to be placed in the background.

*****************************************************************************************
Example run:

$ smallsh
: ls
junk   smallsh    smallsh.c
: ls > junk
: status
exit value 0
: cat junk
junk
smallsh
smallsh.c
: wc < junk > junk2
: wc < junk
       3       3      23
: test -f badfile
: status
exit value 1
: wc < badfile
cannot open badfile for input
: status
exit value 1
: badfile
badfile: no such file or directory
: sleep 5
^Cterminated by signal 2
: status &
terminated by signal 2
: sleep 15 &
background pid is 4923
: ps
   PID TTY      TIME CMD
  4923 pts/4    0:00 sleep
  4564 pts/4    0:03 tcsh-6.0
  4867 pts/4    1:32 smallsh
: 
: # that was a blank command line, this is a comment line
:
background pid 4923 is done: exit value 0
: # the background sleep finally finished
: sleep 30 &
background pid is 4941
: kill -15 4941
background pid 4941 is done: terminated by signal 15
: pwd
/nfs/stak/faculty/b/brewsteb/CS344/prog3
: cd
: pwd
/nfs/stak/faculty/b/brewsteb
: cd CS344
: pwd
/nfs/stak/faculty/b/brewsteb/CS344
: echo 5755
5755
: echo $$
5755
: ^C
: ^Z
Entering foreground-only mode (& is now ignored)
: date
 Mon Jan  2 11:24:33 PST 2017
: sleep 5 &
: date
 Mon Jan  2 11:24:38 PST 2017
: ^Z
Exiting foreground-only mode
: date
 Mon Jan  2 11:24:39 PST 2017
: sleep 5 &
background pid is 4963
: date
 Mon Jan  2 11:24:39 PST 2017
: exit
$
