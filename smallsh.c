#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

void type_prompt() {
	static int first_time = 1;
	if (first_time) {
		printf("Welcome to smallsh!  Type 'exit' to leave.\n");
		first_time = 0;
	}
	printf(":");		// Display prompt
	fflush(stdout);		// Ensure prompt is shown immediately
}

void read_command (char cmd[], char *par[]) {
	char line[1024];
	int count = 0, i = 0, j = 0;
	char *array[100], *token;

	if (!fgets(line, sizeof(line), stdin)) {	// Read input line
		return;	// If EOF, return	
	}

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
}

int main() {

	char cmd[100], command[100], *parameters[20];		// Command and parameters arrays

	while(1) {					// Repeat forever

		type_prompt();			// Display prompt
		read_command(command, parameters); // Read command from user	

		if (parameters[0] == NULL) {
			continue;			// If no command, prompt again
		}

		if (strcmp(command, "exit") == 0) {
			break;				// Exit if command is "exit"
		}

		pid_t pid = fork();		// Create a new process
		
		if (pid == 0) {
			// Child process

			for (int i = 0; parameters[i] != NULL; i++) {		// Check for output redirection
				if (strcmp(parameters[i], ">") == 0) {	
					// Handle output redirection
					int fd = open(parameters[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
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
				if (strcmp(parameters[i], "<") == 0) {	
					// Handle input redirection
					int fd = open(parameters[i + 1], O_RDONLY);
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
			execvp(command, parameters); // Run command
			perror("execvp failed"); // If execvp returns, an error occurred
			exit(1);
		}
		else {
			wait(NULL);				// Parent process waits for child
		}
	}

	return 0;
}
