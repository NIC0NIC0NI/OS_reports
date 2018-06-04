#include "synchronize.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

int max_int_2(int a, int b) { return (a > b) ? a : b; }

void print_atomic_info() {
  const char *key[3] = {"never", "sometimes", "always"};
  const int lf_bool = atomic_is_lock_free((atomic_bool *)NULL);
  const int lf_int = atomic_is_lock_free((atomic_uint *)NULL);
  const int lf_GT_tail = atomic_is_lock_free((_Atomic mutex_GT_tail_t *)NULL);
  const int lf_MCS_nodep =
      atomic_is_lock_free((_Atomic mutex_MCS_ownership_ptr_t *)NULL);
  printf("Atomic bool is %s lock-free.\n", key[lf_bool]);
  printf("Atomic int is %s lock-free.\n", key[lf_int]);
  printf("Atomic GT mutex tail structure is %s lock-free.\n", key[lf_GT_tail]);
  printf("Atomic MCS mutex node pointer is %s lock-free.\n", key[lf_MCS_nodep]);
}

void print_help(const char *argv0) {
  printf("USAGE:\n\t%s <#threads> <#repetitions>\n", argv0);
}

typedef struct timeval my_time_t;

void tic(my_time_t *start, pthread_barrier_t *barrier) {
  pthread_barrier_wait(barrier);
  if (thread_current_id() == 0) {
    gettimeofday(start, NULL);
  }
}

void toc(my_time_t *start, pthread_barrier_t *barrier, int repetitions,
         const char *name) {
  pthread_barrier_wait(barrier);
  if (thread_current_id() == 0) {
    my_time_t end;
    double t;
    gettimeofday(&end, NULL);
    t = (double)(end.tv_sec - start->tv_sec) +
        1e-6 * (double)(end.tv_usec - start->tv_usec);
    printf("\t\t%s: total %lfs, average %lfus \n", name, t,
           t / (double)repetitions * 1e9);
  }
}

#ifdef TEST_UNSYNC
/* If `TEST_UNSYNC` is defined, then an assertion failure is expected when
 * running this program */
#define mutex_lock(type, ...)
#define mutex_unlock(type, ...)
#define barrier_wait(type, ...)
#else
#define mutex_lock(type, ...) mutex_lock_##type(__VA_ARGS__)
#define mutex_unlock(type, ...) mutex_unlock_##type(__VA_ARGS__)
#define barrier_wait(type, ...) barrier_wait_##type(__VA_ARGS__)
#endif

typedef pthread_mutex_t mutex_pthread_t;
#define mutex_lock_pthread pthread_mutex_lock
#define mutex_unlock_pthread pthread_mutex_unlock
typedef pthread_barrier_t barrier_pthread_t;
#define barrier_wait_pthread pthread_barrier_wait

#ifdef EMPTY_SECTION
#define CRITICAL_SECTION(test_shared)
#define PARALEL_REGION(test_shared, i, tid)
#define check_shared_for_mutex(repetitions, test_shared)
#define check_shared_for_barrier(repetitions, test_shared)
#else
#define CRITICAL_SECTION(test_shared)                                          \
  {                                                                            \
    int x = (++test_shared[3]);                                                \
    if (x < 1) {                                                               \
      test_shared[1]++;                                                        \
    } else if (x > 1) {                                                        \
      test_shared[2]++;                                                        \
    } else {                                                                   \
      test_shared[0]++;                                                        \
    }                                                                          \
    --test_shared[3];                                                          \
  }

#define PARALEL_REGION(test_shared, i, tid)                                    \
  {                                                                            \
    test_shared[(tid + 1) % 2 + 2 * ((i + 1) % 2)] =                           \
        test_shared[tid % 2 + 2 * (i % 2)] + 1;                                \
  }

