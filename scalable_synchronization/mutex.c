#include "synchronize.h"
#include <stdlib.h>

static uint ceiling_2(uint n) {
  uint i = 2;
  while (i < n) {
    i = i << 2;
  }
  return i;
}

int mutex_init_test_and_set(mutex_test_and_set_t *mutex) {
  atomic_init(&mutex->locked, false);
  return SUCCESS;
}

void mutex_lock_test_and_set(mutex_test_and_set_t *mutex) {
  while (atomic_exchange_explicit(&mutex->locked, true, memory_order_acquire)) {
    delay(0);
  }
}

void mutex_unlock_test_and_set(mutex_test_and_set_t *mutex) {
  atomic_store_explicit(&mutex->locked, false, memory_order_release);
}

int mutex_init_ticket(mutex_ticket_t *mutex) {
  atomic_init(&mutex->new_ticket, 0);
  atomic_init(&mutex->now_serving, 0);
  return SUCCESS;
}

void mutex_lock_ticket(mutex_ticket_t *mutex) {
  int my_ticket =
      atomic_fetch_add_explicit(&mutex->new_ticket, 1, memory_order_relaxed);
  while (atomic_load_explicit(&mutex->now_serving, memory_order_acquire) !=
         my_ticket) {
    delay(0);
  }
}

void mutex_unlock_ticket(mutex_ticket_t *mutex) {
  int next =
      atomic_load_explicit(&mutex->now_serving, memory_order_relaxed) + 1;
  atomic_store_explicit(&mutex->now_serving, next, memory_order_release);
}

int mutex_init_Anderson(mutex_Anderson_t *mutex, uint thread_num) {
  uint i;
  uint tnum = ceiling_2(thread_num);
  padded_abool_t *slots =
      (padded_abool_t *)malloc(sizeof(padded_abool_t) * tnum);

  if (slots == NULL) {
    return OUT_OF_MEMORY;
  }
  mutex->slots = slots;
  mutex->mask = tnum - 1;

  atomic_init(&slots[0].value, false);
  for (i = 1; i < tnum; ++i) {
    atomic_init(&slots[i].value, true);
  }
  atomic_init(&mutex->next_slot, 0);
  return SUCCESS;
}

void mutex_destroy_Anderson(mutex_Anderson_t *mutex) {
  free(mutex->slots);
  mutex->slots = NULL;
}

void mutex_lock_Anderson(mutex_Anderson_t *mutex,
                         mutex_Anderson_ownership_t *onwership) {
  int my_place =
      atomic_fetch_add_explicit(&mutex->next_slot, 1, memory_order_relaxed) &
      mutex->mask;
  while (atomic_load_explicit(&mutex->slots[my_place].value,
                              memory_order_acquire)) {
    delay(0);
  }
  atomic_store_explicit(&mutex->slots[my_place].value, true,
                        memory_order_relaxed);
  onwership->my_place = my_place;
}

void mutex_unlock_Anderson(mutex_Anderson_t *mutex,
                           mutex_Anderson_ownership_t *onwership) {
  atomic_store_explicit(
      &mutex->slots[(onwership->my_place + 1) & mutex->mask].value, false,
      memory_order_release);
}

int mutex_init_GT(mutex_GT_t *mutex, uint t_num) {
  uint i;
  padded_abool_t *slots;
  mutex_GT_tail_t init_value = {0, false};

  slots = (padded_abool_t *)malloc(sizeof(padded_abool_t) * t_num);
  if (slots == NULL) {
    return OUT_OF_MEMORY;
  }
  mutex->slots = slots;

  for (i = 0; i < t_num; ++i) {
    atomic_init(&slots[i].value, true);
  }
  atomic_init(&mutex->tail, init_value);

  return SUCCESS;
}

void mutex_destroy_GT(mutex_GT_t *mutex) {
  free(mutex->slots);
  mutex->slots = NULL;
}

void mutex_lock_GT(mutex_GT_t *mutex) {
  uint tid = thread_current_id();
  mutex_GT_tail_t current = {tid, atomic_load_explicit(&mutex->slots[tid].value,
                                                       memory_order_relaxed)};
  mutex_GT_tail_t last =
      atomic_exchange_explicit(&mutex->tail, current, memory_order_relaxed);
  while (atomic_load_explicit(&mutex->slots[last.id].value,
                              memory_order_acquire) == last.locked) {
    delay(0);
  }
}

void mutex_unlock_GT(mutex_GT_t *mutex) {
  uint tid = thread_current_id();
  bool value =
      atomic_load_explicit(&mutex->slots[tid].value, memory_order_relaxed);
  atomic_store_explicit(&mutex->slots[tid].value, !value, memory_order_release);
}

int mutex_init_MCS(mutex_MCS_t *mutex) {
  atomic_init(&mutex->tail, NULL);
  return SUCCESS;
}

void mutex_lock_MCS(mutex_MCS_t *mutex, mutex_MCS_ownership_t *ownership) {
  mutex_MCS_ownership_t *predecessor;
  atomic_init(&ownership->next, NULL);
  predecessor =
      atomic_exchange_explicit(&mutex->tail, ownership, memory_order_relaxed);
  if (predecessor != NULL) {
    atomic_init(&ownership->locked, true);
    atomic_store_explicit(&predecessor->next, ownership, memory_order_release);
    while (atomic_load_explicit(&ownership->locked, memory_order_acquire)) {
      delay(0);
    }
  }
}

void mutex_unlock_MCS(mutex_MCS_t *mutex, mutex_MCS_ownership_t *ownership) {
  mutex_MCS_ownership_t *successor;
  if (atomic_load_explicit(&ownership->next, memory_order_relaxed) == NULL) {
    mutex_MCS_ownership_t *expected = ownership;
    if (atomic_compare_exchange_strong_explicit(&mutex->tail, &expected, NULL,
                                                memory_order_release,
                                                memory_order_relaxed)) {
      return;
    }
    while (atomic_load_explicit(&ownership->next, memory_order_acquire) ==
           NULL) {
      delay(0);
    }
  }
  successor = atomic_load_explicit(&ownership->next, memory_order_relaxed);
  atomic_store_explicit(&successor->locked, false, memory_order_release);
}