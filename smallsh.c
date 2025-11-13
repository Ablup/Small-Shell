#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>


void type_prompt() {
	static int first_time = 1;
	if (first_time) {
		printf("Welcome to smallsh!  Type 'exit' to leave.\n");
		first_time = 0;
	}
	printf(":");		// Display prompt
	fflush(stdout);		// Ensure prompt is shown immediately
}

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

void cd(char *path) {
	if (path == NULL) {
		chdir(getenv("HOME"));		// Change to home directory if no path
	} else {
		if (chdir(path) != 0) {		// Change to specified directory
			perror("chdir failed");	// Print error if chdir fails
		}
	}
}

void read_command (char cmd[], char *par[], int *background) {
	char line[2049];
	int count = 0, i = 0, j = 0;
	char *array[100], *token;
	int pid = getpid();

	if (!fgets(line, sizeof(line), stdin)) {	// Read input line
		return;	// If EOF, return	
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
	if (count == 1) return;			// If no input, return

	strcpy(cmd, array[0]);		// First token is command

	for (int j = 0; j < i; j++) {		// Copy tokens to parameters array
		par[j] = array[j];
	}
	par[i] = NULL;			// NULL-terminate the parameters array

	if (strcmp(par[i - 1], "&") == 0) {	// Check for background process
				par[i - 1] = NULL;	// Remove '&' from parameters
				*background = 1;		// Set background flag
	}
	else {
		*background = 0;		// Clear background flag
	}
	

}

void background_tracker() {
	pid_t apid = 0;
	int track_status = 0;
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
		if (strcmp(command, "status") == 0) {			// Print status of last foreground process
			if (WIFEXITED(status_value)) {
				printf("exit value %d\n", WEXITSTATUS(status_value));
			} else {
				printf("terminated by signal %d\n", WTERMSIG(status_value));
			}			// Show status of most recent foreground command if command is "status"
			continue;
		}

		pid_t pid = fork();		// Create a new process
		
		if (pid == 0) {
			// Child process

			for (int i = 0; parameters[i] != NULL; i++) {		// Check for output redirection
				int fd;
				if (strcmp(parameters[i], ">") == 0) {
					// Handle output redirection in background
					if(background == 1) {			// Redirect output to /dev/null for background processes
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
					dup2(fd, STDOUT_FILENO); // Redirect stdout to file
					close(fd);
					parameters[i] = NULL; // Remove redirection from parameters
					break;
				}
			}
			for (int i = 0; parameters[i] != NULL; i++) {		// Check for input redirection
				int fd;
				if (strcmp(parameters[i], "<") == 0) {
					// Handle input redirection in background
					if(background == 1) {			// Redirect input from /dev/null for background processes
						if (parameters[i + 1] != NULL) {
							fd = open(parameters[i + 1], O_RDONLY);
						}
						else {
							fd = open("/dev/null", O_RDONLY);
						}
						if (fd < 0) {
							perror("open");
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
						perror("open");
						exit(1);
					}
					dup2(fd, STDIN_FILENO); // Redirect stdin from file
					close(fd);
					parameters[i] = NULL; // Remove redirection from parameters
					break;
				}
			}

			strcat(cmd, command);
			execvp(command, parameters); // Run command using execvp
			perror("execvp failed"); // If execvp returns, an error occurred
			exit(1);
		}
		else {
			if (background != 1) {		// Parent process waits for child to finish
				if (waitpid(pid, &status_value, 0)== -1) {
					perror("waitpid failed");
					exit(1);
				}
			}
			else {
				printf("Background pid is %d\n", pid); // Print background PID
				fflush(stdout);
			}
		}
	}

	return 0;
}
