#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>


// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;      // number of arguments
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none
    command* next; // next command to be processed
    int op;        // control operator, NULL if none
    char* rd_token; // redirect token (<, >, 2>)
    int pop;  // conditional operator, NULL if none
    int pstatus;    // status of previous process, NULL if not set
};


// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    c->pid = -1;
    c->next = NULL;
    c->op = -1;
    c->rd_token = NULL;
    c->pop = -1;
    c->pstatus = 0;
    return c;
}


// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
    for (int i = 0; i != c->argc; ++i)
        free(c->argv[i]);
    free(c->argv);
    free(c);
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// num_pipes(c)
// Determine the number of pipes

int num_ops(command* c, int op) {
    int num = 0;
    while (c && c->op == op) {
        num++;
        c = c->next;
    }
    return num;
}


// COMMAND EVALUATION

// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t start_command(command* c, pid_t pgid) {
    (void) pgid;
    // Your code here!
    c->pid = fork();
    if (!c->pid)
        execvp(c->argv[0], c->argv);
    return c->pid;
}

// run_list(c)
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `set_foreground(pgid)` before waiting for the pipeline.
//       - Call `set_foreground(0)` once the pipeline is complete.
//       - Cancel the list when you detect interruption.

void run_list(command* c) {

    pid_t pid = 0;
    int status;

    command* current = c;
    while(current && current->argc) {

    	if (current->op == TOKEN_REDIRECTION) {

    		int redirect_count = num_ops(current, TOKEN_REDIRECTION);
    		int fd[redirect_count];
    		pid_t cpid;

    		cpid = fork();

    		if (!cpid) {
				
    			command* cmd = current;

				for (int i = 0; i < redirect_count; i++) {
					current = current->next;

					if (!current || !current->argv[0]) {
                        printf("No such file or directory\n");
						exit(EXIT_FAILURE);
                    }

					// Redirect STDIN
					if (!strcmp(current->rd_token, "<")) {
                        fd[i] = open(current->argv[0], O_RDONLY);
                        if (fd[i] == -1) {
                            perror(strerror(errno));
                            exit(EXIT_FAILURE);
                        }
						dup2(fd[i], STDIN_FILENO);
					}

					// Redirect STDOUT
					else if (!strcmp(current->rd_token, ">")) {
                        fd[i] = open(current->argv[0], O_WRONLY | O_CREAT, 0666);
                        if (fd[i] == -1) {
                            printf("No such file or directory, error %s\n", strerror(errno));
                            exit(EXIT_FAILURE);
                        }
						dup2(fd[i], STDOUT_FILENO);
                    }
					// Reidrect STDERR
					else if (!strcmp(current->rd_token, "2>")) {
                        fd[i] = open(current->argv[0], O_WRONLY | O_CREAT, 0666);
                        if (fd[i] == -1) {
                            printf("No such file or directory, error %s\n", strerror(errno));
                            exit(EXIT_FAILURE);
                        }                        
						dup2(fd[i], STDERR_FILENO);
					}

					close(fd[i]);
    			}

                // If the current commmand is followed by a || (pipe)
                if (current->op == TOKEN_PIPE) {

                    // Count the number of pipes
                    int pipe_count = num_ops(current, TOKEN_PIPE); 
                    int fd[2 * pipe_count];
                    pid_t cpid;

                    // Create all of the pipes
                    for (int i = 0; i < pipe_count; i++) {
                        if (pipe(fd + i*2) == -1) {
                            perror("pipe");
                            exit(EXIT_FAILURE);
                        }
                    }

                    int cmd_count = 0;

                    // Process the pipe commands
                    while (current && (!cmd_count || current->pop == TOKEN_PIPE)) {

                        // First, fork!
                        cpid = fork();

                        if (cpid == -1) {
                            perror("fork");
                            exit(EXIT_FAILURE);
                        }


                        // If we are in the child process...
                        if (!cpid) {
                            // If not the first commmand
                            if (cmd_count)
                                dup2(fd[cmd_count - 2], STDIN_FILENO);

                            // If not the last command
                            if (current->next)
                                dup2(fd[cmd_count + 1], STDOUT_FILENO);

                            // Close file descriptors
                            for (int i = 0; i < 2*pipe_count; i++) {
                                close(fd[i]);
                            }                    

                            // Execute the 
                            if (cmd_count)
                                execvp(current->argv[0], current->argv);
                            execvp(cmd->argv[0], cmd->argv);
                        }

                        current->pid = cpid;

                        // Move on to the next command, and increment cmd_count
                        current = current->next;
                        cmd_count += 2;
                    }

                    // Close file descriptors
                    for (int i = 0; i < 2*pipe_count; i++) {
                        close(fd[i]);
                    }

                    // Wait for all child processes to finish
                    waitpid(cpid, &status, 0);

                    // Set the status for next command
                     if (current)
                        current->pstatus = status;

                    continue;
                }

    			execvp(cmd->argv[0], cmd->argv);
    		}

    		// Advanced to commands past redirection in parent
    		for (int i = 0; i < redirect_count; i++) {
    			current = current->next;
    		}

            if (current->op == TOKEN_PIPE) {
                int pipe_count = num_ops(current, TOKEN_PIPE); 
                for (int i = 0; i < pipe_count; i++) {
                    current = current->next;
                }
            }

	        if (current && current->op != TOKEN_BACKGROUND) {
	            waitpid(current->pid, &status, 0);          
	            if (current->next) {
	                current->next->pstatus = status;
	            }
	        }

	        current = current->next;
	        continue;
    	}


        // If the current commmand is followed by a || (pipe)
        if (current->op == TOKEN_PIPE) {

            // Count the number of pipes
            int pipe_count = num_ops(current, TOKEN_PIPE); 
            int fd[2 * pipe_count];
            pid_t cpid;

            // Create all of the pipes
            for (int i = 0; i < pipe_count; i++) {
                if (pipe(fd + i*2) == -1) {
                    perror("pipe");
                    exit(EXIT_FAILURE);
                }
            }

            int cmd_count = 0;

            // Process the pipe commands


            while (current && (!cmd_count || current->pop == TOKEN_PIPE)) {

                // First, fork!
                cpid = fork();

                if (cpid == -1) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }


                // If we are in the child process...
                if (!cpid) {
                    // If not the first commmand
                    if (cmd_count)
                        dup2(fd[cmd_count - 2], STDIN_FILENO);

                    // If not the last command
                    if (current->next)
                        dup2(fd[cmd_count + 1], STDOUT_FILENO);

                    // Close file descriptors
                    for (int i = 0; i < 2*pipe_count; i++) {
                        close(fd[i]);
                    }                    

                    // Execute the command
                    execvp(current->argv[0], current->argv);
                }

                current->pid = cpid;

                // Move on to the next command, and increment cmd_count
                current = current->next;
                cmd_count += 2;
            }

            // Close file descriptors
            for (int i = 0; i < 2*pipe_count; i++) {
                close(fd[i]);
            }

            // Wait for all child processes to finish
            waitpid(cpid, &status, 0);

            // Set the status for next command
             if (current)
                current->pstatus = status;

            continue;
        }


        // If at the start of the conditional, we want to run in the background
        if (((current->op == TOKEN_AND || (current->op == TOKEN_OR)) && 
            current->pop != TOKEN_AND && current->pop != TOKEN_OR))
            pid = fork();

        // In the parent process, don't run the conditionals
        if (pid && (current->op == TOKEN_AND || current->op == TOKEN_OR)) {
            current = current->next;
            continue;
        }

        if (!pid) {

            // Defualt the next command's status to the previous command
            if (current->next)
                current->next->pstatus = current->pstatus;

            // Check conditional statements
            if (!WIFEXITED(current->pstatus)) {
                current = current->next;
                continue;
            }

            if (current->pop == TOKEN_AND && WEXITSTATUS(current->pstatus)) {
                current = current->next;
                continue;
            }


            if (current->pop == TOKEN_OR && !WEXITSTATUS(current->pstatus)) {
                current = current->next;
                continue;
            }
        }

        // If at the end of the conditional...
        if ((current->pop == TOKEN_AND || current->pop == TOKEN_OR) 
            && current->op != TOKEN_AND && current->op != TOKEN_OR) {

            //... and if in a child process, we want to end it
            if (!pid) {
                start_command(current, 0);
                waitpid(current->pid, &status, 0);
                exit(EXIT_SUCCESS);
            }

            //... otherwise, we will put the process in the background if &
            if (current->op != TOKEN_BACKGROUND) {           
                waitpid(pid, &status, 0);
            }

            pid = 0;
            current = current->next;
            continue;
        }      
    

        start_command(current, 0);
        if (current->op != TOKEN_BACKGROUND) {
            waitpid(current->pid, &status, 0);            
            if (current->next) {
                current->next->pstatus = status;
            }
        }

        current = current->next;
    }
}


// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

void eval_line(const char* s) {
    int type;
    char* token;
    // Your code here!
    // build the command
    command* c = command_alloc();
    command* current = c;
    while ((s = parse_shell_token(s, &type, &token)) != NULL) {
        if (type) {
            current->op = type;
            current->next = command_alloc();
            current = current->next;
            
            // Set previous operator for the next command
            current->pop = type;
            if (type == TOKEN_REDIRECTION) {
            	current->rd_token = token;
            }
        }
        else 
            command_append_arg(current, token);
    }

    // execute it
    if (c->argc)
        run_list(c);

    // free all commands
    current = c;
    while (current) {
        command* next = current->next;
        command_free(current);
        current = next; 
    }
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    set_foreground(0);
    handle_signal(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file))
                    perror("sh61");
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
        int status;
        int pid = 1;

        while (pid != -1 && pid != 0) {
            pid = waitpid(-1, &status, WNOHANG);
        }
    }

    return 0;
}
