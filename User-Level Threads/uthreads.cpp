#include "uthreads.h"
#include "Thread.h"
#include <signal.h>
#include <sys/time.h>
#include <queue>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

// ============================= constants ============================

#define SUCCESS 1
#define USECS 1000000
#define EXIT_FAILURE 1
#define FAILURE (-1)
#define INVALID_QUANTUM "sigprocmask error\n"
#define ALLOC_ERROR "system error: thread library error: ERROR - bad alloc\n"
#define SIGACTION_ERROR "system error: sigaction error.\n"
#define SIGPROCMASK_ERROR "system error: sigprocmask error\n"
#define SET_TIMER_ERROR "system error: setitimer error.\n"
#define MAX_THREAD_ERROR "thread library error: ERROR - reached maximum threads number\n"
#define ID_ERROR "thread library error: ERROR - no thread with given ID\n"
#define BLOCK_MAIN_THREAD_ERROR "thread library error: ERROR - cannot block the main thread\n"
#define INVALID_ENTRY_POINT "entry point error: ERROR - entry point cannot be nullptr\n"

// =============================== enums ===============================

enum Action {
  UNBLOCK_SIG, BLOCK_SIG
};

enum Task {
  TERMINATE = 200, SLEEP, SELF_BLOCK
};

// =============================== fields ===============================

typedef unsigned long address_t;
static std::map<int, Thread *> thread_dict; // maps the threads to ids need
static std::set<int> sleeping_threads_set;
static std::deque<Thread *> ready_threads;
static Thread *running;
static int total_num_of_quantum;
static struct sigaction sa;
static struct itimerval timer;
static sigset_t sig;
priority_queue<int, vector<int>, greater<int>> id_queue;


// ========================= Auxiliary methods ============================

void block(Action action)
{
  switch (action) {
    case BLOCK_SIG:
      if (sigprocmask(SIG_BLOCK, &sig, nullptr) < 0) {
        std::cerr << SIGPROCMASK_ERROR << std::endl;
        exit(EXIT_FAILURE);
      }
      break;
    case UNBLOCK_SIG:
      if (sigprocmask(SIG_UNBLOCK, &sig, nullptr) < 0) {
        std::cerr << SIGPROCMASK_ERROR << std::endl;
        exit(EXIT_FAILURE);
      }
      break;
  }
}

void remove_from_ready_queue(Thread *th)
{
  auto it = find(ready_threads.begin(), ready_threads.end(), th);
  ready_threads.erase(it);
}

void handle_sleeping_threads()
{
  set<int> set_to_erase;
  for (auto s : sleeping_threads_set) {
    if (s == running->get_id()) continue;
    thread_dict[s]->dec_sleep_time();
    if ((thread_dict[s]->get_sleep_time()) > 0) continue;
    set_to_erase.insert(s);
    if (thread_dict[s]->get_state() == SLEEPING_AND_BLOCK) {thread_dict[s]->set_state(BLOCK);}
    else {
      ready_threads.push_back(thread_dict[s]);
      thread_dict[s]->set_state(READY);
    }
  }
  for (auto s : set_to_erase) {
    sleeping_threads_set.erase(s);
  }

}

void scheduler(int task)
{
  block(BLOCK_SIG);
  total_num_of_quantum++;
  handle_sleeping_threads();
  if (ready_threads.empty()) {
    running->increase_quantum();
    block(UNBLOCK_SIG);
    return;
  }
  if (task != TERMINATE) {
    if (running->save_env() == 1) // save the environment of the running thread.
    {
      block(UNBLOCK_SIG);
      return;
    }
  }
  int cur_id = running->get_id();
  if (task == TERMINATE) {
    if (sleeping_threads_set.count(running->get_id())) {
      sleeping_threads_set.erase(running->get_id());
    }
    id_queue.push(cur_id);
    thread_dict.erase(running->get_id());
    delete (running);
  } else if (task != SLEEP && task != SELF_BLOCK) {
    ready_threads.push_back(running);
    running->set_state(READY);
  } else if (task == SELF_BLOCK) {
    running->set_state(BLOCK);
  }
  running = ready_threads.front();
  ready_threads.pop_front();
  running->set_state(RUNNING);
  running->increase_quantum();
  block(UNBLOCK_SIG);
  running->jump_env();
}

void set_the_timer()
{
  if (setitimer(ITIMER_VIRTUAL, &timer, nullptr)) {
    printf(SET_TIMER_ERROR);
    exit(EXIT_FAILURE);
  }
}

void init_the_timer(int quantum_usecs)
{
  sa.sa_handler = &scheduler; // implement timer_handler
  if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
    printf(SIGACTION_ERROR);
    exit(EXIT_FAILURE);
  }
  timer.it_value.tv_sec = quantum_usecs / USECS;
  timer.it_value.tv_usec = quantum_usecs % USECS;
  timer.it_interval.tv_sec = quantum_usecs / USECS;
  timer.it_interval.tv_usec = quantum_usecs % USECS;
  // Start a virtual timer. It counts down whenever this process is executing.
  set_the_timer();
}

// ========================= API methods ============================

