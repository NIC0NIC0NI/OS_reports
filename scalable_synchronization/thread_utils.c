#include "synchronize.h"
#define THREAD_LOCAL _Thread_local

static atomic_uint thread_num = ATOMIC_VAR_INIT(0);
static THREAD_LOCAL uint thread_id;

void thread_init(int expected_thread_num) {
  thread_id = atomic_fetch_add_explicit(&thread_num, 1, memory_order_release);
  while (atomic_load_explicit(&thread_num, memory_order_acquire) < expected_thread_num) {
    delay(0);
  }
}

uint thread_total_number() { return atomic_load(&thread_num); }

uint thread_current_id() { return thread_id; }
