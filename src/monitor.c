#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>

#define BUFFER_SIZE 2048

// Estrutura para armazenar as informações de execução do programa
typedef struct program_execution{
    pid_t pid;
    char name[BUFFER_SIZE];
    unsigned long long start_time;
} Program_execution;

// Lista ligada para armazenar as informações de execução dos programas
typedef struct Node {
    Program_execution program;
    struct Node* next;
} Node;

unsigned long long getTimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return timestamp;
}

void store_program(const char* pasta, char* nome_programa, int pid, long tempo_execucao) 
{
    char file_name[BUFFER_SIZE];
    snprintf(file_name, sizeof(file_name), "%s/%d.txt", pasta, pid);

    FILE *fd = fopen(file_name, "w"); // abre o ficheiro para escrita

    if (fd == NULL) 
    {
        printf("Erro ao abrir o ficheiro para escrita!\n");
        return;
    }

    fprintf(fd, "%s,%ldms\n", nome_programa, tempo_execucao); // cria uma string com as informações
    fclose(fd); // fecha o ficheiro
}

Node *program_list = NULL;

// Função para adicionar um programa à lista de execução
void add_program(pid_t pid, const char* name, unsigned long long start_time) {
    Node *new_node = (Node*) malloc(sizeof(struct Node));
    if (new_node == NULL) {
        perror("Erro ao alocar memória");
        return;
    }
    new_node->program.pid = pid;
    strcpy(new_node->program.name, name);
    new_node->program.start_time = start_time;
    new_node->next = NULL;

    if (program_list == NULL) {
        program_list = new_node;
    } 
    else {
        Node *current = program_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
}

// Função para remover um programa da lista de execução
void remove_program(pid_t pid) {
    Node *current = program_list;
    Node *previous = NULL;

    while (current != NULL) {
        if (current->program.pid == pid) {
            if (previous == NULL) {
                program_list = current->next;
            }
            else {
                previous->next = current->next;
            }
            free(current);
            break;
        }
        previous = current;
        current = current->next;
    }
}

Program_execution get_program(pid_t pid){
    Node *current = program_list;

    while (current != NULL) {
        if (current->program.pid == pid) {
            return current->program;
        }
        else {
            current = current->next;
        }
    }
}

// Função para calcular o tempo de execução em milissegundos
unsigned long long calculate_execution_time(unsigned long long start_time, unsigned long long end_time) {
    return end_time - start_time;
}



int main(int argc, char const *argv[]){

    char *directory = strdup(argv[1]);
    if (mkfifo("server_pipe", 0666) < 0) {
        perror("Não foi possível criar o fifo server_pipe.\n");
    }
    int server_pipe = open("server_pipe", O_RDONLY);
    char ping[BUFFER_SIZE];


    while (1) {
        Node *current = NULL;

        // Lê o ping enviado pelo cliente
        ssize_t bytes_read = read(server_pipe, ping, BUFFER_SIZE);
        if(bytes_read > 0){
            ping[bytes_read] = '\0';
            printf("%s\n", ping);


            char* token = strtok(ping, ";");
            int flag = atoi(token);

            // Verifica a flag (0 para remover programa terminado, 1 para adicionar)
            if(flag == 2){
                token = strtok(NULL, ";");
                char *pid_name_pipe = strdup(token);
                char client_pipe[20];
                sprintf(client_pipe, "client_pipe_%d", atoi(pid_name_pipe));
                mkfifo(client_pipe, 0666);
                char buffer[BUFFER_SIZE];
                char answer[BUFFER_SIZE];
                answer[0] = '\0';
                current = program_list;
                while(current != NULL){
                    printf("%s\n", ping);
                    unsigned long long end_time = getTimestamp();
                    unsigned long long execution_time = calculate_execution_time(current->program.start_time, end_time);
                    sprintf(buffer, "%s %d %llu;", current->program.name, current->program.pid, execution_time);
                    strcat(answer, buffer);
                    current = current->next;
                }
                int client_pipe_fd = open(client_pipe, O_WRONLY);
                write(client_pipe_fd, answer, strlen(answer));
                close(client_pipe_fd);
                unlink(client_pipe);
            }
        
            else if(flag == 1){
                // Dá parsing do ping do cliente
                token = strtok(NULL, ";");
                pid_t pid = atoi(token);
                token = strtok(NULL, ";");
                char name[BUFFER_SIZE];
                strcpy(name, token);
                token = strtok(NULL, ";");
                unsigned long long start_time = strtoull(token, NULL, 10);

                // Adiciona o programa à lista ligada
                add_program(pid, name, start_time);
            }

            else if (flag == 3) {
                // Dá parsing do ping do cliente
                token = strtok(NULL, ";");
                pid_t pid = atoi(token);

                // inicia a string para armazenar os nomes dos programas
                char names[BUFFER_SIZE];
                names[0] = '\0';

                // Obtém o tempo de início
                token = strtok(NULL, ";");
                unsigned long long start_time = strtoull(token, NULL, 10);

                // Obtém os nomes dos programas
                token = strtok(NULL, ";");
                while (token != NULL) {
                    strcat(names, token);
                    strcat(names, " | ");
                    token = strtok(NULL, ";");  // Obtém o próximo token
                }

                size_t length = strlen(names);
                if (length >= 3) {
                    names[length - 3] = '\0';
                }
                

                // Adiciona o programa à lista ligada
                add_program(pid, names, start_time);
            }

            else if(flag == 0){

                token = strtok(NULL, ";");
                pid_t pid = atoi(token);
                Program_execution p = get_program(pid);
                unsigned long long timestamp_fim = getTimestamp();
                unsigned long long execution_time = calculate_execution_time(p.start_time, timestamp_fim);
                store_program(directory, p.name, pid, execution_time);
        
                // Remove processo que terminou
                remove_program(pid);
            }
        }
    }

    // Encerra o pipe do servidor
    close(server_pipe);

    return 0;
}
