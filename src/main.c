#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_TOKENS 100
#define MAX_CMDS 20
#define MAX_HISTORY 1000
#define MAX_VARS 100
#define MAX_VAR_NAME 64
#define MAX_VAR_VALUE 256

typedef struct {
	int id;
	pid_t pid;
	char cmd[256];
	int stopped;
} Job;

typedef struct {
	char name[MAX_VAR_NAME];
	char value[MAX_VAR_VALUE];
} ShellVar;


Job jobs[100];
int job_count = 0;
pid_t fg_pid = -1;
char *history[MAX_HISTORY];
int history_count = 0;
ShellVar shell_vars[MAX_VARS];
int var_count = 0;



// trim trailing newline
static void trim_newline(char *s){
	size_t len = strlen(s);
	if(len > 0 && s[len-1] == '\n') s[len-1] = '\0';
}

// add history
void add_history_entry(const char *line) {
	if(line == NULL) return;
	char *clean = strdup(line);
	if(!clean) return;
	trim_newline(clean);

	if(history_count > 0 && strcmp(history[history_count-1], clean) == 0) {
		free(clean);
		return;
	}

	if(history_count < MAX_HISTORY) {
		history[history_count++] = clean;
	} else {
		free(history[0]);
		for(int i=1; i<MAX_HISTORY; i++) history[i-1] = history[i];
		history[MAX_HISTORY-1] = clean;
	}
}