/**
 * @brief initializes the thread library.
 *
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
  if (quantum_usecs <= 0) { // take care of none positive input
    std::cerr << INVALID_QUANTUM << std::endl;
    return FAILURE;
  }

  // make an empty signal set and add SIGVTALRM, SIGALRM signals.
  sigemptyset(&sig);
  sigaddset(&sig, SIGVTALRM);
  try {
    // create the thread
    auto thread = new Thread(0, nullptr);
    thread->set_state(RUNNING);
    thread->set_quantum(1);
    thread->save_env();
    thread_dict[0] = thread;
    running = thread;
    total_num_of_quantum = 1;

    // initialize available id's
    for (int i = 1; i <= 100; i++) {id_queue.push(i);}
    init_the_timer(quantum_usecs);
    return SUCCESS;
  }
  catch (std::bad_alloc &ba) {
    std::cerr << ALLOC_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point)
{
  block(BLOCK_SIG);
  size_t size = thread_dict.size();
  if (size >= MAX_THREAD_NUM) {
    std::cerr << MAX_THREAD_ERROR << std::endl;
    block(UNBLOCK_SIG);
    return FAILURE;
  }
  if (entry_point == nullptr) {
    std::cerr << INVALID_ENTRY_POINT << std::endl;
    block(UNBLOCK_SIG);
    return FAILURE;
  }
  try {
    int cur_id = id_queue.top();
    id_queue.pop();
    auto *thread = new Thread(cur_id, entry_point);
    ready_threads.push_back(thread); // add Thread to ready_threads
    thread_dict[cur_id] = thread;
    block(UNBLOCK_SIG);
    return cur_id;
  }
  catch (std::bad_alloc &ba) {
    std::cerr << ALLOC_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
  block(BLOCK_SIG);
  if (tid == 0) {
    for (auto &iter : thread_dict) {delete iter.second;}
    block(UNBLOCK_SIG);
    exit(EXIT_FAILURE);
  }
  if (!thread_dict.count(tid)) {
    std::cerr << ID_ERROR << std::endl;
    block(UNBLOCK_SIG);
    return FAILURE;
  }
  Thread *cur_thread = thread_dict[tid];
  if (cur_thread->get_state() == RUNNING) {
    set_the_timer();
    block(UNBLOCK_SIG);
    scheduler(TERMINATE);
    return SUCCESS;
  }
  thread_dict.erase(tid);  // remove thread from thread_dict.
  if (cur_thread->get_state() == READY) {remove_from_ready_queue(cur_thread);}
  delete (cur_thread);
  id_queue.push(tid);
  block(UNBLOCK_SIG);
  return SUCCESS;
}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
  block(BLOCK_SIG);
  if (tid == 0) {
    std::cerr << BLOCK_MAIN_THREAD_ERROR << std::endl;
    block(UNBLOCK_SIG);
    return FAILURE;
  }
  if (!thread_dict.count(tid)) {
    std::cerr << ID_ERROR << std::endl;
    block(UNBLOCK_SIG);
    return FAILURE;
  }
  Thread *cur_thread = thread_dict[tid];
  if (cur_thread->get_state() == READY) {
    remove_from_ready_queue(cur_thread);
    cur_thread->set_state(BLOCK);
  }
  if (cur_thread->get_state() == RUNNING) {
    set_the_timer();
    scheduler(SELF_BLOCK);
    cur_thread->set_state(BLOCK);
  }
  if (cur_thread->get_state() == SLEEPING) {
    cur_thread->set_state(SLEEPING_AND_BLOCK);
  }
  block(UNBLOCK_SIG);
  return SUCCESS;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
  block(BLOCK_SIG);
  if (!thread_dict.count(tid)) {
    std::cerr << ID_ERROR << std::endl;
    block(UNBLOCK_SIG);
    return FAILURE;
  }
  Thread *cur_thread = thread_dict[tid];
  if (cur_thread->get_state() == BLOCK) {
    cur_thread->set_state(READY);
    ready_threads.push_back(cur_thread);
  } else if (cur_thread->get_state() == SLEEPING_AND_BLOCK) {
    cur_thread->set_state(SLEEPING);
  }
  block(UNBLOCK_SIG);
  return SUCCESS;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY threads list.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid==0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums)
{
  block(BLOCK_SIG);
  if (num_quantums <= 0) {
    std::cerr << INVALID_QUANTUM << std::endl;
    return FAILURE;
  }
  if (running->get_id() == 0) {
    std::cerr << BLOCK_MAIN_THREAD_ERROR << std::endl;
    return FAILURE;
  }
  thread_dict[running->get_id()]->set_sleep_time(num_quantums);
  sleeping_threads_set.insert(running->get_id());
//  set_the_timer();
  running->set_state(SLEEPING);
  scheduler(SLEEP);
  return SUCCESS;
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid()
{
  return running->get_id();
}

/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums()
{
  return total_num_of_quantum;
}

/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
  block(BLOCK_SIG);
  if (!thread_dict.count(tid)) {
    std::cerr << ID_ERROR << std::endl;
    block(UNBLOCK_SIG);
    return -1;
  }
  block(UNBLOCK_SIG);
  return thread_dict[tid]->get_quantum();
}



