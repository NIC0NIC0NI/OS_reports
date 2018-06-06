#include "synchronize.h"
#include <stdlib.h>

static uint ceiling_log2(uint n) {
  uint i = 0;
  while ((1 << i) < n) {
    ++i;
  }
  return i;
}

static uint ceiling_frac(uint num, uint den) { return (num - 1) / den + 1; }
static uint min_uint_2(uint a, uint b) { return (a < b) ? a : b; }

int barrier_init_centralized(barrier_centralized_t *barrier, uint t_num) {
  uint i;
  padded_bool_t *local_sense =
      (padded_bool_t *)malloc(sizeof(padded_bool_t) * t_num);
  if (local_sense == NULL) {
    return OUT_OF_MEMORY;
  }
  barrier->local_sense = local_sense;
  barrier->thread_num = t_num;
  atomic_init(&barrier->count, t_num);
  atomic_init(&barrier->sense, true);
  for (i = 0; i < t_num; ++i) {
    local_sense[i].value = true;
  }
  return SUCCESS;
}

void barrier_destroy_centralized(barrier_centralized_t *barrier) {
  free(barrier->local_sense);
  barrier->local_sense = NULL;
}

void barrier_wait_centralized(barrier_centralized_t *barrier) {
  uint tid = thread_current_id();
  bool local_sense = !(barrier->local_sense[tid].value);
  barrier->local_sense[tid].value = local_sense;
  if (ATOMIC_SUB(&barrier->count, 1) == 1) {
    ATOMIC_STORE(&barrier->count, barrier->thread_num);
    ATOMIC_RELEASE(&barrier->sense, local_sense);
  } else {
    while (ATOMIC_ACQUIRE(&barrier->sense) != local_sense) {
      delay(0);
    }
  }
}

int barrier_init_combining_tree(barrier_combining_tree_t *barrier, uint t_num) {
  uint i, n_num, n_lower, n_curr, n_prev;
  padded_combining_tree_node_t *nodes;
  padded_bool_t *local_sense =
      (padded_bool_t *)malloc(sizeof(padded_bool_t) * t_num);
  if (local_sense == NULL) {
    return OUT_OF_MEMORY;
  }
  for (i = 0; i < t_num; ++i) {
    local_sense[i].value = true;
  }

  n_curr = ceiling_frac(t_num, COMBINING_TREE_FAN_IN);
  n_lower = 0;

  while (n_curr > 1) {
    n_lower += n_curr;
    n_curr = ceiling_frac(n_curr, COMBINING_TREE_FAN_IN);
  }
  n_num = n_lower + 1;

  nodes = (padded_combining_tree_node_t *)malloc(
      sizeof(padded_combining_tree_node_t) * n_num);
  if (nodes == NULL) {
    free(local_sense);
    return OUT_OF_MEMORY;
  }

  for (i = 0; i < n_num; ++i) {
    nodes[i].value.fan_in = COMBINING_TREE_FAN_IN;
  }

  n_prev = t_num;
  n_curr = ceiling_frac(n_prev, COMBINING_TREE_FAN_IN);
  n_lower = 0;
  while (n_curr > 1) {
    uint n_next = ceiling_frac(n_curr, COMBINING_TREE_FAN_IN);
    for (i = 0; i < n_curr; ++i) {
      uint i_next = i / COMBINING_TREE_FAN_IN;
      nodes[n_lower + i].value.parent = &nodes[n_lower + n_curr + i_next].value;
    }
    nodes[n_lower + n_curr - 1].value.fan_in =
        n_prev - (n_curr - 1) * COMBINING_TREE_FAN_IN;
    n_lower += n_curr;
    n_prev = n_curr;
    n_curr = n_next;
  }
  nodes[n_lower + n_curr - 1].value.fan_in =
      n_prev - (n_curr - 1) * COMBINING_TREE_FAN_IN;
  nodes[n_lower + n_curr - 1].value.parent = NULL;

  for (i = 0; i < n_num; ++i) {
    atomic_init(&nodes[i].value.count, nodes[i].value.fan_in);
    atomic_init(&nodes[i].value.sense, false);
  }

  barrier->nodes = nodes;
  barrier->local_sense = local_sense;
  return SUCCESS;
}

void barrier_destroy_combining_tree(barrier_combining_tree_t *barrier) {
  free(barrier->nodes);
  free(barrier->local_sense);
  barrier->nodes = NULL;
  barrier->local_sense = NULL;
}

