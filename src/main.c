#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/cdefs.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>

// Allowed commands
#define CMD_EXEC "exec"
#define CMD_GLOBALUSAGE "globalusage"
#define CMD_QUIT "quit"

// Allowed modifiers
#define MODIFIER_BACKGROUND "&"
#define MODIFIER_OUTPUT ">"

// Allowed input/output
#define STDIN 0
#define STDOUT 1

// Allowed buffer sizes
#define BUFFER_SIZE 1024
#define MAX_ARGS 64

// Allowed max process count
#define MAX_PROCESSES 8

// Allowed exit codes
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

// Command arguments
//
// Contains:
//  - Arguments
//  - Size
typedef struct args_t
{
    char *args[MAX_ARGS];
    size_t size;
} args_t;

// A IMCSH command.
//
// Contains:
//  - Name
//  - Arguments
//  - Whether it should be executed in the background
//  - Output file
typedef struct command_t
{
    char *name;
    args_t *args;
    bool background;
    char *output_f;
} command_t;

/**
 *
 * A process.
 *
 * Contains:
 * - PID
 * - Position in the process array
 */
typedef struct process
{
    pid_t pid;
    int position;
} process_t;

// Function prototypes
inline void print_usage();
inline void print_prompt();
inline void read_command(char *, command_t *);
inline void execute_command(command_t *, process_t **);
inline void free_command(command_t *);
inline void pass(void *);
inline char *pparray(char **, size_t);
inline void ppargs(args_t *);
inline void display_command(command_t *);
inline bool redirects_output(args_t *);
inline bool is_background(args_t *);
inline void read_into(char *);
inline args_t *parse_args(char *);
inline args_t *crop_args(args_t *);
inline int add_process(pid_t, process_t **);
inline void terminate_processes(process_t **);

int main(int argc, char *argv[])
{
    // Print usage
    print_usage();

    // Command
    command_t command;

    // Buffer
    char buf[BUFFER_SIZE];

    // Processes
    /// Where's C23 when you need it?
    process_t *processes[MAX_PROCESSES] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

    // Main app
    while (1)
    {
        // Read command
        read_command(buf, &command);

        // Execute command
        execute_command(&command, processes);

        // Free command
        free_command(&command);
    }

    // Exit
    return EXIT_SUCCESS;
}

/**
 *
 * Prints usage.
 *
 */
inline void print_usage()
{
    printf("IMCSH Version 1.1 created by Antonino Rossi @IMC, Mykhailo Neroznak @IMC.\n");
}

/**
 *
 * Prints prompt.
 *
 */
inline void print_prompt()
{
    // Get username from linux
    char *username = getenv("USER");

    // Get hostname from linux
    struct utsname *name = malloc(sizeof(struct utsname));
    int machine_name = uname(name);
    pass(&machine_name);

    // Print prompt
    printf("%s@%s> ", username, name->nodename);
    fflush(NULL);

    // Free
    free(name);
}

/**
 *
 * Reads command.
 *
 * @param buffer Buffer to read into
 * @param command Command
 */
inline void read_command(char *buffer, command_t *command)
{
    // Reset buffer
    memset(buffer, 0, BUFFER_SIZE);

    // Print console prompt prefix
    print_prompt();

    // Read line from stdin into buffer
    read_into(buffer);

    // Tokenize command
    char *token = strtok(buffer, " ");

    // Extract command name
    command->name = token;

    // Parse arguments
    args_t *args = parse_args(buffer);

    // Check if the command has to be executed in background
    if (is_background(args))
    {
        command->background = true;
        args->size--;
    }
    else
    {
        command->background = false;
    }

    // Check if the command has to redirect output
    if (redirects_output(args))
    {
        command->output_f = args->args[args->size - 1];
        args->size -= 2;
    }
    else
    {
        command->output_f = NULL;
    }

    // Finalise initialisation
    command->args = args;
}

