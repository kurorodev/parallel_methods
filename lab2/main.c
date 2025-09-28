/* Методы параллельных вычислений Лабораторная работа номер 2
 * Выполинл: Кудрявцев Даниил 3-ИАИТ-103
 */

#include <math.h>
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
#define MAX_THREADS 100

/* Макросы функций */
#define POLYNOMIAL(x) ((x) * (x) + 3 * (x) + 2)
#define OSCILLATORY(x) (sin(x) * exp(-(x) / 5.0))
#define LOGARITHMIC(x) (log(1 + (x)))
#define GAUSSIAN(x) (exp(-(x) * (x)))

typedef enum {
  REACTANGLE_METHOD,
  TRAPECIA_METHOD,
  MONTE_CARLO_METHOD,
} method_t;

typedef enum {
  POLYNOMIAL_FUNC,
  OSCILLATORY_FUNC,
  LOGARITHMIC_FUNC,
  GAUSSIAN_FUNC,
} func_t;

typedef struct {
  func_t func;
  method_t method;
  double a;
  double b;
  int n;
  int thread_id;
  int num_threads;
  double partial_res;
  int *random_seed;
} thread_data_t;

double calculate_func(func_t func, double x) {
  switch (func) {
  case POLYNOMIAL_FUNC:
    return POLYNOMIAL(x);
  case OSCILLATORY_FUNC:
    return OSCILLATORY(x);
  case LOGARITHMIC_FUNC:
    return LOGARITHMIC(x);
  case GAUSSIAN_FUNC:
    return GAUSSIAN(x);
  default:
    return 0.0;
  }
}

void *thread_function(void *arg) {
  thread_data_t *data = (thread_data_t *)arg;

  double partial_sum = 0;

  int points_per_thread = data->n / data->num_threads;
  int start_index = data->thread_id * points_per_thread;
  int end_index = (data->thread_id == data->num_threads - 1)
                      ? data->n
                      : start_index + points_per_thread;

  switch (data->method) {
  case REACTANGLE_METHOD: {
    double h = (data->b - data->a) / data->n;
    for (int i = start_index; i < end_index; i++) {
      double x = data->a + (i + 0.5) * h;
      partial_sum += calculate_func(data->func, x);
    }
    data->partial_res = partial_sum * h;
    break;
  }
  case TRAPECIA_METHOD: {
    double h = (data->b - data->a) / data->n;

    if (data->thread_id == 0) {
      partial_sum += 0.5 * calculate_func(data->func, data->a);
    }
    if (data->thread_id == data->num_threads - 1) {
      partial_sum += 0.5 * calculate_func(data->func, data->b);
    }
    for (int i = start_index; i < end_index; i++) {
      if (i > 0 && i < data->n) {
        double x = data->a + i * h;
        partial_sum += calculate_func(data->func, x);
      }
    }
    data->partial_res += partial_sum * h;
    break;
  }
  case MONTE_CARLO_METHOD: {
    unsigned int seed = (unsigned int)(time NULL) + data->thread_id;
    for (int i = 0; i < points_per_thread; i++) {
      double random_value = (double)rand_r(&seed) / RAND_MAX;
      double x = data->a + random_value * (data->b - data->a);
      partial_sum += calculate_func(data->func, x);
    }
    data->partial_res = partial_sum;
    break;
  }
  }
  return NULL;
}

double rectangle_method(func_t func, double a, double b, int n) {
  double h = (b - a) / n;

  double sum = 0;

  for (int i = 0; i < n; i++) {
    double x = a + (i + 0.5) * h;
    sum += calculate_func(func, x);
  }

  return sum * h;
}

double trapecia_method(func_t func, double a, double b, int n) {
  double h = (b - a) / n;

  double sum = 0.5 * (calculate_func(func, a) + calculate_func(func, b));

  for (int i = 0; i < n; i++) {
    double x = a + i * h;
    sum += calculate_func(func, x);
  }
  return sum * h;
}

double monte_carlo_method(func_t func, double a, double b, int n) {
  double sum = 0;

  for (int i = 0; i < n; i++) {
    double random_seed = (double)rand() / RAND_MAX;
    double x = a + random_seed * (b - a);
    sum += calculate_func(func, x);
  }
  return (b - a) * sum / n;
}

double single_thread_calc(func_t func, method_t method, double a, double b,
                          int n) {
  switch (method) {
  case REACTANGLE_METHOD:
    return rectangle_method(func, a, b, n);
  case TRAPECIA_METHOD:
    return trapecia_method(func, a, b, n);
  case MONTE_CARLO_METHOD:
    return monte_carlo_method(func, a, b, n);
  default:
    return 0.0;
  }
}

double multi_thread_calc(func_t func, method_t method, double a, double b,
                         int n, int num_threads) {
  if (num_threads <= 1) {
    return single_thread_calc(func, method, a, b, n);
  }

  pthread_t threads[MAX_THREADS];
  thread_data_t thread_data[MAX_THREADS];
  for (int i = 0; i < num_threads; i++) {
    thread_data[i].func = func;
    thread_data[i].method = method;
    thread_data[i].num_threads = num_threads;
    thread_data[i].a = a;
    thread_data[i].b = b;
    thread_data[i].thread_id = i;
    thread_data[i].n = n;
    thread_data[i].partial_res = 0.0;
  }

  for (int i = 0; i < num_threads; i++) {
    pthread_create(&threads[i], NULL, thread_function, &thread_data[i]);
  }

  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      perror("ERROR JOIN THREAD");
      exit(1);
    };
  }

  double total_res = 0.0;

  for (int i = 0; i < num_threads; i++) {
    total_res += thread_data[i].partial_res;
  }

  if (method == MONTE_CARLO_METHOD) {
    total_res = (b - a) * total_res / n;
  }

  return total_res;
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

  method_t method;
  func_t func;
  int threads, n;
  double diff, a, b, result_single, result_multi, execution_single_time,
      execution_multi_time, difference;
  clock_t start, end;

  sscanf(body_start,
         "{\"method\":%d,\"func\":%d,\"n\":%d,\"threads\":%d,\"a\":%lf, "
         "\"b\":%lf}",
         &method, &func, &n, &threads, &a, &b);

  if (threads <= 0) {
    threads = 4;
  }
  if (threads > MAX_THREADS) {
    threads = MAX_THREADS;
  }
  printf("Функция: %d, Метод: %d, Потоков: %d, Интервалов: %d\n", func, method,
         threads, n);

  // Однопоточное выполнение
  start = clock();
  result_single = single_thread_calc(func, method, a, b, n);
  end = clock();
  execution_single_time = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Однопоточный результат: %.6f, Время: %.6f сек\n", result_single,
         execution_single_time);

  // Многопоточное выполнение
  start = clock();
  result_multi = multi_thread_calc(func, method, a, b, n, threads);
  end = clock();
  execution_multi_time = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Многопоточный результат: %.6f, Время: %.6f сек\n", result_multi,
         execution_multi_time);

  difference = execution_multi_time - execution_single_time;
  printf("Разница во времени: %lf\n", difference);
  printf("---\n");

  char response[1024];
  snprintf(response, sizeof(response),
           "{\"difference\":%lf, \"method\":%d, \"threads\":%d, "
           "\"result_single\":%lf, \"result_multi\":%lf, "
           "\"execution_single_time\":%lf,\"execution_multi_time\"%lf}",
           difference, method, threads, result_single, result_multi,
           execution_single_time, execution_multi_time);
  send_json_response(socket, response);
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
