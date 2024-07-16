#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>

#define BUFFER_SIZE 2048
#define MAX_PROGRAMS 10
#define MAX_ARGS 10

unsigned long long getTimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return timestamp;
}


void process_command(char *command, char *flag, char ***programs) {

    unsigned long long start_time = getTimestamp();

    // Verifica se o comando é o "execute"
    if (strcmp(command, "execute") == 0 && strcmp(flag, "-u") == 0) {
        // Cria um novo processo filho
        pid_t pid = fork();

        // Verifica se o processo foi criado
        if (pid < 0) {
            perror("ERRO AO CRIAR PROCESSO");
            exit(1);
        } 
        
        // Executa o "execute"
        else if (pid == 0) {

            pid_t child_pid = getpid();
            printf("Running PID %d\n", child_pid);
            int server_pipe = open("server_pipe", O_WRONLY);
            if (server_pipe < 0) {
                perror("ERRO AO ABRIR O PIPE DO SERVER");
                exit(1);
            }

            char process_start_notify_server[BUFFER_SIZE];
            snprintf(process_start_notify_server, BUFFER_SIZE, "1;%d;%s;%llu", child_pid, programs[0][0], start_time);
            if (write(server_pipe, process_start_notify_server, strlen(process_start_notify_server)) < 1) {
                perror("ERRO AO ENVIAR NOTIFICAÇÃO DE INÍCIO AO SERVIDOR");
                exit(1);
            }
            close(server_pipe);

            // Executa o programa
            execvp(programs[0][0], programs[0]);
        }
        int status;
        wait(&status);


        unsigned long long end_time = getTimestamp();
        unsigned long long execution_time = end_time - start_time;
        printf("Ended in %llu ms\n", execution_time);

        // Notifica o servidor do fim do programa
        int server_pipe = open("server_pipe", O_WRONLY);
        if (server_pipe < 0) {
            perror("ERRO AO ABRIR O PIPE DO SERVER");
            exit(1);
        }
        char process_end_notify_server[BUFFER_SIZE];
        snprintf(process_end_notify_server, BUFFER_SIZE, "0;%d;%llu", pid, end_time);
        if (write(server_pipe, process_end_notify_server, strlen(process_end_notify_server)) < 1) {
            perror("ERRO AO ENVIAR NOTIFICAÇÃO DE FIM AO SERVIDOR");
            exit(1);
        }
        close(server_pipe);
    }

    else if(strcmp(command, "execute") == 0 && strcmp(flag, "-p") == 0){
     // Cria um novo processo filho
    pid_t pid = fork();
    // Verifica se o processo foi criado
    if (pid < 0) {
        perror("ERRO AO CRIAR PROCESSO");
        exit(1);
    } else if (pid == 0) {
        pid_t child_pid = getpid();  // Armazena o PID do processo filho em uma variável separada
        printf("Running PID %d\n", child_pid);  // Imprime o PID do processo filho
        int server_pipe = open("server_pipe", O_WRONLY);
        if (server_pipe < 0) {
            perror("ERRO AO ABRIR O PIPE DO SERVER");
            exit(1);
        }

        int num_programs = 0;
        while (programs[num_programs] != NULL) {
            num_programs++;
        }
        printf("num_progs: %d\n", num_programs);

        start_time = getTimestamp();

        // Notifica o servidor do início da pipeline
        char pipeline_start_ping_server[BUFFER_SIZE];
        snprintf(pipeline_start_ping_server, BUFFER_SIZE, "3;%d", child_pid);
        for (int j = 0; j < num_programs; j++) {
            strcat(pipeline_start_ping_server, ";");
            strcat(pipeline_start_ping_server, programs[j][0]);
        }
        strcat(pipeline_start_ping_server, ";");
        char temp[BUFFER_SIZE];
        sprintf(temp, "%lld", start_time);
        strcat(pipeline_start_ping_server, temp);

        if (write(server_pipe, pipeline_start_ping_server, strlen(pipeline_start_ping_server)) < 1) {
            perror("ERRO AO ENVIAR NOTIFICAÇÃO DE INÍCIO DA PIPELINE AO SERVIDOR");
            exit(1);
        }

        int pipe_fds[num_programs - 1][2];

        // Cria os pipes para a comunicação entre os programas da pipeline
        for (int i = 0; i < num_programs - 1; i++) {
            if (pipe(pipe_fds[i]) < 0) {
                perror("ERRO AO CRIAR PIPE");
                exit(1);
            }
        }

        // Executa os programas da pipeline
        for (int i = 0; i < num_programs; i++) {
            pid_t child_pid = fork();
            if (child_pid < 0) {
                perror("ERRO AO CRIAR PROCESSO");
                exit(1);
            } else if (child_pid == 0) {
                if (i == 0) {
                    close(pipe_fds[0][0]);
                    dup2(pipe_fds[0][1], STDOUT_FILENO);
                    close(pipe_fds[0][1]);
                    execvp(programs[0][0], programs[i]);
                    _exit(1);
                }

            if (i > 0 && i < num_programs - 1) {
                close(pipe_fds[i - 1][1]);
                dup2(pipe_fds[i - 1][0], STDIN_FILENO);
                close(pipe_fds[i - 1][0]);
                close(pipe_fds[i][0]);
                dup2(pipe_fds[i][1], STDOUT_FILENO);
                close(pipe_fds[i][1]);
                execvp(programs[i][0], programs[i]);
                _exit(1);
            }

            if (i == num_programs) {
                close(pipe_fds[num_programs - 1][1]);
                dup2(pipe_fds[num_programs - 1][0], STDIN_FILENO);
                close(pipe_fds[num_programs - 1][0]);
                execvp(programs[num_programs-1][0], programs[num_programs-1]);
                _exit(1);
            }

                // Fecha os pipes utilizados pelo programa
                for (int j = 0; j < num_programs - 1; j++) {
                    close(pipe_fds[j][0]);
                    close(pipe_fds[j][1]);
                }               
                // Executa o programa
                execvp(programs[i][0], programs[i]);
                perror("ERRO AO EXECUTAR O PROGRAMA");
                exit(1);
            }
        }
        

        // Fecha os pipes não utilizados pelo processo atual
        for (int i = 0; i < num_programs - 1; i++) {
            close(pipe_fds[i][0]);
            close(pipe_fds[i][1]);
        }

        // Aguarda a terminação dos processos filhos
        for (int i = 0; i < num_programs; i++) {
            wait(NULL);
        }

        // Notifica o servidor do fim da pipeline
        unsigned long long end_time = getTimestamp();
        char pipeline_end[BUFFER_SIZE];
        snprintf(pipeline_end, BUFFER_SIZE, "0;%d;%llu", child_pid, end_time);
        if (write(server_pipe, pipeline_end, strlen(pipeline_end)) < 1) {
            perror("ERRO AO ENVIAR NOTIFICAÇÃO DE FIM DA PIPELINE AO SERVIDOR");
            exit(1);
        }

        // Calcula e imprime o tempo de execução da pipeline
        unsigned long long execution_time = end_time - start_time;
        printf("Ended in %llu ms\n", execution_time);

        exit(0);
    }
}






    else if(strcmp(command, "status") == 0) {
        int server_pipe = open("server_pipe", O_WRONLY);
        pid_t pid1 = getpid();
        char flag[BUFFER_SIZE];
        sprintf(flag, "2;%d", pid1);
        
        write(server_pipe, flag, strlen(flag));
        close(server_pipe);

        char client_pipe[20];
        sprintf(client_pipe, "client_pipe_%d", pid1);
        mkfifo(client_pipe, 0666);
        int client_pipe_fd = open(client_pipe, O_RDONLY);
        char buffer[BUFFER_SIZE];
        read(client_pipe_fd, buffer, BUFFER_SIZE);
        close(client_pipe_fd);
        unlink(client_pipe);

        char *token;
        token = strtok(buffer, ";");
        while (token != NULL) {
            char line[BUFFER_SIZE];
            snprintf(line, BUFFER_SIZE, "%s ms\n", token);
            write(STDOUT_FILENO, line, strlen(line));
            token = strtok(NULL, ";");
        }

        
    }
}