static void barrier_wait_combining_tree_auxilary(combining_tree_node_t *node,
                                                 bool local_sense) {
  if (ATOMIC_SUB(&node->count, 1) == 1) {
    if (node->parent != NULL) {
      barrier_wait_combining_tree_auxilary(node->parent, local_sense);
    }
    ATOMIC_STORE(&node->count, node->fan_in);
    ATOMIC_RELEASE(&node->sense, local_sense);
  } else {
    while (ATOMIC_ACQUIRE(&node->sense) != local_sense) {
      delay(0);
    }
  }
}

void barrier_wait_combining_tree(barrier_combining_tree_t *barrier) {
  uint tid = thread_current_id();
  combining_tree_node_t *group_node =
      &barrier->nodes[tid / COMBINING_TREE_FAN_IN].value;
  bool local_sense = barrier->local_sense[tid].value;
  barrier_wait_combining_tree_auxilary(group_node, local_sense);
  barrier->local_sense[tid].value = !local_sense;
}

int barrier_init_dissemination(barrier_dissemination_t *barrier, uint t_num) {
  uint i, j, succ, log_t_num, table_size;
  padded_dissemination_flags_t *flags = (padded_dissemination_flags_t *)malloc(
      sizeof(padded_dissemination_flags_t) * t_num);
  if (flags == NULL) {
    return OUT_OF_MEMORY;
  }

  log_t_num = ceiling_log2(t_num);

  table_size =
      ceiling_frac(sizeof(atomic_bool *) * log_t_num, CACHE_LINE_SIZE) *
      CACHE_LINE_SIZE;

  for (i = 0; i < t_num; ++i) {
    void *buff = malloc(table_size + sizeof(atomic_bool) * 2 * log_t_num);
    if (buff == NULL) {
      break;
    }
    flags[i].value.sense = true;
    flags[i].value.parity = 0;
    flags[i].value.partner_flags = (atomic_bool **)buff;
    flags[i].value.my_flags =
        (atomic_bool *)((void *)((char *)buff + table_size));
  }
  succ = i;
  for (i = 0; i < t_num; ++i) {
    for (j = 0; j < log_t_num; ++j) {
      atomic_init(&flags[i].value.my_flags[j], false);
      atomic_init(&flags[i].value.my_flags[j + log_t_num], false);
      flags[i].value.partner_flags[j] =
          &flags[(i + (1 << j)) % t_num].value.my_flags[j];
    }
  }

  if (succ == t_num) {
    barrier->flags = flags;
    barrier->thread_num = t_num;
    barrier->log_thread_num = log_t_num;
    return SUCCESS;
  } else {
    for (i = 0; i < succ; ++i) {
      free(flags[i].value.partner_flags);
    }
    free(flags);
    return OUT_OF_MEMORY;
  }
}

void barrier_destroy_dissemination(barrier_dissemination_t *barrier) {
  uint i;
  for (i = 0; i < barrier->thread_num; ++i) {
    free(barrier->flags[i].value.partner_flags);
  }
  free(barrier->flags);
  barrier->flags = NULL;
}

void barrier_wait_dissemination(barrier_dissemination_t *barrier) {
  uint i;
  uint my_id = thread_current_id();
  dissemination_flags_t *my_flags = &barrier->flags[my_id].value;
  uint parity = my_flags->parity;
  bool sense = my_flags->sense;
  uint offset = barrier->log_thread_num * parity;

  for (i = 0; i < barrier->log_thread_num; ++i) {
    atomic_bool *partner_flag = my_flags->partner_flags[i];
    ATOMIC_RELEASE(&partner_flag[offset], sense);
    while (ATOMIC_ACQUIRE(&my_flags->my_flags[i + offset]) != sense) {
      delay(0);
    }
  }

  if (parity == 1) {
    my_flags->sense = !sense;
  }
  my_flags->parity = parity ^ 1;
}

