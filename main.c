#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <dirent.h>

enum {normal, hist, pip, hist_pip};

struct Comand{
    int type;
    int in_fd;
    int out_fd;
    bool built_in;
    char** tokens;
    char* text;
};

char* history[200];
int history_count = 0;
struct Comand comand;
struct Comand subcomand;

void handler(int sgn){
    if(sgn == SIGINT){
        kill(-getppid(),SIGINT);
    }
}

void try_save_comand(struct Comand* com) {
    if (!history_count && com->text[0] != ' ')
        history[history_count++] = com->text;
    else if(com->text[0] != ' ' && strcmp(history[history_count - 1], com->text) != 0)
        history[history_count++] = com->text;
}

void build_comand(char text[], size_t end, struct Comand* com, bool save);

void init_comand(struct Comand* com){
    com->in_fd = STDIN_FILENO;
    com->out_fd = STDOUT_FILENO;
    com->text = "";
    com->type = normal;
    com->built_in = true;
    com->tokens = calloc(20, sizeof(char*));
    com->text = malloc(300);
}

void tokenize(char text[], size_t end, struct Comand* com){

    strncpy(com->text,text,end - 1);

    int ind_token = 0;
    char* token = malloc(50);
    int index= 0;
    for (int j = 0; j < 300; ++j) {
        if(text[j] == '\n' || text[j] == 0){
            com->tokens[ind_token] = malloc(50);
            strncpy(com->tokens[ind_token],token,strlen(token));
            break;
        }
        else if(text[j] == ' '){
            if(strcmp(token,"") != 0){
                com->tokens[ind_token] = malloc(50);
                strcpy(com->tokens[ind_token++],token);
                token = malloc(50);
                index = 0;
            }
            while(text[j] == ' ')
                j++;

            if(text[j] == '\n' || text[j] == 0)
                break;
        }
        if((text[j] == '|' || text[j] == '<' || text[j] == '>') && strcmp(token,"") != 0) {
            com->tokens[ind_token] = malloc(50);
            strncpy(com->tokens[ind_token++], token, strlen(token));
            com->tokens[ind_token] = malloc(50);

            int a = text[j];
            char* a1 = malloc(300);
            int ind = 0;
            while(text[j] == a){
                a1[ind++] = text[j];
                j++;
            }

            strcpy(com->tokens[ind_token++], a1);
            token = malloc(50);
            index = 0;

            if(text[j] == '\n' || text[j] == 0)
                break;
        }
        else if((text[j] == '|' || text[j] == '<' || text[j] == '>') && strcmp(token,"") == 0){
            com->tokens[ind_token] = malloc(50);

            int a = text[j];
            char* a1 = malloc(300);
            int ind = 0;
            while(text[j] == a){
                a1[ind++] = text[j];
                j++;
            }
            strcpy(com->tokens[ind_token++], a1);

            if(text[j] == '\n' || text[j] == 0)
                break;
        }

        if(text[j] != 32)
            token[index++] = text[j];
    }
}

