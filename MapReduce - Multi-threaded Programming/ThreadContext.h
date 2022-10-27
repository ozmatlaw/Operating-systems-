#ifndef EX3__THREADCONTEXT_H_
#define EX3__THREADCONTEXT_H_
#include "MapReduceFramework.h"
class JobContext;


class ThreadContext {
 private:
  IntermediateVec intermediateVec{};
  JobContext* job_context;
  int id;

 public:
  ThreadContext(int id, JobContext* jc);
  void addMapping(K2* k,V2* v);
  JobContext* getJob();
  IntermediateVec &getIntermediateVec();
  int getId() const;
  void sort();
  void map();
};

#endif //EX3__THREADCONTEXT_H_