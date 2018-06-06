#ifndef SYNCHRONIZE_H_INCLUDED
#define SYNCHRONIZE_H_INCLUDED 1

#include <stdatomic.h>
#include <stdbool.h>

#define delay(time)

#define CACHE_LINE_SIZE 64

typedef unsigned int uint;

#define AVOID_FALSE_SHARING(original_type, padded_type)                        \
  typedef struct AVOID_FALSE_SHARING_##original_type {                         \
    original_type value;                                                       \
    char padding[CACHE_LINE_SIZE - sizeof(original_type)];                     \
  } padded_type;

AVOID_FALSE_SHARING(bool, padded_bool_t)
AVOID_FALSE_SHARING(atomic_bool, padded_abool_t)

#define LIB_INIT_INVALID -1
#define OUT_OF_MEMORY -2
#define SUCCESS 0

/* Atomics */
#define ATOMIC_LOAD(x_) atomic_load_explicit((x_), memory_order_relaxed)
#define ATOMIC_STORE(x_, v_)                                                   \
  atomic_store_explicit((x_), (v_), memory_order_relaxed)
#define ATOMIC_ACQUIRE(x_) atomic_load_explicit((x_), memory_order_acquire)
#define ATOMIC_RELEASE(x_, v_)                                                 \
  atomic_store_explicit((x_), (v_), memory_order_release)
#define ATOMIC_EXCHANGE(x_, v_)                                                \
  atomic_exchange_explicit((x_), (v_), memory_order_relaxed)
#define ATOMIC_ACQUIRE_EXCHANGE(x_, v_)                                        \
  atomic_exchange_explicit((x_), (v_), memory_order_acquire)
#define ATOMIC_ADD(x_, v_)                                                     \
  atomic_fetch_add_explicit((x_), (v_), memory_order_relaxed)
#define ATOMIC_SUB(x_, v_)                                                     \
  atomic_fetch_sub_explicit((x_), (v_), memory_order_relaxed)
#define ATOMIC_COMPARE_EXCHANGE_RELEASE(x_, e_, v_)                            \
  atomic_compare_exchange_strong_explicit(x_, e_, v_, memory_order_release,    \
                                          memory_order_relaxed)

/* Mutex types declaration */

typedef struct { atomic_bool locked; } mutex_test_and_set_t;

typedef struct { atomic_uint new_ticket, now_serving; } mutex_ticket_t;

typedef struct {
  padded_abool_t *slots;
  atomic_uint next_slot;
  uint mask;
} mutex_Anderson_t;

typedef struct { uint my_place; } mutex_Anderson_ownership_t;

typedef struct {
  uint id;
  bool locked;
} mutex_GT_tail_t;

typedef struct {
  padded_abool_t *slots;
  _Atomic mutex_GT_tail_t tail;
} mutex_GT_t;

typedef struct QNODE mutex_MCS_ownership_t;
typedef mutex_MCS_ownership_t *mutex_MCS_ownership_ptr_t;

struct QNODE {
  _Atomic mutex_MCS_ownership_ptr_t next;
  atomic_bool locked;
};

typedef struct { _Atomic mutex_MCS_ownership_ptr_t tail; } mutex_MCS_t;

typedef struct {
  uint my_id;
  uint watching;
} mutex_CLH_state_t;

AVOID_FALSE_SHARING(mutex_CLH_state_t, padded_CLH_state_t)

typedef struct {
  padded_abool_t *slots;
  padded_CLH_state_t *states;
  atomic_uint tail;
} mutex_CLH_t;

/* Mutex routines declaration */

int mutex_init_test_and_set(mutex_test_and_set_t *mutex);
void mutex_lock_test_and_set(mutex_test_and_set_t *mutex);
void mutex_unlock_test_and_set(mutex_test_and_set_t *mutex);

int mutex_init_ticket(mutex_ticket_t *mutex);
void mutex_lock_ticket(mutex_ticket_t *mutex);
void mutex_unlock_ticket(mutex_ticket_t *mutex);

int mutex_init_Anderson(mutex_Anderson_t *mutex, uint t_num);
void mutex_destroy_Anderson(mutex_Anderson_t *mutex);
void mutex_lock_Anderson(mutex_Anderson_t *mutex,
                         mutex_Anderson_ownership_t *onwership);
void mutex_unlock_Anderson(mutex_Anderson_t *mutex,
                           mutex_Anderson_ownership_t *onwership);

int mutex_init_GT(mutex_GT_t *mutex, uint t_num);
void mutex_destroy_GT(mutex_GT_t *mutex);
void mutex_lock_GT(mutex_GT_t *mutex);
void mutex_unlock_GT(mutex_GT_t *mutex);

