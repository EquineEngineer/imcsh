# IMCSH V0.1
## Authors: Antonino Rossi, Mykhailo Neroznak

## Developed for the OS Course Semester III, IMC FH Krems

### IMCSH is a shell-proxy which currently defines 3 commands:

* ```globalusage```: Displays the available commands and use mode.
* ```exec```: Executes any string after it as a shell command. You may use two operators with this command: `> <filename>` redirects the program output to `filename` instead of stdout, `&` used at the end of a command will execute the process in background and return control immediately.
* ```quit```: Terminates the shell execution. If any background processes are still running, the user will be prompted to either kill all remaining background processes or to abort the quit operation.

### Build the project

#### Build release (compile and execute)
```bash
make
```

#### Build debug (compile and execute with debug information)
```
make debug
```

#### Compile the project
```
make build
```

#### Run the project
```
make run
```

#### Build folder structure
```
make dirs
```

#### Clear object files and executables
```
make clean
```

## Library Documentation
### Structs
* `args_t`: holds the command arguments and the number of arguments.
* `command_t`: holds the name of the command, the commands passed to it, whether it should be executed in background and whether it redirects its result away from stdout.
* `process_t`: process wrap utility, holds the id of a running process and its place in the running process queue.

### Functions
(Utility functions omitted for brevity's sake)

* `void read_command(char *, command_t *)`: reads a string from stdin, loads it into a buffer and parses it as a `command_t`.
* `void execute_command(command_t *, process_t **)`: takes a command and a list of processes, tries to allocate such command to the list of processes (can fail if too many processes are running at the same time) and executes it.
* `args_t *parse_args(char *)`: Reads a string from stdin and parses it into an `args_t`.
* `int add_process(pid_t, process_t **)`: Adds a process with the given id to the list of running processes.
* `void terminate_processes(process_t **)`: Kills all running child processes and deallocates them.
