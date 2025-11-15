#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>

int mode; 			// global variable used to track foreground-only mode

/*
* Function: switch_mode()
* Description: Used when SIGTSTP is called, it will toggle the mode variable between
*			 normal mode (0) and foreground-only mode (1), and print
*			 a message using write to inform the user of the mode change.
*/
void switch_mode() {
	if (mode == 0) {
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
		mode = 1;
	} else {
		char* message = "\nExiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 30);
		mode = 0;
	}
	return;
}

/*
 * Function: type_prompt()
 * Description: Displays the command prompt to the user. On the first call, it will 
 * also print a welcome message. The prompt is displayed as ":" and flushed to ensure 
 * it appears immediately.
*/
void type_prompt() {
	static int first_time = 1;
	if (first_time) {
		printf("Welcome to smallsh!  Type 'exit' to leave.\n");
		first_time = 0;
	}
	printf(":");		// Display prompt
	fflush(stdout);		// Ensure prompt is shown immediately
}

/*
* Function: variable_expansion
* Description: This function takes an input string and replaces all occurrences of "$$"
*			 with the provided process ID (pid). It returns a new string with the
*			 expanded content. Memory for the new string is dynamically allocated and
*			 freed by where it is called in main()
* Parameters:
*   - input_str: The original input command from the user containing potential "$$" patterns.
*   - pid: The process ID to replace "$$" with.
* Returns:
*   - A new string with all "$$" replaced by the given pid.
*/
char* variable_expansion(char* input_str, int pid) {
	char* str = malloc(2049);		// Allocate memory for expanded string
	char* ptr = input_str;
	str[0] = '\0';
	// Scan through input string and replace $$ with PID
	while (*ptr) {
		if (*ptr == '$' && *(ptr + 1) == '$') {
			char pid_str[20];
			sprintf(pid_str, "%d", pid);
			strcat(str, pid_str);
			ptr += 2;
		} else {
			size_t len = strlen(str);
			str[len] = *ptr;
			str[len + 1] = '\0';
			ptr++;
		}
	}
	return str;
} 

/*
* Function: cd()
* Description: Changes the current working directory of the process. If no path is
*			 provided, it changes to the user's home directory. If a path is provided,
*			 it attempts to change to that directory and prints an error message if
*			 the change fails.
* Parameters: 
*	- path: The target directory path to change to. If NULL, changes to home directory.
*/
void cd(char *path) {
	// chdir and getenv from the assignment description file
	if (path == NULL) {
		chdir(getenv("HOME"));		// Change to home directory if no path
	} else {
		if (chdir(path) != 0) {		// Change to specified directory
			perror("chdir failed");	// Print error if chdir fails
		}
	}
}

/*
* Function: read_command()
* Description: Reads a command line from standard input, calls variable_expansion to
*			 replace any occurrences of "$$" with the current process ID, and parses
*			 the command into a command string and an array of parameters. It also
*			 checks if the command should be run in the background (if it ends with '&').
* Parameters:
*   - cmd: A character array to store the command name.
*   - par: An array of character pointers to store the command parameters.
*   - background: A pointer to an integer that will be set to 1 if the command is to be
*                 run in the background, or 0 otherwise.
*/
void read_command (char cmd[], char *par[], int *background) {
	char line[2049];
	int count = 0, i = 0, j = 0;
	char *array[100], *token;
	int pid = getpid(); // getpid() from the assignment description file

	if (!fgets(line, sizeof(line), stdin)) {	// Read input line
		return;	// If there is an error or EOF, return	
	}

	// Replace $$ with PID
	char* expanded_line = variable_expansion(line, pid);
	strcpy(line, expanded_line);
	free(expanded_line);

	token = strtok (line, " \n");		// Tokenize input line
	while (token != NULL) {	
		array[i++] = strdup(token);
		token = strtok (NULL, " \n");
	}
	if (i == 0) {			// No command entered
		cmd[0] = '\0';
		par[0] = NULL;
		*background = 0;
		return;		
	}

	if (i > 0) {
		strcpy(cmd, array[0]);		// First token is command
	}

	for (int j = 0; j < i; j++) {		// Copy tokens to parameters array
		par[j] = array[j];
	}
	par[i] = NULL;			// NULL-terminate the parameters array

	if (i > 0) {
		if (strcmp(par[i - 1], "&") == 0) {	// Check for background process
					par[i - 1] = NULL;	// Remove '&' from parameters
					*background = 1;		// Set background flag
		}
		else {
			*background = 0;		// Clear background flag
		}
	}
}

/*
* Function: background_tracker()
* Description: This function checks for any completed background processes using
*			 waitpid with the WNOHANG option. If a background process has completed,
*			 it prints the process ID and its exit status or termination signal.
*/
void background_tracker() {
	pid_t apid = 0;
	int track_status = 0;
	// Uses waitpid and WNOHANG to check for completed background processes and determine their status
	// waitpid, WNOHANG, and the WIFEXITED functions from the lecutre notes on Processes
	while ((apid = waitpid(-1, &track_status, WNOHANG)) > 0) {
		printf("background pid %d is done: ", apid);
		if (WIFEXITED(track_status)) {
			printf("exit value %d\n", WEXITSTATUS(track_status));
		} else {
			printf("terminated by signal %d\n", WTERMSIG(track_status));
		}
	}
}

