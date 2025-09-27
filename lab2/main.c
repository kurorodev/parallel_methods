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

int main() { return 0; }
