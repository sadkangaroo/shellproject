#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#define MaxLen 600

sigset_t block_sigchild;

void childHandler(int signum) {
    waitpid(WAIT_ANY, NULL, WNOHANG | WUNTRACED);
}

typedef struct process {
    struct process *next; 
    char *argv[MaxLen];         
}process;

typedef struct job {
    process *first_process;    
    char * input;         
    char * output; 
    int foreground;
}job;

void free_process(process* p) {
    if (p == NULL) {
        return;
    }
    free_process(p->next);
    free(p);
}

void free_job(job* j) {
    if (j == NULL) {
        return;
    }
    free_process(j->first_process);
    free(j);
}

pid_t shell_pgid;
int shell_terminal;
struct termios shell_tmodes;

job *analyze(char **argv, int tokens) {
    job *j;
    process *p;
    char *command[MaxLen];
    int i, k, cnt;
    j = (job*)malloc(sizeof(job));
    j->first_process = NULL;
    j->input = NULL;
    j->output = NULL;
    j->foreground = 1;
    if (strcmp(argv[tokens - 1], "&") == 0) {
        j->foreground = 0;
        tokens--;
    }
    cnt = 0;    
    for ( i = 0; i <= tokens; i++) {        
        if (i == tokens || strcmp(argv[i], "|") == 0) {
            if (cnt == 0) {
                printf("ERROR: Unable to parse command\n");
                return NULL;
            }
            if (!j->first_process) {
                j->first_process = (process*)malloc(sizeof(process));
                for (k = 0; k < cnt; k++) {
                    j->first_process->argv[k] = command[k];
                }
                j->first_process->argv[k] = '\0';
                j->first_process->next = NULL;
            }	
            else {
                p = j->first_process; 
                while (p->next) {
                    p = p->next;
                }
                p->next = (process*)malloc(sizeof(process));
                p = p->next;
                for (k = 0; k < cnt; k++) 
                    p->argv[k] = command[k];
                p->argv[k] = '\0';
                p->next = NULL;                
            }
            cnt = 0;
        }        
        else if(strcmp(argv[i], "<") == 0) {
            if(j->first_process || i + 1 == tokens) {
                printf("ERROR: Unable to parse command\n");
                return NULL;
            }            
            j->input = argv[++i];
        }
        else if(strcmp(argv[i], ">") == 0) {
            if (i + 2 != tokens) {        
                printf("ERROR: Unable to redirect files in this manner\n"); return NULL;
            }
            j->output = argv[++i];
        } 
        else {
            command[cnt++] = argv[i];
        }
    }
    return j;
}

int get_tokens(char *line, char **argv) {
    int tokens = 0;
    while (*line != '\0') {      
        while (isspace(*line) || *line == '\0') {
            if (*line == '\0') {
                *argv = '\0';            
                return tokens;
            }
            *line++ = '\0';     
        }
        if(*line != '<' && *line != '>' && *line != '|' && *line != '&') {
            *argv++ = line; 
            tokens++;
        }
        while (!isspace(*line) && *line != '\0') {
            if (*line == '<' || *line == '>' || *line == '|' || *line == '&') {
                if (*line == '<') {
                    *argv++ = "<";
                }
                if (*line == '>') {
                    *argv++ = ">";
                }
                if (*line == '|') {
                    *argv++ = "|";
                }
                if (*line == '&') {
                    *argv++ = "&";
                }
                tokens++;
                *line++ = '\0';
                break;
            }
            else {
                line++;          
            }
        }
    }
    *argv = '\0';                 
    return tokens;
}


void launch_job(job *j) {
    int pid, mypipe[2], infile, outfile;
    process* p;
    if (j->input) {
        if ((infile = open(j->input, O_RDONLY))< 0) {
            printf("ERROR: Could not open read file\n");
            return;
        }
    }
    else {
        infile = STDIN_FILENO;
    }
    for (p = j->first_process; p != NULL; p = p->next) {
        if (p->next) {
            if (pipe(mypipe) < 0) {
                printf("ERROR: Unable to create pipeline\n");
                return;
            }
            outfile = mypipe[1];
        }
        else if (j->output) {
            outfile = open(j->output, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR);
        }
        else {
            outfile = STDOUT_FILENO;
        }
        sigprocmask (SIG_BLOCK, &block_sigchild, NULL);
        pid = fork();
        if (pid == 0) {
            if (infile != STDIN_FILENO) {
                if(close(STDIN_FILENO) < 0) {
                    printf("ERROR: Could not close STDIN\n");
                }
                if(dup(infile) != STDIN_FILENO) {
                    printf("ERROR: Could not dup input\n");
                }
            }
            if (outfile != STDOUT_FILENO) {
                if (close(STDOUT_FILENO) < 0) {
                    printf("ERROR: Could not close STDOUT\n");			
                }
                if (dup (outfile) != STDOUT_FILENO) {
                    printf("ERROR: dup output\n");
                }
            }	
            if (execvp (p->argv[0], p->argv) < 0) { 
                printf("ERROR: Could not execute command\n");
            }
            if (infile != STDIN_FILENO) {
                close(infile);
            }
            if (outfile != STDOUT_FILENO) {
                close(outfile);
            }
            exit(0);
        }
        else if (pid < 0) {
            printf("ERROR: forking child process failed\n");
            exit(1);
        }
        else {
            if (infile != STDIN_FILENO) {
                close(infile);
            }
            if (outfile != STDOUT_FILENO) {
                close(outfile);
            }
            infile = mypipe[0];
            if (j->foreground) {
                waitpid(pid, NULL, 0);
            }
        }
        sigprocmask(SIG_UNBLOCK, &block_sigchild, NULL);
    }
}

void cd(char *argv[], int tokens) {
    if (tokens != 2) {
        printf("ERROR: Invalid argument for cd\n");            
        return;
    }
    if (chdir(argv[1]) < 0) {
        printf("ERROR: No such file or directory %s\n", argv[1]);            
    }
}

int main() {
    char line[MaxLen];             
    char *argv[MaxLen];           
    int tokens;
    job *cnt_job;
    sigemptyset(&block_sigchild);
    sigaddset(&block_sigchild, SIGCHLD);
    signal(SIGCHLD, childHandler);    
    while (1) {                  
        printf("shell:");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n"); 
            return 0;
        }
        tokens = get_tokens(line, argv);
        if (argv[0] == '\0') {
            continue;
        }
        else if (strcmp(argv[0], "cd") == 0) { 
            cd(argv, tokens);                     
        }
        else {
            if ((cnt_job = analyze(argv, tokens)) != NULL) {
                launch_job(cnt_job);
                free_job(cnt_job);
            }
        }
    }
}