bool array_equal(const int *a, const int *b, int n) {
  int i;
  for (i = 0; i < n; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

void check_shared_for_mutex(int repetitions, int *test_shared) {
  if (thread_current_id() == 0) {
    int ref_shared[4] = {thread_total_number() * repetitions, 0, 0, 0};
    assert(array_equal(test_shared, ref_shared, 4));
    memset(test_shared, 0, sizeof(int) * 4);
  }
}

int check_aux(int n, int i) { return n - (n - i) % 2; }

void check_shared_for_barrier(int repetitions, int *test_shared) {
  if (thread_current_id() == 0) {
    int ref_shared[4] = {check_aux(repetitions, 0), check_aux(repetitions, 0),
                         check_aux(repetitions, 1), check_aux(repetitions, 1)};
    assert(array_equal(test_shared, ref_shared, 4));
    memset(test_shared, 0, sizeof(int) * 4);
  }
}

#endif

#define CREATE_MUTEX_TESTER_1(type)                                            \
  \
void test_mutex_##type(mutex_##type##_t *mutex, pthread_barrier_t *barrier,    \
                       int repetitions, int *test_shared) \
{                 \
    my_time_t t;                                                               \
    int i;                                                                     \
    tic(&t, barrier);                                                          \
    for (i = 0; i < repetitions; ++i) {                                        \
      mutex_lock(type, mutex);                                                 \
      CRITICAL_SECTION(test_shared)                                            \
      mutex_unlock(type, mutex);                                               \
    }                                                                          \
    toc(&t, barrier, repetitions, #type);                                      \
  \
}

#define CREATE_MUTEX_TESTER_2(type)                                            \
  \
void test_mutex_##type(mutex_##type##_t *mutex, pthread_barrier_t *barrier,    \
                       int repetitions, int *test_shared) \
{                 \
    my_time_t t;                                                               \
    mutex_##type##_ownership_t ownership;                                      \
    int i;                                                                     \
    tic(&t, barrier);                                                          \
    for (i = 0; i < repetitions; ++i) {                                        \
      mutex_lock(type, mutex, &ownership);                                     \
      CRITICAL_SECTION(test_shared)                                            \
      mutex_unlock(type, mutex, &ownership);                                   \
    }                                                                          \
    toc(&t, barrier, repetitions, #type);                                      \
  \
}

CREATE_MUTEX_TESTER_1(test_and_set)
CREATE_MUTEX_TESTER_1(ticket)
CREATE_MUTEX_TESTER_2(Anderson)
CREATE_MUTEX_TESTER_1(GT)
CREATE_MUTEX_TESTER_2(MCS)
CREATE_MUTEX_TESTER_1(pthread)

#define CREATE_BARRIER_TESTER(type)                                            \
  void test_barrier_##type(barrier_##type##_t *barrier,                        \
                           pthread_barrier_t *barrier_aux, int repetitions,    \
                           int *test_shared) \
{                              \
    my_time_t t;                                                               \
    uint i, tid = thread_current_id();                                         \
    tic(&t, barrier_aux);                                                      \
    for (i = 0; i < repetitions; ++i) {                                        \
      barrier_wait(type, barrier);                                             \
      PARALEL_REGION(test_shared, i, tid)                                      \
    }                                                                          \
    toc(&t, barrier_aux, repetitions, #type);                                  \
  \
}

CREATE_BARRIER_TESTER(centralized)
CREATE_BARRIER_TESTER(combining_tree)
CREATE_BARRIER_TESTER(dissemination)
CREATE_BARRIER_TESTER(tournament)
CREATE_BARRIER_TESTER(dual_tree)
CREATE_BARRIER_TESTER(arrival_tree)
CREATE_BARRIER_TESTER(pthread)

typedef struct {
  int thread_num;
  int repetitions;
  int test_shared[4];
  mutex_test_and_set_t mutex_test_and_set;
  mutex_ticket_t mutex_ticket;
  mutex_Anderson_t mutex_Anderson;
  mutex_GT_t mutex_GT;
  mutex_MCS_t mutex_MCS;
  pthread_mutex_t mutex_pthread;
  barrier_centralized_t barrier_centralized;
  barrier_combining_tree_t barrier_combining_tree;
  barrier_dissemination_t barrier_dissemination;
  barrier_tournament_t barrier_tournament;
  barrier_dual_tree_t barrier_dual_tree;
  barrier_arrival_tree_t barrier_arrival_tree;
  pthread_barrier_t barrier_pthread;
  pthread_barrier_t barrier_aux;
} pthread_subroutine_args_t;

void *pthread_subroutine(void *args) {
  pthread_subroutine_args_t *obj = (pthread_subroutine_args_t *)args;
  thread_init(obj->thread_num);
  if (thread_current_id() == 0) {
    memset(obj->test_shared, 0, sizeof(int) * 4);
    puts("\tTesting mutexes...");
  }
  test_mutex_test_and_set(&obj->mutex_test_and_set, &obj->barrier_aux,
                          obj->repetitions, obj->test_shared);
  check_shared_for_mutex(obj->repetitions, obj->test_shared);
  test_mutex_ticket(&obj->mutex_ticket, &obj->barrier_aux, obj->repetitions,
                    obj->test_shared);
  check_shared_for_mutex(obj->repetitions, obj->test_shared);
  test_mutex_Anderson(&obj->mutex_Anderson, &obj->barrier_aux, obj->repetitions,
                      obj->test_shared);
  check_shared_for_mutex(obj->repetitions, obj->test_shared);
  test_mutex_GT(&obj->mutex_GT, &obj->barrier_aux, obj->repetitions,
                obj->test_shared);
  check_shared_for_mutex(obj->repetitions, obj->test_shared);
  test_mutex_MCS(&obj->mutex_MCS, &obj->barrier_aux, obj->repetitions,
                 obj->test_shared);
  check_shared_for_mutex(obj->repetitions, obj->test_shared);
#ifdef TEST_PTHREAD
  test_mutex_pthread(&obj->mutex_pthread, &obj->barrier_aux, obj->repetitions,
                     obj->test_shared);
  check_shared_for_mutex(obj->repetitions, obj->test_shared);
#endif

  if (thread_current_id() == 0) {
    memset(obj->test_shared, 0, sizeof(int) * obj->thread_num);
    puts("\tTesting barriers...");
  }
  test_barrier_centralized(&obj->barrier_centralized, &obj->barrier_aux,
                           obj->repetitions, obj->test_shared);
  check_shared_for_barrier(obj->repetitions, obj->test_shared);
  test_barrier_combining_tree(&obj->barrier_combining_tree, &obj->barrier_aux,
                              obj->repetitions, obj->test_shared);
  check_shared_for_barrier(obj->repetitions, obj->test_shared);
  test_barrier_dissemination(&obj->barrier_dissemination, &obj->barrier_aux,
                             obj->repetitions, obj->test_shared);
  check_shared_for_barrier(obj->repetitions, obj->test_shared);
  test_barrier_tournament(&obj->barrier_tournament, &obj->barrier_aux,
                          obj->repetitions, obj->test_shared);
  check_shared_for_barrier(obj->repetitions, obj->test_shared);
  test_barrier_dual_tree(&obj->barrier_dual_tree, &obj->barrier_aux,
                         obj->repetitions, obj->test_shared);
  check_shared_for_barrier(obj->repetitions, obj->test_shared);
  test_barrier_arrival_tree(&obj->barrier_arrival_tree, &obj->barrier_aux,
                            obj->repetitions, obj->test_shared);
  check_shared_for_barrier(obj->repetitions, obj->test_shared);
#ifdef TEST_PTHREAD
  test_barrier_pthread(&obj->barrier_pthread, &obj->barrier_aux,
                       obj->test_shared);
  check_shared_for_barrier(obj->repetitions, obj->test_shared);
#endif
  return NULL;
}

int parallel_execute(void *(*routine)(void *), void *args, int thread_num) {
  int created_tnum = 0;
  int i;
  pthread_t *threads =
      (pthread_t *)malloc(sizeof(pthread_t) * (thread_num - 1));
  if (threads == NULL) {
    return -1;
  }

  for (i = 0; i < thread_num - 1; ++i) {
    if (pthread_create(&threads[i], NULL, routine, args) != 0) {
      break;
    }
  }
  created_tnum = i;
  routine(args);
  for (i = 0; i < created_tnum; ++i) {
    pthread_join(threads[i], NULL);
  }

  free(threads);
  return (thread_num - 1 == created_tnum) ? 0 : -2;
}

int main(int argc, char **argv) {
  int i;
  pthread_subroutine_args_t obj;
  /* print_atomic_info(); */
  if (argc > 2) {
    int t_num = atoi(argv[1]);
    int repetitions = atoi(argv[2]);
    if (t_num > 0) {
      int retval;
      obj.thread_num = t_num;
      obj.repetitions = repetitions;
      printf("Testing with %d threads, %d repetitions...\n", t_num,
             repetitions);

      pthread_barrier_init(&obj.barrier_aux, NULL, t_num);
      pthread_barrier_init(&obj.barrier_pthread, NULL, t_num);
      barrier_init_centralized(&obj.barrier_centralized, t_num);
      barrier_init_combining_tree(&obj.barrier_combining_tree, t_num);
      barrier_init_dissemination(&obj.barrier_dissemination, t_num);
      barrier_init_tournament(&obj.barrier_tournament, t_num);
      barrier_init_dual_tree(&obj.barrier_dual_tree, t_num);
      barrier_init_arrival_tree(&obj.barrier_arrival_tree, t_num);
      mutex_init_ticket(&obj.mutex_ticket);
      pthread_mutex_init(&obj.mutex_pthread, NULL);
      mutex_init_test_and_set(&obj.mutex_test_and_set);
      mutex_init_ticket(&obj.mutex_ticket);
      mutex_init_Anderson(&obj.mutex_Anderson, t_num);
      mutex_init_GT(&obj.mutex_GT, t_num);
      mutex_init_MCS(&obj.mutex_MCS);

      retval = parallel_execute(pthread_subroutine, (void *)&obj, t_num);

      mutex_destroy_Anderson(&obj.mutex_Anderson);
      mutex_destroy_GT(&obj.mutex_GT);
      pthread_mutex_destroy(&obj.mutex_pthread);
      barrier_destroy_combining_tree(&obj.barrier_combining_tree);
      barrier_destroy_centralized(&obj.barrier_centralized);
      barrier_destroy_dissemination(&obj.barrier_dissemination);
      barrier_destroy_tournament(&obj.barrier_tournament);
      barrier_destroy_dual_tree(&obj.barrier_dual_tree);
      barrier_destroy_arrival_tree(&obj.barrier_arrival_tree);
      pthread_barrier_destroy(&obj.barrier_pthread);
      pthread_barrier_destroy(&obj.barrier_aux);
      return retval;
    } else {
      print_help(argv[0]);
      return -3;
    }
  } else {
    print_help(argv[0]);
    return -3;
  }
}