/**
 *
 * Executes command.
 *
 * @param command The command to execute.
 */
inline void execute_command(command_t *command, process_t **processes)
{
    // Check for quit
    if (strcmp(command->name, CMD_QUIT) == 0)
    {
        // Quit only if no arguments are passed
        if (command->args->size > 0)
        {
            printf("Invalid arguments for `quit` command.\n");
            printf("Usage: quit\n");
        }
        else
        {
            // Allocate memory for input
            char *input = malloc(sizeof(char) * BUFFER_SIZE);
            // Check if any process is running
            for (int i = 0; i < MAX_PROCESSES; i++)
            {
                if (processes[i] != NULL)
                {
                    printf("There are still running processes. Would you like to kill them? (y/N): ");
                    fflush(NULL);
                    read_into(input);

                    if (strcmp(input, "y") == 0)
                    {
                        terminate_processes(processes);

                        // Free input
                        free(input);
                        printf("All processes terminated.\n");
                        exit(EXIT_SUCCESS);
                    }
                    else
                    {
                        // Free input
                        free(input);
                        printf("Aborted.\n");
                        return;
                    }

                    break;
                }
            }
        }
    }

    // Check for globalusage
    else if (strcmp(command->name, CMD_GLOBALUSAGE) == 0)
    {
        // GLOBALUSAGE only accepts 0 arguments
        if (command->args->size > 0)
        {
            printf("Invalid arguments for `globalusage` command.\n");
            printf("Usage: globalusage\n");
        }
        else
        {
            print_usage();
        }
    }

    // Check for exec
    else if (strcmp(command->name, CMD_EXEC) == 0)
    {
        // EXEC only accepts 1+ arguments
        if (command->args->size < 1)
        {
            printf("Invalid arguments for `exec` command.\n");
            printf("Usage: exec <command> [args] [> <file>] [&]\n");
        }
        else
        {
            display_command(command);

            // Fork
            pid_t pid = fork();

            // Check for error
            if (pid == -1)
            {
                printf("Error forking.\n");
                return;
            }

            // Check if child
            if (pid == 0)
            {
                // Check if output should be redirected
                if (command->output_f != NULL)
                {
                    // Open file
                    FILE *f = fopen(command->output_f, "a");

                    // Check for error
                    if (f == NULL)
                    {
                        printf("Error opening file.\n");
                        return;
                    }

                    // Redirect output
                    dup2(fileno(f), STDOUT_FILENO);
                }

                // Crop function arguments to account for flags / background / redirection
                char **to_execute = crop_args(command->args)->args;

                // use execvp to execute the command
                int status = execvp(to_execute[0], to_execute);

                // Check for error
                if (status == -1)
                {
                    printf("Error executing command.\n");
                }
                else
                {
                    printf("Process %d exited with status %d.\n", getpid(), status);
                    fflush(NULL);
                }
            }

            // Check if parent
            else
            {
                // add pid to running processes
                int pidx = add_process(pid, processes);

                // Check for error
                if (pidx == -1)
                {
                    printf("Error adding process.\n");
                    return;
                }
                else
                {
                    printf("Process %d added to running processes.\n", pid);
                }

                // Check if command should be executed in background
                if (!command->background)
                {
                    fflush(NULL);
                    // Wait for child
                    int status;
                    waitpid(pid, &status, 0);
                    printf("Process %d exited with status %d.\n", pid, status);
                }
            }
        }
    }

    else
    {
        printf("Command not found.\n");
        print_usage();
    }

    printf("\n");
}

/**
 *
 * Frees command.
 *
 * @param command Command to free.
 */
inline void free_command(command_t *command)
{
    free(command->args);

    // Reset command
    command->name = NULL;
    command->args = NULL;
    command->background = false;
    command->output_f = NULL;
}

inline void pass(void *t) {}

/**
 *
 * Takes an array of strings and returns a string representation.
 *
 * @param array Array of strings.
 * @param size Size of array.
 */
