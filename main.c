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
    int in_fd;
    int out_fd;
    bool built_in;
    char** tokens;
    char* text;
};

char* history[50];
int history_count = 0;
char* path;
struct Comand comand;

void try_save_comand() {
    if (!history_count && comand.text[0] != ' ')
        history[history_count++] = comand.text;
    else if (comand.text[0] != ' ' && strcmp(history[history_count - 1], comand.text) != 0)
        history[history_count++] = comand.text;
}

void compile_comand(char text[], size_t end);

void init_comand(){
    comand.in_fd = STDIN_FILENO;
    comand.out_fd = STDOUT_FILENO;
    comand.text = "";
    comand.type = normal;
    comand.built_in = true;
    comand.tokens = calloc(20, sizeof(char*));
    comand.text = malloc(300);
}

void tokenize(char text[], size_t end){

    strncpy(comand.text,text,end - 1);

    char* text_clone = malloc(strlen(comand.text));
    strcpy(text_clone,comand.text);

    char* token = strtok(text_clone," ");
    comand.tokens[0] = token;

    for (int i = 1; i < 20; ++i) {
        token = strtok(NULL," ");
        comand.tokens[i] = token;
        if(token == NULL)
            break;
    }
};

void parse_comand(){
    for (int i = 0; i < 20; ++i) {
        char* token = comand.tokens[i];

        if(token == NULL)
            break;
        else if(strncmp(token,"<",2) == 0){
            comand.type = cin;

            int file = open(comand.tokens[i + 1],O_RDONLY);
            comand.in_fd = file;
        }
        else if(strncmp(token,">>",2) == 0){
            comand.type = ccout;

            int file = open(comand.tokens[i + 1],O_APPEND);
            comand.out_fd = file;
        }
        else if(strncmp(token,">",1) == 0){
            comand.type = cout;
            comand.tokens[i] = NULL;
            //revisar ................................................................
            int file = open(comand.tokens[i + 1],O_WRONLY,O_TRUNC);
            comand.out_fd = file;
        }
        else if(strncmp(token,"cd",2) == 0){
            comand.type = cd;
            comand.built_in = false;

            chdir(comand.tokens[i + 1]);
            break;
        }
        else if(strncmp(token,"history",7) == 0){
            comand.type = hist;
            comand.built_in = false;
        }
        else if(strncmp(token,"!",1) == 0){

            char* val = strtok(token,"!");

            if(val == NULL){
                compile_comand(history[history_count - 1], strlen(history[history_count - 1]) + 1);
                return;
            }
            for (int j = history_count; j > 0; --j) {
                if(strncmp(history[j - 1],val,strlen(val)) == 0){
                    compile_comand(history[j - 1], strlen(history[j - 1]) + 1);
                    return;
                }
            }
            long line = strtol(val,NULL,10);
            if(line > 0 && line <= history_count) {
                compile_comand(history[line - 1], strlen(history[line - 1]) + 1);
                return;
            }
            comand.type = unknown;
            comand.built_in = false;
            return;
        }
    }
    try_save_comand();
}

void compile_comand(char *text, size_t end) {
    init_comand();
    tokenize(text,end);
    parse_comand();
}

void print_prompt(){
    path = getcwd(path,500);
    write(STDOUT_FILENO,path,strlen(path));
    write(STDOUT_FILENO," $ ",3);
}

int main(int argc, char const *argv[]) {
    while (1) {
        print_prompt();

        char text[300];
        ssize_t end = read(STDIN_FILENO, text, 300);
        compile_comand(text,end);

//        int p[2];
//        pipe(p);

        int id = fork();

        if (!id) {
            dup2(comand.in_fd, STDIN_FILENO);
            dup2(comand.out_fd, STDOUT_FILENO);

            if (comand.built_in) {
                if (execvp(comand.tokens[0], (char *const *) comand.tokens) == -1) {
                    printf("Unknown comand \n");
                }
            }
            else if (comand.type == hist) {
                for (int i = 0; i < 50; ++i) {
                    if (history[i] == NULL)
                        break;
                    printf("%d: ", i + 1);
                    printf("%s \n", history[i]);
                }
            }
            exit(0);
        } else {
            int stat = 0;
            wait(NULL);

//            char buffer[1024];
//            ssize_t stop;

//            if (comand.type != unknown && comand.type != cd)
//                stop = read(comand.out_fd, buffer, 1024);
//
//            if (comand.type == normal || comand.type == hist || comand.type == cin)
//                write(comand.in_fd, buffer, stop);
        }

        if (comand.type == unknown) {
            printf("Unkonwn comand \n");
        }
    }
}

