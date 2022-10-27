#ifndef _THREAD_H_
#define _THREAD_H_

#include <setjmp.h>
#include <iostream>

#define STACK_SIZE 4096
enum Status {
  READY, RUNNING, BLOCK, SLEEPING, SLEEPING_AND_BLOCK
};

typedef void (*thread_entry_point)(void);

class Thread {
 public:
  Thread(int id, thread_entry_point entry_point);

  void set_state(Status s);

  Status get_state() const;

  int get_id() const;

  void set_quantum(int quantum);

  void increase_quantum();

  int get_quantum() const;

  int save_env();

  int get_sleep_time();

  void dec_sleep_time();

  void set_sleep_time(int value);

  void jump_env();

  ~Thread();

 private:
  Status _status;
  char *_stack;
  int _num_quantum;
  int _thread_ID;
  sigjmp_buf _environment;
  thread_entry_point _entry_point;
  int _sleep_time;


};

#endif //_THREAD_H_