int main() {

	// sigaction and functions from the lecture notes on Signals, plus assignment description file
	struct sigaction SIGTSTP_action = {0}; 		// Set up SIGTSTP handler
	SIGTSTP_action.sa_handler = switch_mode; // calls switch_mode for variable toggling and message printing
	sigfillset(&SIGTSTP_action.sa_mask);		// Block all signals during the handler
	SIGTSTP_action.sa_flags = SA_RESTART; 	// Restart interrupted sys calls
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);	// Install SIGTSTP handler, tell sigaction to watch for SIGTSTP

	// Also from lecture notes on Signals (the CTRL-C handling part)
	signal(SIGINT, SIG_IGN);		// Ignore SIGINT signals)

	char cmd[100], command[100], *parameters[20];		// Command and parameters arrays

	int status_value = 0; 			// Status value for last foreground process
	int background = 0;				// Background process flag

	while(1) {					// Repeat forever

		background_tracker();		// Check for completed background processes
		type_prompt();			// Display prompt
		read_command(command, parameters, &background); // Read command from user	

		// Handle built-in commands
		if (parameters[0] == NULL || command[0] == '#') {
			continue;			// If no command or a comment, prompt again
		}
		if (strcmp(command, "exit") == 0) {
			break;				// Exit if command is "exit"
		}
		if (strcmp(command, "cd") == 0) {
			cd(parameters[1]);	// Change directory if command is "cd"
			continue;
		}
		if (strcmp(command, "status") == 0) {	// if command is "status" print last status
			// From lecture notes on Processes
			if (WIFEXITED(status_value)) {
				printf("exit value %d\n", WEXITSTATUS(status_value));
			} else {
				printf("terminated by signal %d\n", WTERMSIG(status_value));
			}	
			continue;
		}

		// fork() from the lecture notes on Processes
		pid_t pid = fork();		// Create a new process

		if (mode == 1) {		// If in foreground-only mode, force foreground
			background = 0;
		}
		
		if (pid == 0) {
			// This is the child process created from fork()

			// Handle SIGINT and SIGTSTP based on foreground/background status
			if (background == 1) {		// If background process, ignore SIGINT
				signal(SIGINT, SIG_IGN); // Sets to ingore SIGINT
			}
			if (background == 0) {		// Foreground process
				signal(SIGINT, SIG_DFL);	// Restore default SIGINT behavior
			}

			// Handle output redirection, dup2 from lecture notes "More UNIX I/O"
			for (int i = 0; parameters[i] != NULL; i++) {		// Check for output redirection
				int fd;
				if (strcmp(parameters[i], ">") == 0) {
					// Handle output redirection in background, /dev/null from assignment description file
					if(background == 1) {		// Redirect output to /dev/null for background processes
						if (parameters[i + 1] != NULL) {
							fd = open(parameters[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
						}
						else {
							fd = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
						}
						if (fd < 0) {
							perror("open");
							exit(1);
						}
						// dup2 from lecture notes "More UNIX I/O"
						dup2(fd, STDOUT_FILENO); // Redirect stdout to /dev/null
						close(fd);
						parameters[i] = NULL; // Remove redirection from parameters
						break;
					}
					// Handle output redirection in forground
					fd = open(parameters[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
					if (fd < 0) {
						perror("open");
						exit(1);
					}
					// dup2 from lecture notes "More UNIX I/O"
					dup2(fd, STDOUT_FILENO); // Redirect stdout to file
					close(fd);
					parameters[i] = NULL; // Remove redirection from parameters
					break;
				}
			}
			// Handle input redirection, dup2 from lecture notes "More UNIX I/O"
			for (int i = 0; parameters[i] != NULL; i++) {		// Check for input redirection
				int fd;
				if (strcmp(parameters[i], "<") == 0) {
					// Handle input redirection in background, /dev/null from assignment description file
					if(background == 1) {			// Redirect input from /dev/null for background processes
						if (parameters[i + 1] != NULL) {
							fd = open(parameters[i + 1], O_RDONLY);
						}
						else {
							fd = open("/dev/null", O_RDONLY);
						}
						if (fd < 0) {
							printf("Error: file cannot be opened\n");
							exit(1);
						}
						dup2(fd, STDIN_FILENO); // Redirect stdin from /dev/null
						close(fd);
						parameters[i] = NULL; // Remove redirection from parameters
						break;
					}	
					// Handle input redirection in foreground
					fd = open(parameters[i + 1], O_RDONLY);
					if (fd < 0) {
						printf("Error: file cannot be opened\n");
						exit(1);
					}
					dup2(fd, STDIN_FILENO); // Redirect stdin from file
					close(fd);
					parameters[i] = NULL; // Remove redirection from parameters
					break;
				}
			}

			strcat(cmd, command);	// Prepare command for execvp
			signal(SIGTSTP, SIG_IGN);	// Ignore SIGTSTP in child process
			int fail = execvp(command, parameters); // Run command using execvp, from lecture notes on Processes
			if (fail == -1) {		// If execvp fails, print error and exit
				printf("Error: Command cannot be executed\n");
				fflush(stdout);
				exit(1);
			}
		}
		else {
			// This is the parent process after fork()
			if (background != 1) {		// Parent process waits for child to finish
				if (waitpid(pid, &status_value, 0)== -1) {
					perror("waitpid failed");
					exit(1);
				}
				WIFEXITED(status_value); 		// Check if child exited normally
				if (WIFSIGNALED(status_value)) {
					printf("terminated by signal %d\n", WTERMSIG(status_value));
					fflush(stdout);
				}
			}
			// Background process handling
			else {
				printf("Background pid is %d\n", pid); // Print background PID
				fflush(stdout);
			}
		}
	}
	return 0;
}