int mutex_init_MCS(mutex_MCS_t *mutex);
void mutex_lock_MCS(mutex_MCS_t *mutex, mutex_MCS_ownership_t *ownership);
void mutex_unlock_MCS(mutex_MCS_t *mutex, mutex_MCS_ownership_t *ownership);

int mutex_init_CLH(mutex_CLH_t *mutex, uint t_num);
void mutex_destroy_CLH(mutex_CLH_t *mutex);
void mutex_lock_CLH(mutex_CLH_t *mutex);
void mutex_unlock_CLH(mutex_CLH_t *mutex);

/* Barrier types declaration */

#define COMBINING_TREE_FAN_IN 4
#define DUAL_TREE_FAN_IN 4
#define DUAL_TREE_FAN_OUT 4

typedef struct {
  padded_bool_t *local_sense;
  uint thread_num;
  atomic_uint count;
  atomic_bool sense;
} barrier_centralized_t;

typedef struct CT_NODE {
  struct CT_NODE *parent;
  uint fan_in;
  atomic_uint count;
  atomic_bool sense;
} combining_tree_node_t;

AVOID_FALSE_SHARING(combining_tree_node_t, padded_combining_tree_node_t)

typedef struct {
  padded_combining_tree_node_t *nodes;
  padded_bool_t *local_sense;
} barrier_combining_tree_t;

typedef struct {
  atomic_bool *my_flags;
  atomic_bool **partner_flags;
  uint parity;
  bool sense;
} dissemination_flags_t;

AVOID_FALSE_SHARING(dissemination_flags_t, padded_dissemination_flags_t)

typedef struct {
  padded_dissemination_flags_t *flags;
  uint thread_num, log_thread_num;
} barrier_dissemination_t;

typedef struct {
  atomic_bool *my_flags;
  atomic_bool **opponent_flags;
  char *roles;
  bool sense;
} tournament_flags_t;

#define WINNER 'W'
#define LOSER 'L'
#define BYE 'B'
#define CHAMPION 'C'

AVOID_FALSE_SHARING(tournament_flags_t, padded_tournament_flags_t)

typedef struct {
  padded_tournament_flags_t *flags;
  uint log_thread_num, thread_num;
} barrier_tournament_t;

typedef struct {
  atomic_bool *fan_out_child_flags[DUAL_TREE_FAN_OUT];
  atomic_bool *fan_in_parent_flag;
  atomic_bool fan_out_parent_sense;
  bool have_fan_in_child[DUAL_TREE_FAN_IN];
  atomic_bool fan_in_child_not_ready[DUAL_TREE_FAN_IN];
  bool local_sense;
} dual_tree_node_t;

AVOID_FALSE_SHARING(dual_tree_node_t, padded_dual_tree_node_t)

typedef struct { padded_dual_tree_node_t *nodes; } barrier_dual_tree_t;

typedef struct {
  atomic_bool *fan_in_parent_flag;
  bool have_fan_in_child[DUAL_TREE_FAN_IN];
  atomic_bool fan_in_child_not_ready[DUAL_TREE_FAN_IN];
  bool local_sense;
} arrival_tree_node_t;

AVOID_FALSE_SHARING(arrival_tree_node_t, padded_arrival_tree_node_t)

typedef struct {
  padded_arrival_tree_node_t *nodes;
  atomic_bool sense;
} barrier_arrival_tree_t;

int barrier_init_centralized(barrier_centralized_t *barrier, uint t_num);
void barrier_destroy_centralized(barrier_centralized_t *barrier);
void barrier_wait_centralized(barrier_centralized_t *barrier);

int barrier_init_combining_tree(barrier_combining_tree_t *barrier, uint t_num);
void barrier_destroy_combining_tree(barrier_combining_tree_t *barrier);
void barrier_wait_combining_tree(barrier_combining_tree_t *barrier);

int barrier_init_dissemination(barrier_dissemination_t *barrier, uint t_num);
void barrier_destroy_dissemination(barrier_dissemination_t *barrier);
void barrier_wait_dissemination(barrier_dissemination_t *barrier);

int barrier_init_tournament(barrier_tournament_t *barrier, uint t_num);
void barrier_destroy_tournament(barrier_tournament_t *barrier);
void barrier_wait_tournament(barrier_tournament_t *barrier);

int barrier_init_dual_tree(barrier_dual_tree_t *barrier, uint t_num);
void barrier_destroy_dual_tree(barrier_dual_tree_t *barrier);
void barrier_wait_dual_tree(barrier_dual_tree_t *barrier);

int barrier_init_arrival_tree(barrier_arrival_tree_t *barrier, uint t_num);
void barrier_destroy_arrival_tree(barrier_arrival_tree_t *barrier);
void barrier_wait_arrival_tree(barrier_arrival_tree_t *barrier);

void thread_init(int thread_num);
uint thread_total_number();
uint thread_current_id();

#endif