int barrier_init_tournament(barrier_tournament_t *barrier, uint t_num) {
  uint i, j, succ, log_t_num, table_size;
  padded_tournament_flags_t *flags = (padded_tournament_flags_t *)malloc(
      sizeof(padded_tournament_flags_t) * t_num);
  if (flags == NULL) {
    return OUT_OF_MEMORY;
  }

  log_t_num = ceiling_log2(t_num);

  table_size = ceiling_frac((sizeof(atomic_bool *) + sizeof(char)) * log_t_num,
                            CACHE_LINE_SIZE) *
               CACHE_LINE_SIZE;

  for (i = 0; i < t_num; ++i) {
    void *buff = malloc(table_size + sizeof(atomic_bool) * log_t_num);
    if (buff == NULL) {
      break;
    }

    flags[i].value.opponent_flags = (atomic_bool **)buff;
    flags[i].value.roles = (char *)((void *)((atomic_bool **)buff + log_t_num));
    flags[i].value.my_flags =
        (atomic_bool *)((void *)((char *)buff + table_size));
  }
  succ = i;
  for (i = 0; i < t_num; ++i) {
    for (j = 0; j < log_t_num; ++j) {
      char role;
      uint base = (1 << (j + 1));
      uint half_base = (1 << j);

      if (i == 0 && base >= t_num) {
        flags[i].value.roles[j] = CHAMPION;
        flags[i].value.opponent_flags[j] =
            &flags[i + half_base].value.my_flags[j];
      } else if (i % base == half_base) {
        flags[i].value.roles[j] = LOSER;
        flags[i].value.opponent_flags[j] =
            &flags[i - half_base].value.my_flags[j];
      } else if (i % base == 0) {
        if (i + half_base < t_num) {
          flags[i].value.roles[j] = WINNER;
          flags[i].value.opponent_flags[j] =
              &flags[i + half_base].value.my_flags[j];
        } else {
          flags[i].value.roles[j] = BYE;
        }
      } else {
        break;
      }

      atomic_init(&flags[i].value.my_flags[j], false);
    }
    flags[i].value.sense = true;
  }

  if (succ == t_num) {
    barrier->flags = flags;
    barrier->thread_num = t_num;
    barrier->log_thread_num = log_t_num;
    return SUCCESS;
  } else {
    for (i = 0; i < succ; ++i) {
      free(flags[i].value.opponent_flags);
    }
    free(flags);
    return OUT_OF_MEMORY;
  }
}

void barrier_destroy_tournament(barrier_tournament_t *barrier) {
  uint i;
  for (i = 0; i < barrier->thread_num; ++i) {
    free(barrier->flags[i].value.opponent_flags);
  }
  free(barrier->flags);
  barrier->flags = NULL;
}

void barrier_wait_tournament(barrier_tournament_t *barrier) {
  uint my_id = thread_current_id();
  tournament_flags_t *my_flag = &barrier->flags[my_id].value;
  bool sense = my_flag->sense;
  int r;

  for (r = 0; r < barrier->log_thread_num; ++r) {
    char role = my_flag->roles[r];

    if (role == LOSER) {
      ATOMIC_RELEASE(my_flag->opponent_flags[r], sense);
      while (ATOMIC_ACQUIRE(&my_flag->my_flags[r]) != sense) {
        delay(0);
      }
      break;

    } else if (role == WINNER) {
      while (ATOMIC_ACQUIRE(&my_flag->my_flags[r]) != sense) {
        delay(0);
      }

    } else if (role == CHAMPION) {
      while (ATOMIC_ACQUIRE(&my_flag->my_flags[r]) != sense) {
        delay(0);
      }
      ATOMIC_RELEASE(my_flag->opponent_flags[r], sense);
      break;
    }
  }

  for (; r >= 0; --r) {
    if (my_flag->roles[r] == WINNER) {
      ATOMIC_RELEASE(my_flag->opponent_flags[r], sense);
    }
  }
  my_flag->sense = !sense;
}

int barrier_init_dual_tree(barrier_dual_tree_t *barrier, uint t_num) {
  uint i, j;
  padded_dual_tree_node_t *nodes = (padded_dual_tree_node_t *)malloc(
      sizeof(padded_dual_tree_node_t) * t_num);
  if (nodes == NULL) {
    return OUT_OF_MEMORY;
  }
  for (i = 0; i < t_num; ++i) {
    dual_tree_node_t *node = &nodes[i].value;
    node->fan_in_parent_flag =
        (i == 0)
            ? NULL
            : &nodes[(i - 1) / DUAL_TREE_FAN_IN]
                   .value.fan_in_child_not_ready[(i - 1) % DUAL_TREE_FAN_IN];

    for (j = 0; j < DUAL_TREE_FAN_IN; ++j) {
      bool have_child = DUAL_TREE_FAN_IN * i + j + 1 < t_num;
      node->have_fan_in_child[j] = have_child;
      atomic_init(&node->fan_in_child_not_ready[j], have_child);
    }

    for (j = 0; j < DUAL_TREE_FAN_OUT; ++j) {
      if (DUAL_TREE_FAN_OUT * i + j + 1 > t_num) {
        node->fan_out_child_flags[j] = NULL;
      } else {
        node->fan_out_child_flags[j] =
            &nodes[DUAL_TREE_FAN_OUT * i + j + 1].value.fan_out_parent_sense;
      }
    }

    node->local_sense = true;
    atomic_init(&node->fan_out_parent_sense, false);
  }

  barrier->nodes = nodes;
  return SUCCESS;
}

