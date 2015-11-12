Design Consideration:
+ All the requirements in the task are implemented.

Implementation Details:
+ Important Struct and Functions
|-- struct process: keep arguments and its next process in the pipeline
|-- struct job: keep the first process in this job, and the input, the output, and whether it is a background job
|-- function childHandler: Used by signal(SIGCHLD, childHandler), wait for background processes
|-- function cd: deal with cd command
|-- function get_tokens: get tokens from input line
|-- function analyze: parse the tokens of the input line
|-- function launch_job: execute a job
+ Core Function Details
|--+ main
|  |-- set SIGCHLD handler
|  |-- construct signal set which contains only SIGCHLD
|  |--+ infinite loop
|     |-- show prompt
|     |-- get input
|     |-- parse
|     |-- launch job
|--+ launch_job
   |-- redirect input
   |--+ loop through all the processes in the job
      |-- redirect output
      |-- block SIGCHLD
      |--+ fork
      |  |--+ child process
      |  |  |-- dup input
      |  |  |-- dup output
      |  |  |-- execute
      |  |  |-- exit
      |  |--+ parent process
      |     |-- redirect input to pipe
      |     |-- waitpid
      |-- unblock SIGCHLD

Limitations:
+ When executing a process, this shell does not support the Ctrl-C interrupt
+ Not all kinds of invalid input are perfectly detected
+ When exit the shell, it does not check whether there are unterminated processes
