#include <algorithm>
#include "ThreadContext.h"
#include "JobContext.h"


ThreadContext::ThreadContext(int id,JobContext* jc){
  this->id = id;
  this->job_context = jc;
}

void ThreadContext::addMapping(K2 *k, V2 *v) {
  intermediateVec.push_back({k, v});
}

JobContext* ThreadContext::getJob() {return job_context;}
IntermediateVec &ThreadContext::getIntermediateVec() {return intermediateVec;}

int ThreadContext::getId() const
{return id;}

void ThreadContext::sort() {
  std::sort(intermediateVec.begin(), intermediateVec.end(),
            [](const IntermediatePair a,const IntermediatePair b){return *(a.first)<*(b.first);});
}

void ThreadContext::map(){
  JobContext* jc = getJob();
  unsigned long old_value = 0;
  size_t inputVecSize = (jc->inputVec).size();
  while((old_value = jc->inputAtomic++) < inputVecSize){
    jc->getClient().map(jc->getInputVec()[old_value].first,
                        jc->getInputVec()[old_value].second, this);
    jc->incTotalPairs();
    uint64_t i = 1;
    jc->atomicVar+= (i<<33);
  }
}