void barrier_destroy_dual_tree(barrier_dual_tree_t *barrier) {
  free(barrier->nodes);
  barrier->nodes = NULL;
}

static bool any_of(atomic_bool *arr) {
  uint i;
  for (i = 0; i < DUAL_TREE_FAN_IN; ++i) {
    if (ATOMIC_ACQUIRE(&arr[i])) {
      return true;
    }
  }
  return false;
}

void barrier_wait_dual_tree(barrier_dual_tree_t *barrier) {
  uint i;
  uint my_id = thread_current_id();
  dual_tree_node_t *my_node = &barrier->nodes[my_id].value;
  bool sense = my_node->local_sense;

  while (any_of(my_node->fan_in_child_not_ready)) {
    delay(0);
  }
  for (i = 0; i < DUAL_TREE_FAN_IN; ++i) {
    ATOMIC_STORE(&my_node->fan_in_child_not_ready[i],
                 my_node->have_fan_in_child[i]);
  }
  if (my_id != 0) {
    ATOMIC_RELEASE(my_node->fan_in_parent_flag, false);
    while (ATOMIC_ACQUIRE(&my_node->fan_out_parent_sense) != sense) {
      delay(0);
    }
  }

  for (i = 0; i < DUAL_TREE_FAN_OUT; ++i) {
    atomic_bool *child = my_node->fan_out_child_flags[i];
    if (child != NULL) {
      ATOMIC_RELEASE(child, sense);
    }
  }
  my_node->local_sense = !sense;
}

int barrier_init_arrival_tree(barrier_arrival_tree_t *barrier, uint t_num) {
  uint i, j;
  padded_arrival_tree_node_t *nodes = (padded_arrival_tree_node_t *)malloc(
      sizeof(padded_arrival_tree_node_t) * t_num);
  if (nodes == NULL) {
    return OUT_OF_MEMORY;
  }
  for (i = 0; i < t_num; ++i) {
    arrival_tree_node_t *node = &nodes[i].value;
    node->fan_in_parent_flag =
        (i == 0)
            ? NULL
            : &nodes[(i - 1) / DUAL_TREE_FAN_IN]
                   .value.fan_in_child_not_ready[(i - 1) % DUAL_TREE_FAN_IN];

    for (j = 0; j < DUAL_TREE_FAN_IN; ++j) {
      bool have_child = DUAL_TREE_FAN_IN * i + j + 1 < t_num;
      node->have_fan_in_child[j] = have_child;
      atomic_init(&node->fan_in_child_not_ready[j], have_child);
    }

    node->local_sense = true;
  }

  barrier->nodes = nodes;
  return SUCCESS;
}

void barrier_destroy_arrival_tree(barrier_arrival_tree_t *barrier) {
  free(barrier->nodes);
  barrier->nodes = NULL;
}

void barrier_wait_arrival_tree(barrier_arrival_tree_t *barrier) {
  uint i;
  uint my_id = thread_current_id();
  arrival_tree_node_t *my_node = &barrier->nodes[my_id].value;
  bool sense = my_node->local_sense;

  while (any_of(my_node->fan_in_child_not_ready)) {
    delay(0);
  }
  for (i = 0; i < DUAL_TREE_FAN_IN; ++i) {
    ATOMIC_STORE(&my_node->fan_in_child_not_ready[i],
                 my_node->have_fan_in_child[i]);
  }
  if (my_id != 0) {
    ATOMIC_RELEASE(my_node->fan_in_parent_flag, false);
    while (ATOMIC_ACQUIRE(&barrier->sense) != sense) {
      delay(0);
    }
  } else {
    ATOMIC_RELEASE(&barrier->sense, sense);
  }

  my_node->local_sense = !sense;
}