// expand history
char *expand_history(const char *line) {
	if(line == NULL) return NULL;

	// work on local copy
	char tmp[MAX_TOKENS * 32];
	strncpy(tmp, line, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';
	trim_newline(tmp);

	if(strcmp(tmp, "!!") == 0) {
		if(history_count == 0) {
			printf("mysh: no commands in history\n");
			return NULL;
		}
		return strdup(history[history_count -1]);
	}

	if(tmp[0] == '!' && isdigit((unsigned char)tmp[1])) {
		int idx = atoi(tmp+1)-1;
		if(idx < 0 || idx >= history_count) {
			printf("mysh: no such command in history: %s\n", tmp);
			return NULL;
		}
		return strdup(history[idx]);
	}

	return strdup(tmp);
}

// set/update local variable
void set_local_var(const char *name, const char *value){
	if(!name) return;
	for(int i=0; i<var_count; i++){
		if(strcmp(shell_vars[i].name, name) == 0) {
			strncpy(shell_vars[i].value, value? value: "", sizeof(shell_vars[i].value)-1);
			shell_vars[i].value[sizeof(shell_vars[i].value)-1] = '\0';
			return;
		}
	}

	if(var_count < MAX_VARS) {
		strncpy(shell_vars[var_count].name, name, sizeof(shell_vars[var_count].name)-1);
		shell_vars[var_count].name[sizeof(shell_vars[var_count].name)-1] = '\0';
		strncpy(shell_vars[var_count].value, value? value: "", sizeof(shell_vars[var_count].value)-1);
		shell_vars[var_count].value[sizeof(shell_vars[var_count].value)-1] = '\0';
		var_count++;
	}
}

// get local var
const char* get_local_var(const char *name) {
	if(!name) return NULL;
	for(int i=0; i<var_count; i++){
		if(strcmp(shell_vars[i].name, name) == 0) {
			return shell_vars[i].value;
		}
	}
	return NULL;
}

// unset
void unset_local_var(const char *name) {
	if (!name) return;
	for(int i=0; i<var_count; i++) {
		if(strcmp(shell_vars[i].name, name) == 0) {
			for(int j=i; j<var_count-1; j++) {
				shell_vars[j] = shell_vars[j+1];
			}
			var_count--;
			return;
		}
	}
}

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

void shell_history() {
	for(int i=0; i<history_count; i++) {
		printf("%d %s\n", i+1, history[i]);
	}
}

// Execute command
void shell_execute(char **args){
	
	if(args[0] == NULL) return;

	if(args[0] != NULL) {
		char *eq = strchr(args[0], '=');
		if(eq != NULL && args[1] == NULL) {
			size_t namelen = eq-args[0];
			if(namelen > 0 && namelen < MAX_VAR_NAME) {
				char name[MAX_VAR_NAME];
				memcpy(name, args[0], namelen);
				name[namelen] = '\0';

				int ok = (isalpha((unsigned char)name[0]) || name[0] == '_');
				for(size_t k = 1; k<namelen && ok; ++k) {
					if(!(isalnum((unsigned char)name[k]) || name[k] == '_')) ok = 0;
				}
				if(ok) {
					const char *value = eq + 1;
					set_local_var(name, value);
					return;
				}
			}
		}
	}

	// built ins
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
		return;
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
	if(strcmp(args[0], "history") == 0) {
		shell_history();
		return;
	}
	if(strcmp(args[0], "export") == 0) {
		if(args[1]) {
			const char *val = get_local_var(args[1]);
			if(val) {
				setenv(args[1], val, 1);
			}
		}
		return;
	}
	if(strcmp(args[0], "unset") == 0) {
		if(args[1]) {
			unset_local_var(args[1]);
			unsetenv(args[1]);
		}
		return;
	}
	
	int background = 0;
	for(int i=0; args[i] != NULL; i++) {
		if(strcmp(args[i], "&") == 0 && args[i+1] == NULL) {
			background = 1;
			args[i] = NULL;
			break;
		}
	}

	// variable assignment check
	char *eq = strchr(args[0], '=');
	if(eq != NULL) {
		*eq = '\0';
		set_local_var(args[0], eq+1);
		return;
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

	for(int j=0; j<pos; j++){
		if(tokens[j][0] == '$') {
			const char *val = get_local_var(tokens[j] + 1);
			if(!val) val = getenv(tokens[j] + 1);
			if(val) {
				tokens[j] = strdup(val);
			} else {
				tokens[j] = strdup("");
			}
		}
	}

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

char *expand_token(const char *token) {
	if (!token) return NULL;
	size_t tlen = strlen(token);
	size_t cap = tlen + 64;
	char *out = malloc(cap);
	if(!out) return NULL;
	size_t oi = 0;

	for(size_t i=0; i<tlen; ++i) {
		if(token[i] == '$') {
			size_t j = i+1;

			// variable name: [A-Za-z]
			if(j < tlen && (isalpha((unsigned char)token[j]) || token[j] == '_')) {
				size_t start = j;
				while(j<tlen && (isalnum((unsigned char)token[j]) || token[j] == '_')) j++;
				size_t varlen = j-start;
				char varname[256];
				if(varlen >= sizeof(varname)) varlen = sizeof(varname) -1;
				memcpy(varname, token + start, varlen);
				varname[varlen] = '\0';

				const char *val = get_local_var(varname);
				if(!val) val = getenv(varname);
				if(!val) val = "";

				size_t vlen = strlen(val);
				if(oi + vlen + 1 > cap) {
					cap = oi + vlen + 64;
					char *tmp = realloc(out, cap);
					if(!tmp) {
						free(out);
						return NULL;
					}
					out = tmp;
				}
				memcpy(out+oi, val, vlen);
				oi += vlen;

				i = j-1;
				continue;
			} else {
				if(oi + 2 > cap) {
					cap = oi + 64;
					char *tmp = realloc(out, cap);
					if(!tmp) {free(out); return NULL;}
					out = tmp;
				}
				out[oi++] = '$';
				continue;
			}
		} else {
			if(oi + 2 > cap) {
				cap = oi + 64;
				char *tmp = realloc(out, cap);
				if(!tmp) {free(out); return NULL;}
				out = tmp;
			}
			out[oi++] = token[i];
		}
	}

	out[oi] = '\0';
	return out;
}


// parse pipeline
int parse_pipeline(char *line, char ***commands) {
	int cmd_count = 0;
	char *cmd = strtok(line, "|");

	while(cmd != NULL && cmd_count < MAX_CMDS) {
		commands[cmd_count] = malloc(MAX_TOKENS * sizeof(char*));
		if(!commands[cmd_count]) return cmd_count;


		int pos = 0;
		char * token = strtok(cmd, " \t\r\n");
		while(token != NULL && pos < MAX_TOKENS-1) {

			char *expanded = expand_token(token);
			if(expanded) {
				commands[cmd_count][pos++] = expanded;
			} else {
				commands[cmd_count][pos++] = strdup(token);
			}
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
	if(line == NULL) return;

	char *expanded = expand_history(line);
	if(!expanded) return;

	add_history_entry(expanded);

	if(strcmp(expanded, "history") == 0) {
		shell_history();
		free(expanded);
		return;
	}
	
	char *line_copy = strdup(expanded);
	if(!line_copy) { free(expanded); return; }
	free(expanded);

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

char *command_generator(const char *text, int state) {
	static int list_index, len;
	static char *commands[] = {
		"cd", "pwd", "exit", "jobs", "fg", "bg", "history", NULL
	};

	if(!state) {
		list_index = 0;
		len = strlen(text);
	}

	while(commands[list_index]) {
		char *name = commands[list_index];
		list_index++;
		if(strncmp(name, text, len) == 0) {
			return strdup(name);
		}
	}
	return NULL;
}

char **my_completion(const char *text, int start, int end) {
	(void)start; (void)end;
	return rl_completion_matches(text, command_generator);
}

char *make_prompt() {
	static char prompt[1024];
	char cwd[512];
	char *user = getenv("USER");

	if(getcwd(cwd, sizeof(cwd)) != NULL) {
		snprintf(prompt, sizeof(prompt), "%s:%s$ ", user? user: "user", cwd);
	} else {
		snprintf(prompt, sizeof(prompt), "$ ");
	}
	return prompt;
}

int main() {
	rl_attempted_completion_function = my_completion;

	char *line = NULL;
	signal(SIGINT, sigint_handler);
	signal(SIGTSTP, sigtstp_handler);

	while(1) {
		line = readline(make_prompt());

		if(!line) break;

		if(strlen(line) > 0) {
			add_history(line);
			shell_execute_line(line);
		}

		free(line);
		check_background_jobs();
	}
	free(line);
	return 0;
}