inline char *pparray(char **array, size_t size)
{
    // Reset buffer
    char *buffer = (char *)malloc(BUFFER_SIZE);

    // Loop
    for (int i = 0; i < size; i++)
    {
        // Check for null
        if (array[i] == NULL)
        {
            break;
        }

        // Append to buffer
        strcat(buffer, array[i]);
        strcat(buffer, ", ");
    }

    // Return buffer
    return buffer;
}

/**
 *
 * Pretty-prints arguments.
 *
 * @param args Arguments.
 */
inline void ppargs(args_t *args)
{
    char *buffer = pparray(args->args, args->size);
    printf("args: (%s)\n", buffer);
    free(buffer);
    printf("\n");
}

/**
 *
 * Displays command information.
 * (Works only if DEBUG is defined, run `make debug` to enable)
 *
 * @param command Command.
 */
inline void display_command(command_t *command)
{
#ifdef DEBUG
    printf("\nExecuting: %s...\n", command->name);
    printf("%s\n", command->background ? "The task will be executed in the background" : "The task will be executed in main thread");
    printf("Redirecting output to: %s\n", command->output_f ? command->output_f : "stdout");
    ppargs(command->args);
#endif
}

/**
 *
 * Checks if the command has to redirect output.
 *
 * @param args Arguments.
 */
inline bool redirects_output(args_t *args)
{
    return (args->size > 1) && (strcmp(args->args[args->size - 2], MODIFIER_OUTPUT) == 0);
}

/**
 *
 * Checks if the command has to be executed in background.
 *
 * @param args Arguments.
 */
inline bool is_background(args_t *args)
{
    return (args->size > 0) && (strcmp(args->args[args->size - 1], MODIFIER_BACKGROUND) == 0);
}

/**
 *
 * Reads line from stdin into buffer.
 *
 * @param buffer Buffer to read into.
 */
inline void read_into(char *buffer)
{
    // Read from stdin
    ssize_t lines = read(STDIN, buffer, BUFFER_SIZE);

    // Check for errors
    if (lines < 0)
    {
        // Print error
        perror("read");

        // Exit
        exit(EXIT_FAILURE);
    }

    // Remove trailing newline
    (buffer)[strcspn(buffer, "\n")] = '\0';
}

/**
 *
 * Parses arguments.
 *
 * @param buffer Buffer to parse.
 */
inline args_t *parse_args(char *buffer)
{
    size_t n = 0;
    args_t *args = malloc(sizeof(args_t));

    while (buffer != NULL)
    {
        buffer = strtok(NULL, " ");
        args->args[n] = buffer;
        n++;
    }

    args->size = n - 1;

    return args;
}

/**
 *
 * Remove flags from arguments.
 *
 * @param args Arguments.
 *
 */
inline args_t *crop_args(args_t *args)
{
    args_t *new_args = malloc(sizeof(args_t));
    new_args->size = args->size;
    for (int i = 0; i < new_args->size; i++)
    {
        new_args->args[i] = args->args[i];
    }
    return new_args;
}

/**
 *
 * Adds process to running processes.
 *
 * @param id Process id.
 * @param processes List of processes currently running.
 */
inline int add_process(pid_t id, process_t **processes)
{
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        if (processes[i] == NULL)
        {
            processes[i] = malloc(sizeof(process_t));
            processes[i]->pid = id;
            processes[i]->position = i;
            return i;
        }
    }
    printf("Too many processes running.\n");
    return -1;
}

/**
 *
 * Terminates all running processes.
 *
 * @param processes Processes.
 */
inline void terminate_processes(process_t **processes)
{
    // Loop through processes
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        // Check if process is running
        if (processes[i] != NULL)
        {
            // Kill process
            printf("Killing process %d\n", processes[i]->pid);
            fflush(NULL);
            kill(processes[i]->pid, SIGKILL);

            // Free process memory
            free(processes[i]);
        }
    }
}