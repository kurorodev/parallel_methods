/* Методы параллельных вычислений Лабораторная работа номер 2
 * Выполинл: Кудрявцев Даниил 3-ИАИТ-103
 */

#include <bits/pthreadtypes.h>
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
  if (func == 0 || x == 0) {
    puts("ERROR TYPE OR X");
    return 0.0;
  }

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
      double x = data->a + random_value * (data->b - data->b);
      partial_sum += calculate_func(data->func, x);
    }
    data->partial_res += (data->b - data->a) * partial_sum / points_per_thread;
    break;
  }
    return NULL;
  }
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

  pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
  thread_data_t *thread_data = malloc(num_threads * sizeof(pthread_t));

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
    pthread_join(threads[i], NULL);
  }

  double total_res = 0.0;

  for (int i = 0; i < num_threads; i++) {
    total_res += thread_data[i].partial_res;
  }

  if (method == TRAPECIA_METHOD) {
    double h = (b - a) / n;

    total_res *= h;
  }

  free(threads);
  free(thread_data);

  return total_res;
}

int main() { return 0; }
