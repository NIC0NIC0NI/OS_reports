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
  while (ATOMIC_ACQUIRE_EXCHANGE(&mutex->locked, true)) {
    delay(0);
  }
}

void mutex_unlock_test_and_set(mutex_test_and_set_t *mutex) {
  ATOMIC_RELEASE(&mutex->locked, false);
}

int mutex_init_ticket(mutex_ticket_t *mutex) {
  atomic_init(&mutex->new_ticket, 0);
  atomic_init(&mutex->now_serving, 0);
  return SUCCESS;
}

void mutex_lock_ticket(mutex_ticket_t *mutex) {
  int my_ticket = ATOMIC_ADD(&mutex->new_ticket, 1);
  while (ATOMIC_ACQUIRE(&mutex->now_serving) != my_ticket) {
    delay(0);
  }
}

void mutex_unlock_ticket(mutex_ticket_t *mutex) {
  int next = ATOMIC_LOAD(&mutex->now_serving) + 1;
  ATOMIC_RELEASE(&mutex->now_serving, next);
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
  int my_place = ATOMIC_ADD(&mutex->next_slot, 1) & mutex->mask;
  while (ATOMIC_ACQUIRE(&mutex->slots[my_place].value)) {
    delay(0);
  }
  ATOMIC_STORE(&mutex->slots[my_place].value, true);
  onwership->my_place = my_place;
}

void mutex_unlock_Anderson(mutex_Anderson_t *mutex,
                           mutex_Anderson_ownership_t *onwership) {
  ATOMIC_RELEASE(&mutex->slots[(onwership->my_place + 1) & mutex->mask].value,
                 false);
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
  mutex_GT_tail_t current = {tid, ATOMIC_LOAD(&mutex->slots[tid].value)};
  mutex_GT_tail_t last = ATOMIC_EXCHANGE(&mutex->tail, current);
  while (ATOMIC_ACQUIRE(&mutex->slots[last.id].value) == last.locked) {
    delay(0);
  }
}

void mutex_unlock_GT(mutex_GT_t *mutex) {
  uint tid = thread_current_id();
  bool value = ATOMIC_LOAD(&mutex->slots[tid].value);
  ATOMIC_RELEASE(&mutex->slots[tid].value, !value);
}

int mutex_init_MCS(mutex_MCS_t *mutex) {
  atomic_init(&mutex->tail, NULL);
  return SUCCESS;
}

void mutex_lock_MCS(mutex_MCS_t *mutex, mutex_MCS_ownership_t *ownership) {
  mutex_MCS_ownership_t *predecessor;
  atomic_init(&ownership->next, NULL);
  predecessor = ATOMIC_EXCHANGE(&mutex->tail, ownership);
  if (predecessor != NULL) {
    atomic_init(&ownership->locked, true);
    ATOMIC_RELEASE(&predecessor->next, ownership);
    while (ATOMIC_ACQUIRE(&ownership->locked)) {
      delay(0);
    }
  }
}

void mutex_unlock_MCS(mutex_MCS_t *mutex, mutex_MCS_ownership_t *ownership) {
  mutex_MCS_ownership_t *successor;
  if (ATOMIC_LOAD(&ownership->next) == NULL) {
    mutex_MCS_ownership_t *expected = ownership;
    if (ATOMIC_COMPARE_EXCHANGE_RELEASE(&mutex->tail, &expected, NULL)) {
      return;
    }
    while (ATOMIC_ACQUIRE(&ownership->next) == NULL) {
      delay(0);
    }
  }
  successor = ATOMIC_LOAD(&ownership->next);
  ATOMIC_RELEASE(&successor->locked, false);
}

int mutex_init_CLH(mutex_CLH_t *mutex, uint t_num) {
  uint i;
  padded_abool_t *slots;
  padded_CLH_state_t *states;

  slots = (padded_abool_t *)malloc(sizeof(padded_abool_t) * (t_num + 1));
  if (slots == NULL) {
    return OUT_OF_MEMORY;
  }
  states = (padded_CLH_state_t *)malloc(sizeof(padded_CLH_state_t) * t_num);
  if (states == NULL) {
    return OUT_OF_MEMORY;
  }

  for (i = 0; i < t_num; ++i) {
    states[i].value.my_id = i;
    atomic_init(&slots[i].value, false);
  }
  atomic_init(&slots[t_num].value, true);
  atomic_init(&mutex->tail, t_num);

  mutex->slots = slots;
  mutex->states = states;
  return SUCCESS;
}

void mutex_destroy_CLH(mutex_CLH_t *mutex) {
  free(mutex->slots);
  free(mutex->states);
  mutex->slots = NULL;
  mutex->states = NULL;
}

void mutex_lock_CLH(mutex_CLH_t *mutex) {
  mutex_CLH_state_t *current_state = &mutex->states[thread_current_id()].value;
  uint node_id = current_state->my_id;
  ATOMIC_STORE(&mutex->slots[node_id].value, false);
  current_state->watching = ATOMIC_EXCHANGE(&mutex->tail, node_id);
  while (!ATOMIC_ACQUIRE(&mutex->slots[current_state->watching].value)) {
    delay(0);
  }
}

void mutex_unlock_CLH(mutex_CLH_t *mutex) {
  mutex_CLH_state_t *current_state = &mutex->states[thread_current_id()].value;
  ATOMIC_RELEASE(&mutex->slots[current_state->my_id].value, true);
  current_state->my_id = current_state->watching;
}