void parse_comand(struct Comand* com, bool save){
    for (int i = 0; i < 20; ++i) {
        char* token = com->tokens[i];

        if(token == NULL)
            break;
        else if(strcmp(token,"<") == 0){
            com->tokens[i] = NULL;
            int file = open(com->tokens[i + 1],O_RDONLY);
            if(file == -1)
                printf("%s: Invalid input\n", com->tokens[i + 1]);
            com->in_fd = file;
        }
        else if(strcmp(token,">>") == 0){
            com->tokens[i] = NULL;
            int file = open(com->tokens[i + 1],O_CREAT|O_APPEND|O_WRONLY,00777);
            if(file == -1)
                printf("%s: Invalid input\n", com->tokens[i + 1]);
            com->out_fd = file;
        }
        else if(strcmp(token,">") == 0) {
            com->tokens[i] = NULL;
            int file = open(com->tokens[i + 1], O_CREAT | O_TRUNC | O_WRONLY, 00777);
            if (file == -1)
                printf("%s: Invalid input\n", com->tokens[i + 1]);
            com->out_fd = file;

        }
        else if(strcmp(token,"|") == 0){
            com->tokens[i] = NULL;

            if(com->type == hist)
                com->type = hist_pip;
            else
                com->type = pip;

            char* subcomand_text = malloc(300);
            strcpy(subcomand_text,strchr(com->text,'|'));
            *subcomand_text = 32;

            build_comand(subcomand_text,strlen(subcomand_text) + 1,&subcomand,false);
            try_save_comand(com);
            return;
        }
        else if(strcmp(token,"cd") == 0){
            com->built_in = false;
            if(chdir(com->tokens[i + 1]) == -1){
                printf("%s: No such file or directory\n", com->tokens[i + 1]);
            }
            break;
        }
        else if(strcmp(token,"history") == 0){
            com->built_in = false;
            com->type = hist;
        }
        else if(strncmp(token,"!",1) == 0){

            char* val = strtok(token,"!");

            if(val == NULL){
                char* t = malloc(300);
                strcpy(t,history[history_count - 1]);
                strcat(t,strchr(com->text,'!') + 2);

                build_comand(t, strlen(t) + 1,com,true);
                return;
            }
            for (int j = history_count; j > 0; --j) {
                if(strncmp(history[j - 1],val,strlen(val)) == 0){
                    char* t = malloc(300);
                    strcpy(t,history[j - 1]);
                    strcat(t,strchr(com->text,'!') + strlen(val) + 1);

                    build_comand(t, strlen(t) + 1,com,true);
                    return;
                }
            }
            long line = strtol(val,NULL,10);
            if(line > 0 && line <= history_count) {
                char* t = malloc(300);
                strcpy(t,history[line - 1]);
                strcat(t,strchr(com->text,'!') + strlen(val) + 1);

                build_comand(t, strlen(t) + 1,com,true);
                return;
            }
            printf("Unknown paramater: '%s' Only numbers, ! and comand in history are permited. \n",val);
            com->built_in = false;
            return;
        }
        else if(strcmp(token,"exit") == 0){
            kill(0,SIGKILL);
        }
    }
    if(com->tokens[0] != NULL && strcmp(com->tokens[0],"") == 0)
        com->built_in = false;

    if(save && strcmp(com->text,"") != 0)
        try_save_comand(com);
}

void build_comand(char *text, size_t end, struct Comand* com, bool save) {
    init_comand(com);
    tokenize(text,end,com);
    parse_comand(com, save);
}

void print_prompt(){
    char* current_path = malloc(300);
    getcwd(current_path,500);
    write(STDOUT_FILENO,current_path,strlen(current_path));
    write(STDOUT_FILENO," $ ",3);
}

int main(int argc, char const *argv[]){
    signal(SIGINT,handler);
    int p[2];

    while (1) {
        wait(NULL);
        pipe(p);
        print_prompt();

        char text[300];
        ssize_t end = read(STDIN_FILENO, text, 300);
        build_comand(text, end, &comand, true);

        if(comand.in_fd == -1 || comand.out_fd == -1 || subcomand.out_fd == -1)
            continue;

        int in_fd = dup(STDIN_FILENO);
        int out_fd = dup(STDOUT_FILENO);

        int id = fork();
        if (!id) {
            if (comand.type == pip || comand.type == hist_pip)
                dup2(p[1], STDOUT_FILENO);
            else
                dup2(comand.out_fd, STDOUT_FILENO);

            dup2(comand.in_fd, STDIN_FILENO);

            if (comand.built_in) {
                execvp(comand.tokens[0], (char *const *) comand.tokens);
                dup2(in_fd,STDIN_FILENO);
                dup2(out_fd,STDOUT_FILENO);
                write(STDOUT_FILENO,"Unknown comand. \n",18);
                exit(0);
            }
            else if (comand.type == hist || comand.type == hist_pip) {
                int index = 0;
                if (history_count > 50)
                    index = 50 * (history_count / 50 - 1) + (history_count % 50);
                for (int i = 0; i < 50; ++i, ++index) {
                    if (history[index] == NULL)
                        break;
                    printf("%d: %s \n", index + 1, history[index]);
                }
            }
            exit(0);
        }
        if(comand.type == pip || comand.type == hist_pip){
            int fd2 = fork();
            if (!fd2) {
                dup2(subcomand.out_fd, STDOUT_FILENO);
                dup2(p[0], STDIN_FILENO);
                if (subcomand.built_in) {
                    execvp(subcomand.tokens[0], (char *const *) subcomand.tokens);
                    dup2(in_fd,STDIN_FILENO);
                    dup2(out_fd,STDOUT_FILENO);
                    write(STDOUT_FILENO,"Unknown comand. \n",18);
                    exit(0);
                }
            }
        }
    }
}