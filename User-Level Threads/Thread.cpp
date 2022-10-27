#include "Thread.h"
#include "env.h"

Thread::Thread(int id, thread_entry_point entry_point)
{
  _thread_ID = id;
  _status = READY;
  _num_quantum = 0;
  _entry_point = entry_point;
  _sleep_time = 0;
  _stack = new char[STACK_SIZE]();
  address_t sp = (address_t) _stack + STACK_SIZE - sizeof(unsigned long);
  address_t pc = (address_t) _entry_point;
  sigsetjmp(_environment, 1);
  (_environment->__jmpbuf)[JB_SP] = translate_address(sp);
  (_environment->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&_environment->__saved_mask);
}


int Thread::get_sleep_time()
{ return _sleep_time; }

void Thread::dec_sleep_time()
{ _sleep_time--; }

void Thread::set_sleep_time(int value)
{ _sleep_time = value; }

void Thread::set_state(Status s)
{ _status = s; }

Status Thread::get_state() const
{ return _status; }

void Thread::increase_quantum()
{ _num_quantum++; }

void Thread::set_quantum(int quantum)
{ _num_quantum = quantum; }

int Thread::get_id() const
{ return _thread_ID; }

int Thread::get_quantum() const
{ return _num_quantum; }

int Thread::save_env()
{ return sigsetjmp(_environment, 1); }

void Thread::jump_env()
{ siglongjmp(_environment, 1); }

Thread::~Thread()
{ delete _stack; }