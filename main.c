#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <dirent.h>

enum {normal, unknown, cd, cin, cout, ccout, hist};

struct Comand{
    int type;
    bool built_in;
    char** tokens;
    char* text;
    char* file;
};

char* history[50];
int history_count = 0;
char* path;
struct Comand comand;

void init_comand(size_t size){
    comand.text = "";
    comand.file = "";
    comand.type = normal;
    comand.built_in = true;
    comand.tokens = calloc(20, sizeof(char*));
    comand.text = malloc(size);
    comand.file = malloc(strlen(path) + 50);
    strcpy(comand.file,path);
    strcat(comand.file,"/");
}

void try_save_comand() {
    if (!history_count && comand.text[0] != ' ')
        history[history_count++] = comand.text;
    else if (comand.text[0] != ' ' && strcmp(history[history_count - 1], comand.text) != 0)
        history[history_count++] = comand.text;
}

void parse_comand(char text[], size_t end){

    init_comand(end);
    strncpy(comand.text,text,end - 1);

    char* text_clone = malloc(strlen(comand.text));
    strcpy(text_clone,comand.text);

    int subcomands = 0;
    char* token = strtok(text_clone," ");
    comand.tokens[0] = token;
    for (int i = 1; i < 20; ++i) {
        subcomands = i;
        if(token == NULL)
            break;
        else if(strncmp(token,">>",2) == 0){
            comand.type = ccout;
            comand.tokens[i] = NULL;

            char* file = strtok(NULL," ");
            strcat(comand.file,file);
            break;
        }
        else if(strncmp(token,">",1) == 0){
            comand.type = cout;
            comand.tokens[i] = NULL;

            char* file = strtok(NULL," ");
            strcat(comand.file,file);
            break;
        }
        else if(strncmp(token,"cd",2) == 0){
            comand.type = cd;
            comand.built_in = false;

            char* folder = strtok(NULL," ");
            chdir(folder);
        }
        else if(strncmp(token,"history",7) == 0){
            comand.type = hist;
            comand.built_in = false;
        }
        else if(strncmp(token,"!",1) == 0){

            char* val = strtok(token,"!");

            if(val == NULL){
                parse_comand(history[history_count - 1], strlen(history[history_count - 1]) + 1);
                return;
            }
            for (int j = history_count; j > 0; --j) {
                if(strncmp(history[j - 1],val,strlen(val)) == 0){
                    parse_comand(history[j - 1], strlen(history[j - 1]) + 1);
                    return;
                }
            }
            long line = strtol(val,NULL,10);
            if(line > 0 && line <= history_count) {
                parse_comand(history[line - 1], strlen(history[line - 1]) + 1);
                return;
            }
            comand.type = unknown;
            comand.built_in = false;
            return;
        }

        if(i > 1)
            comand.tokens[i - 1] = token;

        token = strtok(NULL," ");
    }
    try_save_comand();
}

int main(int argc, char const *argv[]) {
    while(1) {
        path = getcwd(path,500);
        write(STDOUT_FILENO,path,strlen(path));
        write(STDOUT_FILENO," $ ",3);

        char text[300];
        ssize_t end = read(STDIN_FILENO,text,300);
        parse_comand(text,(size_t)end);

        int p[2];
        pipe(p);

        int id = fork();

        if (!id) {
            dup2(p[1], STDOUT_FILENO);

            if(comand.built_in) {
                if(execvp(comand.tokens[0], (char *const *) comand.tokens) == -1){
                    printf("Unknown comand \n");
                }
            }
            else if(comand.type == hist || comand.type == cout || comand.type == ccout){
                for (int i = 0; i < 50; ++i) {
                    if (history[i] == NULL)
                        break;
                    printf("%d: ", i + 1);
                    printf("%s \n", history[i]);
                }
            }
            exit(0);
        }
        else{
            int stat = 0;
            waitpid(id, &stat, WNOHANG);

            char buffer[1024];
            ssize_t stop;

            if(comand.type != unknown && comand.type != cd)
                stop = read(p[0],buffer,1024);

            if(comand.type == normal || comand.type == hist){
                write(STDOUT_FILENO,buffer, stop);
            }
            else if(comand.type == cout){
                FILE* file;
                file = fopen(comand.file, "w+");
                fwrite(buffer,stop,sizeof(char),file);
                fclose(file);
            }
            else if(comand.type == ccout){
                FILE* file;
                file = fopen(comand.file, "a");
                fwrite(buffer,stop,sizeof(char),file);
                fclose(file);
            }
            else if(comand.type == unknown){
                printf("Unkonwn comand \n");
            }
        }
    }
}
