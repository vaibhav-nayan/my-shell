#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_TOKENS 100
#define MAX_CMDS 20

pid_t fg_pid = -1;

typedef struct {
	int id;
	pid_t pid;
	char cmd[256];
	int stopped;
} Job;

Job jobs[100];
int job_count = 0;

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

void shell_jobs() {
	for(int i=0; i<job_count; i++) {
		printf("[%d] %s %s\n",
			jobs[i].id,
			jobs[i].stopped ? "stopped" : "running",
			jobs[i].cmd);
	}
}

void shell_fg(int job_id) {
	for(int i=0; i<job_count; i++){
		if(jobs[i].id == job_id) {
			fg_pid = jobs[i].pid;
			kill(fg_pid, SIGCONT);
			int status;
			waitpid(fg_pid, &status, 0);
			fg_pid = -1;

			for(int j=i; j<job_count-1; j++) jobs[j] = jobs[j+1];
			job_count--;
			return;
		}
	}
	printf("fg: no such job %d\n", job_id);
}

void shell_bg(int job_id) {
	for(int i=0; i<job_count; i++) {
		if(jobs[i].id == job_id) {
			kill(jobs[i].pid, SIGCONT);
			jobs[i].stopped = 0;
			return;
		}
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
	if(strcmp(args[0], "jobs") == 0) {
		shell_jobs();
	}
	if(strcmp(args[0], "fg") == 0) {
		if(args[1] != NULL && args[1][0] == '%') {
			shell_fg(atoi(args[1] + 1));
		} else {
			printf("usage: fg %%jobid\n");
		}
	}
	if(strcmp(args[0], "bg") == 0) {
		if(args[1] != NULL && args[1][0] == '%') {
			shell_bg(atoi(args[1] + 1));
		} else {
			printf("usage: bg %%jobid\n");
		}
	}
	
	int background = 0;
	for(int i=0; args[i] != NULL; i++) {
		if(strcmp(args[i], "&") == 0 && args[i+1] == NULL) {
			background = 1;
			args[i] = NULL;
			break;
		}
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
		if(background) {
			printf("[bg] %d\n", pid);

			// store job
			jobs[job_count].id = job_count+1;
			jobs[job_count].pid = pid;
			strncpy(jobs[job_count].cmd , args[0], sizeof(jobs[job_count].cmd));
			job_count++;
		}
		else {
			int status;
			fg_pid = pid;
			waitpid(pid, &status, 0);
			fg_pid = -1;
		}
	}else {
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

// pipeline execution
void execute_pipeline(char ***commands, int n) {
	int pipes[n-1][2];
	pid_t pids[n];
	
	int background = 0;
	for(int k=0; commands[n-1][k] != NULL; k++) {
		if(strcmp(commands[n-1][k], "&") == 0 && commands[n-1][k+1] == NULL) {
			background = 1;
			commands[n-1][k] = NULL;
			break;
		}
	}

	// create pipes
	for(int i=0; i<n-1; i++){
		if(pipe(pipes[i]) == -1) {
			perror("pipe");
			exit(1);
		}
	}
	
	// fork processes
	for(int i=0; i<n; i++) {
		pids[i] = fork();
		if(pids[i] == 0) {

			// child
			

			// input from prev pipe
			if(i>0) {
				dup2(pipes[i-1][0], STDIN_FILENO);
			}

			// output to next pipe
			if (i < n-1) {
				dup2(pipes[i][1], STDOUT_FILENO);
			}
			else {
			}

			// close all pipe fds in child
			for(int j=0; j<n-1; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}

			// handle redirections
			char *infile = NULL, *outfile = NULL;
			int append = 0;

			for(int k=0; commands[i][k] != NULL; k++) {
				if(strcmp(commands[i][k], "<") == 0) {
					infile = commands[i][k+1];
					commands[i][k] = NULL;
					break;
				}
				else if(strcmp(commands[i][k], ">") == 0 || strcmp(commands[i][k], ">>") == 0) {
					outfile = commands[i][k+1];
					append = (strcmp(commands[i][k], ">>") == 0);
					commands[i][k] = NULL;
					break;
				}
			}

			if(infile) {
				int fd = open(infile, O_RDONLY);
				if(fd < 0) {
					perror("open infile");
					exit(1);
				}
				dup2(fd, STDIN_FILENO);
				close(fd);
			}

			if(outfile) {
				int fd;
				if(append) fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
				else fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if(fd < 0) {
					perror("open outfile");
					exit(1);
				}
				dup2(fd, STDOUT_FILENO);
				close(fd);
			}

			// execute command
			if (execvp(commands[i][0], commands[i]) == -1) {
				perror("execvp");
				exit(1);
			}
		}
	}

	// parent closes all pipe ends
	for(int i=0; i<n-1; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	if(background) {
		printf("[bg] pipeline (pid %d)\n", pids[n-1]);

		// store only last PID to track pipeline job
		jobs[job_count].id = job_count+1;
		jobs[job_count].pid = pids[n-1];
		strncpy(jobs[job_count].cmd, commands[0][0], sizeof(jobs[job_count].cmd));
		job_count++;
	}

	// wait for all children
	for (int i=0; i<n; i++) {
		fg_pid = pids[i];
		waitpid(pids[i], NULL, 0);
		fg_pid = -1;
	}
}

// parse pipeline
int parse_pipeline(char *line, char ***commands) {
	int cmd_count = 0;
	char *cmd = strtok(line, "|");

	while(cmd != NULL && cmd_count < MAX_CMDS) {
		commands[cmd_count] = malloc(MAX_TOKENS * sizeof(char*));

		int pos = 0;
		char * token = strtok(cmd, " \t\r\n");
		while(token != NULL && pos < MAX_TOKENS-1) {
			commands[cmd_count][pos++] = token;
			token = strtok(NULL, " \t\r\n");
		}
		commands[cmd_count][pos] = NULL;

		cmd_count++;
		cmd = strtok(NULL, "|");
	}
	return cmd_count;
}

// dispatcher
void shell_execute_line(char *line) {
	char *line_copy = strdup(line);
	char **commands[MAX_CMDS];
	int n= parse_pipeline(line_copy, commands);

	if(n==1) {
		shell_execute(commands[0]);
	} else {
		execute_pipeline(commands, n);
	}

	for(int i=0; i<n; i++) {
		free(commands[i]);
	}
	free(line_copy);
}

void check_background_jobs() {
	int status;
	pid_t pid;
	while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		for(int i=0; i<job_count; i++){
			if(jobs[i].pid == pid) {
				if(WIFEXITED(status) || WIFSIGNALED(status)) {
					printf("[done] %s (pid %d)\n", jobs[i].cmd, pid);

					// remove from list 
					for(int j=i; j<job_count-1; j++){
						jobs[j] = jobs[j+1];
						jobs[j].id = j+1;

					}
					job_count--;
					break;
				}
			}
		}
	}
}

void sigint_handler(int sig) {
	(void)sig;
	if(fg_pid > 0) {
		kill(fg_pid, SIGINT);
	}
}

void sigtstp_handler(int sig) {
	(void)sig;
	if(fg_pid > 0) {
		kill (fg_pid, SIGTSTP);

		// mark as stopped job
		jobs[job_count].id = job_count + 1;
		jobs[job_count].pid = fg_pid;
		strcpy(jobs[job_count].cmd, "stopped-foreground");
		jobs[job_count].stopped = 1;
		job_count++;
	}
}

int main() {
	char *line = NULL;
	size_t bufsize = 0;
	
	signal(SIGINT, sigint_handler);
	signal(SIGTSTP, sigtstp_handler);

	while(1) {
		//printf("my_shell> ");
		print_prompt();
		if(getline(&line, &bufsize, stdin) == -1){
			break;
		}

		shell_execute_line(line);
		check_background_jobs();
	}

	free(line);
	return 0;
}

