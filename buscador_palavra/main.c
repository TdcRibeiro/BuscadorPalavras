#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>

// Variáveis globais
int num_threads;   // Quantidade de threads escolhidas pelo usuário
char *word_to_find;  // Palavra a ser buscada
int total_occurrences_multithread = 0;
int total_occurrences_singlethread = 0;
pthread_mutex_t mutex;  // Mutex para proteger a contagem de ocorrências

// Função para remover pontuação e acentos de uma string
void normalize_string(char* str) {
    char* src = str;
    char* dst = str;

    while (*src) {
        if (!ispunct((unsigned char)*src)) {
            *dst = tolower((unsigned char)*src);
            dst++;
        }
        src++;
    }
    *dst = '\0';
}

// Função para contar ocorrências de uma palavra em um arquivo
int count_word_in_file(FILE* file) {
    int count = 0;
    char buffer[1024];

    while (fscanf(file, " %1023s", buffer) == 1) {
        normalize_string(buffer);
        if (strcmp(buffer, word_to_find) == 0) {
            count++;
        }
    }

    return count;
}

// Função executada por cada thread na versão multi-threaded
void* thread_function(void* arg) {
    char **files = (char**)arg;
    int local_count = 0;

    // Cada thread processa seu conjunto de arquivos
    for (int i = 0; files[i] != NULL; i++) {
        FILE *file = fopen(files[i], "r");
        if (!file) {
            perror("Erro ao abrir o arquivo");
            continue;
        }

        local_count += count_word_in_file(file);
        fclose(file);
    }

    // Atualiza a contagem global de ocorrências de forma protegida
    pthread_mutex_lock(&mutex);
    total_occurrences_multithread += local_count;
    pthread_mutex_unlock(&mutex);

    free(files);  // Libera o subset de arquivos alocado dinamicamente
    pthread_exit(NULL);
}

// Função para processar os arquivos em single-threaded
void single_thread_process(char** files) {
    for (int i = 0; files[i] != NULL; i++) {
        FILE *file = fopen(files[i], "r");
        if (!file) {
            perror("Erro ao abrir o arquivo");
            continue;
        }

        total_occurrences_singlethread += count_word_in_file(file);
        fclose(file);
    }
}

// Função para ler todos os arquivos do diretório
char** get_files_from_directory(const char* dir_name, int* file_count) {
    DIR *dir;
    struct dirent *ent;
    struct stat statbuf;
    char **file_list = NULL;
    *file_count = 0;

    dir = opendir(dir_name);
    if (dir == NULL) {
        perror("Erro ao abrir o diretorio");
        return NULL;
    }

    // Contar número de arquivos no diretório
    while ((ent = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir_name, ent->d_name);
        if (stat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            (*file_count)++;
        }
    }

    // Alocar espaço para os nomes dos arquivos
    file_list = (char**)malloc((*file_count + 1) * sizeof(char*));
    if (!file_list) {
        perror("Erro de memoria");
        closedir(dir);
        return NULL;
    }

    rewinddir(dir);
    int index = 0;
    while ((ent = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir_name, ent->d_name);
        if (stat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            file_list[index] = strdup(path);  // Copia o nome completo do arquivo
            index++;
        }
    }
    file_list[index] = NULL;  // Finaliza a lista com NULL

    closedir(dir);
    return file_list;
}

int main() {
    // Entrada do usuário
    char dir_name[256];
    printf("Digite o caminho do diretorio: ");
    scanf("%s", dir_name);

    printf("Digite a palavra a ser buscada: ");
    word_to_find = (char*)malloc(256);
    scanf("%s", word_to_find);
    normalize_string(word_to_find);  // Normalizar a palavra buscada

    printf("Digite o numero de threads: ");
    scanf("%d", &num_threads);

    // Obter lista de arquivos do diretório
    int file_count = 0;
    char **files = get_files_from_directory(dir_name, &file_count);
    if (!files) {
        return 1;
    }

    // Inicializa o mutex
    pthread_mutex_init(&mutex, NULL);

    // Versão single-threaded
    clock_t start_time_single = clock();
    single_thread_process(files);
    clock_t end_time_single = clock();
    double time_single = (double)(end_time_single - start_time_single) / CLOCKS_PER_SEC;

    // Versão multi-threaded
    clock_t start_time_multi = clock();

    int files_per_thread = (file_count + num_threads - 1) / num_threads;

    // Cria as threads
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        char **file_subset = (char**)malloc((files_per_thread + 1) * sizeof(char*));
        for (int j = 0; j < files_per_thread && *files != NULL; j++) {
            file_subset[j] = *files;
            files++;
        }
        file_subset[files_per_thread] = NULL;  // Finaliza o conjunto da thread
        pthread_create(&threads[i], NULL, thread_function, (void*)file_subset);
    }

    // Aguarda todas as threads terminarem
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_t end_time_multi = clock();
    double time_multi = (double)(end_time_multi - start_time_multi) / CLOCKS_PER_SEC;

    // Destrói o mutex
    pthread_mutex_destroy(&mutex);

    // Cálculo da diferença percentual
    double percent_diff = ((time_single - time_multi) / time_single) * 100;

    // Exibe os resultados
    printf("Versao Single-Thread:\n");
    printf("Total de ocorrencias da palavra '%s': %d\n", word_to_find, total_occurrences_singlethread);
    printf("Tempo de execucao: %.4f segundos\n\n", time_single);

    printf("Versao Multi-Thread:\n");
    printf("Total de ocorrencias da palavra '%s': %d\n", word_to_find, total_occurrences_multithread);
    printf("Tempo de execucao: %.4f segundos\n\n", time_multi);

    // Exibe a diferença percentual de tempo
    if (percent_diff > 0) {
        printf("A versao multi-thread foi %.2f%% mais rapida que a versao single-thread.\n", percent_diff);
    } else {
        printf("A versao multi-thread foi %.2f%% mais lenta que a versao single-thread.\n", -percent_diff);
    }

    // Limpeza
    free(word_to_find);

    return 0;
}
