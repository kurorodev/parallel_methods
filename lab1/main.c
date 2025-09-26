/* Методы параллельных вычислений Лабораторная работа номер 1
 * Выполинл: Кудрявцев Даниил 3-ИАИТ-103
 */

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define MAX_MATRIX_SIZE 1000
#define MAX_THREADS 100

typedef struct {
  int **A;
  int **B;
  int **C;
  int colsA;
  int colsB;
  int start_row;
  int end_row;
} ThreadData;

void generate_matrix(int **matrix, int rows, int cols) {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      matrix[i][j] = rand() % 10;
    }
  }
}

void multiply_sequential(int **A, int **B, int **C, int rowsA, int colsA,
                         int colsB) {
  for (int i = 0; i < rowsA; i++) {
    for (int j = 0; j < colsB; j++) {
      C[i][j] = 0;
      for (int k = 0; k < colsA; k++) {
        C[i][j] += A[i][k] * B[k][j];
      }
    }
  }
}

void *multiply_row_thread(void *arg) {
  ThreadData *data = (ThreadData *)arg;

  for (int i = data->start_row; i < data->end_row; i++) {
    for (int j = 0; j < data->colsB; j++) {
      data->C[i][j] = 0;
      for (int k = 0; k < data->colsA; k++) {
        data->C[i][j] += data->A[i][k] * data->B[k][j];
      }
    }
  }

  pthread_exit(NULL);
}

double get_wall_time() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return (double)time.tv_sec + (double)time.tv_usec * 1e-6;
}

double multiply_parallel(int **A, int **B, int **C, int rowsA, int colsA,
                         int colsB, int thread_count) {
  pthread_t threads[MAX_THREADS];
  ThreadData thread_data[MAX_THREADS];

  int rows_per_thread = rowsA / thread_count;
  int extra_rows = rowsA % thread_count;

  double start = get_wall_time();

  int current_row = 0;
  for (int i = 0; i < thread_count; i++) {
    thread_data[i].A = A;
    thread_data[i].B = B;
    thread_data[i].C = C;
    thread_data[i].colsA = colsA;
    thread_data[i].colsB = colsB;
    thread_data[i].start_row = current_row;

    int rows_this_thread = rows_per_thread + (i < extra_rows ? 1 : 0);
    thread_data[i].end_row = current_row + rows_this_thread;
    current_row += rows_this_thread;

    pthread_create(&threads[i], NULL, multiply_row_thread, &thread_data[i]);
  }

  for (int i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  double end = get_wall_time();
  return end - start;
}

void free_matrix(int **matrix, int rows) {
  for (int i = 0; i < rows; i++) {
    free(matrix[i]);
  }
  free(matrix);
}

int **allocate_matrix(int rows, int cols) {
  int **matrix = malloc(rows * sizeof(int *));
  for (int i = 0; i < rows; i++) {
    matrix[i] = malloc(cols * sizeof(int));
  }
  return matrix;
}

void send_json_response(int socket, const char *json) {
  char headers[2048];
  snprintf(headers, sizeof(headers),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
           "Access-Control-Allow-Headers: Content-Type\r\n"
           "Content-Length: %zu\r\n\r\n%s",
           strlen(json), json);

  send(socket, headers, strlen(headers), 0);
}

void handle_options(int socket) {
  char response[] = "HTTP/1.1 200 OK\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type\r\n"
                    "Content-Length: 0\r\n\r\n";

  send(socket, response, strlen(response), 0);
}

void handle_request(int socket, const char *request) {
  if (strstr(request, "OPTIONS")) {
    handle_options(socket);
    return;

    char json_data[1024];
    snprintf(json_data, sizeof(json_data),
             "{\"sequential_time\":%.6f,\"parallel_time\":%.6f,"
             "\"speedup\":%.2f,\"correct\":%d,"
             "\"rows\":%d,\"cols\":%d,\"threads\":%d}");

    send_json_response(socket, json_data);
  }

  if (!strstr(request, "POST")) {
    send_json_response(socket, "{\"error\":\"Only POST method supported\"}");
    return;
  }

  const char *body_start = strstr(request, "\r\n\r\n");
  if (!body_start) {
    send_json_response(socket, "{\"error\":\"No request body\"}");
    return;
  }
  body_start += 4;

  int rowsA, colsA, rowsB, colsB, threads = 4;

  if (sscanf(body_start,
             "{\"rowsA\":%d,\"colsA\":%d,\"rowsB\":%d,\"colsB\":%d,\"threads\":"
             "%d}",
             &rowsA, &colsA, &rowsB, &colsB, &threads) < 4) {
    send_json_response(socket, "{\"error\":\"Invalid JSON format\"}");
    return;
  }

  if (colsA != rowsB) {
    send_json_response(socket, "{\"error\":\"Matrix dimensions mismatch\"}");
    return;
  }

  if (threads <= 0)
    threads = 4;
  if (threads > MAX_THREADS)
    threads = MAX_THREADS;

  int **A = allocate_matrix(rowsA, colsA);
  int **B = allocate_matrix(rowsB, colsB);
  int **C_seq = allocate_matrix(rowsA, colsB);
  int **C_par = allocate_matrix(rowsA, colsB);

  srand(time(NULL));
  generate_matrix(A, rowsA, colsA);
  generate_matrix(B, rowsB, colsB);

  clock_t start_seq = clock();
  multiply_sequential(A, B, C_seq, rowsA, colsA, colsB);
  clock_t end_seq = clock();
  double time_seq = ((double)(end_seq - start_seq)) / CLOCKS_PER_SEC;

  double time_par =
      multiply_parallel(A, B, C_par, rowsA, colsA, colsB, threads);

  int correct = 1;
  for (int i = 0; i < rowsA && correct; i++) {
    for (int j = 0; j < colsB && correct; j++) {
      if (C_seq[i][j] != C_par[i][j]) {
        correct = 0;
      }
    }
  }

  char response[1024];

  snprintf(response, sizeof(response),
           "{\"sequential_time\":%.6f,\"parallel_time\":%.6f,"
           "\"speedup\":%.2f,\"correct\":%d,"
           "\"rows\":%d,\"cols\":%d,\"threads\":%d}",
           time_seq, time_par, time_seq / time_par, correct, rowsA, colsB,
           threads);

  send_json_response(socket, response);

  free_matrix(A, rowsA);
  free_matrix(B, rowsB);
  free_matrix(C_seq, rowsA);
  free_matrix(C_par, rowsA);
}

int main() {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Сервер запущен на порте: %d\n", PORT);

  while (1) {
    new_socket =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (new_socket < 0) {
      perror("accept");
      continue;
    }

    char buffer[4096] = {0};
    read(new_socket, buffer, sizeof(buffer) - 1);

    handle_request(new_socket, buffer);
    close(new_socket);
  }

  return 0;
}
