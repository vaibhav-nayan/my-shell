#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_TOKENS 100

// cd
void shell_cd(char **args) {
	char *path = args[1];
	if(path == NULL){
		path = getenv("HOME");
		if(path == NULL) path = "/";
	}
	if(chdir(path) != 0){
		perror("cd");
	}
}


// pwd : print working directory
void shell_pwd() {
	char cwd[1024];
	if(getcwd(cwd, sizeof(cwd)) != NULL){
		printf("%s\n", cwd);
	} else {
		perror("pwd");
	}
}

// Execute command
void shell_execute(char **args){
	if(args[0] == NULL) return;

	if(strcmp(args[0], "cd") == 0){
		shell_cd(args);
		return;
	}
	if(strcmp(args[0], "pwd") == 0){
		shell_pwd();
		return;
	}
	if(strcmp(args[0], "exit") == 0) {
		exit(0);
	}

	pid_t pid = fork();
	if(pid == 0){
		//child
		
		char *infile = NULL, *outfile = NULL;
		int append = 0;

		// parse for redirection
		for(int i=0; args[i] != NULL; i++){
			if(strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
				outfile = args[i+1];
				append = (strcmp(args[i], ">>") == 0);
				args[i] = NULL;
				args[i+1] = NULL;
				break;
			} else if(strcmp(args[i], "<") == 0) {
				infile = args[i+1];
				args[i] = NULL;
				args[i+1] = NULL;
				break;
			}
		}
		
		// handle input redirection
		if(infile) {
			int fd = open(infile, O_RDONLY);
			if(fd < 0) {
				perror("open infile");
				exit(EXIT_FAILURE);
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
		}
		
		// handle output redirection
		if(outfile) {
			int fd;
			if(append) fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
			else fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if(fd < 0) {
				perror("open outfile");
				exit(EXIT_FAILURE);
			}
			dup2(fd, STDOUT_FILENO);
			close(fd);
		}

		//finally exec command
		if(execvp(args[0], args) == -1) {
			fprintf(stderr, "mysh: command not found: %s\n", args[0]);
		}
		exit(EXIT_FAILURE);
	}
	else if(pid > 0) {
		int status;
		waitpid(pid, &status, 0);
	}
	else {
		perror("fork");
	}
}



// split into tokens
char **parse_line(char *line){
	char **tokens = malloc(MAX_TOKENS * sizeof(char*));
	char *token = strtok(line, " \t\r\n");
	int pos = 0;

	while(token != NULL && pos < MAX_TOKENS - 1) {
		tokens[pos++] = token;
		token = strtok(NULL, " \t\r\n");
	}
	tokens[pos] = NULL;
	return tokens;
}

void print_prompt(){
	char cwd[1024];
	char *user = getenv("USER");

	if(getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s:%s$ ", user ? user : "user", cwd);
	} else {
		perror("getcwd");
		printf("$ ");
	}
	fflush(stdout);
}

int main() {
	char *line = NULL;
	size_t bufsize = 0;

	while(1) {
		//printf("my_shell> ");
		print_prompt();
		if(getline(&line, &bufsize, stdin) == -1){
			break;
		}

		char **args = parse_line(line);
		shell_execute(args);
		free(args);
	}

	free(line);
	return 0;
}