char ***parse(const char *program_string) {
    char ***programs_array = (char ***)malloc(MAX_PROGRAMS * sizeof(char **));
    int program_count = 0;
    int arg_count = 0;
    int i = 0;
    int j = 0;

    char current_char;
    char *current_token;

    for (i = 0; i < MAX_PROGRAMS; i++) {
        programs_array[i] = (char **)malloc(MAX_ARGS * sizeof(char *));
        for (j = 0; j < MAX_ARGS; j++) {
            programs_array[i][j] = NULL;
        }
    }

    i = 0;
    j = 0;

    while (program_string[i] != '\0' && program_count < MAX_PROGRAMS) {
        current_char = program_string[i];

        if (current_char == '|') {
            // final do programa
            programs_array[program_count][arg_count] = NULL;
            program_count++;
            arg_count = 0;
            i++;
            continue;
        } else if (current_char == ' ') {
            i++;
            continue;
        }

        // procura nova palavra
        current_token = (char *)malloc(MAX_ARGS * sizeof(char));
        j = 0;
        while (program_string[i] != ' ' && program_string[i] != '|' && program_string[i] != '\0') {
            current_token[j] = program_string[i];
            i++;
            j++;
        }
        current_token[j] = '\0';

        // tira aspas
        if (current_token[0] == '"' && current_token[j - 1] == '"') {
            current_token[j - 1] = '\0';
            current_token++;
        }

        programs_array[program_count][arg_count] = strdup(current_token);
        arg_count++;

        free(current_token);
    }

    // final do último programa
    programs_array[program_count][arg_count] = NULL;

    // final do array de programas
    programs_array[program_count + 1] = NULL;

    return programs_array;
}

int main(int argc, char const *argv[]){


    // if (mkfifo("server_pipe", 0666) < 0) {
    //     perror("Não foi possível criar o fifo server_pipe.\n");
    // }

    char *command = strdup(argv[1]);
    char *flag;
    char *programs_string;

    if(strcmp(command, "execute") == 0){

        flag = strdup(argv[2]);
        programs_string = strdup(argv[3]);

    }
    else {
        flag = NULL;
    }

    char ***programs_array = parse(programs_string);
    
    
    //int i = 0, j = 0, k = 0;
    // while (programs_array[i] != NULL) {
    //     j = 0;

    //     while (programs_array[i][j] != NULL) {
    //         printf("%s\n", programs_array[i][j]);
    //         j++;
    //     }

    //     i++;
    // }


    process_command(command, flag, programs_array);
    
    //unlink("server_pipe");
    return 0;
}