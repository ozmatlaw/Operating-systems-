#include "MapReduceFramework.h"
#include "JobContext.h"
#include "ThreadContext.h"

#include <pthread.h>

#include "algorithm"
#include <iostream>
#include <atomic>


void emit2(K2 *key, V2 *value, void *context)
{
  ThreadContext *threadContext = (ThreadContext *) context;
  threadContext->addMapping(key, value);
}

void emit3(K3 *key, V3 *value, void *context)
{
  auto threadContext = static_cast<ThreadContext *>(context);
  pthread_mutex_lock(&(threadContext->getJob()->getEmitMutex())); //lock mutex
  threadContext->getJob()->getOutputVec().push_back({key, value});
  pthread_mutex_unlock(&(threadContext->getJob()->getEmitMutex()));//unlock mutex
}

JobHandle startMapReduceJob(const MapReduceClient &client,
                            const InputVec &inputVec, OutputVec &outputVec,
                            int multiThreadLevel)
{
  auto jc = new JobContext(client, inputVec, outputVec, multiThreadLevel);
  jc->createThreads();
  return static_cast<void *> (jc);
}
void waitForJob(JobHandle job){
  auto jc = static_cast<JobContext*>(job);
  pthread_mutex_lock(&jc->waitMutex);
  if(jc->threadsJoined){
    pthread_mutex_unlock(&jc->waitMutex);
    return;
  }
  jc->threadsJoined = true;
  for(int i = 0; i<jc->getNumOfThreadsToProcess();i++){
    if (pthread_join(jc->threads[i], nullptr)!=0){
      std::cerr << THREAD_JOIN_ERR << std::endl;
      exit(1);
    }
  }
  pthread_mutex_unlock(&jc->waitMutex);
}

void getJobState(JobHandle job, JobState *state){
  auto jc = static_cast<JobContext*>(job);
  int64_t val = jc->atomicVar.load();
  state->stage = static_cast<stage_t>(val&3);
  float p;
  if ((val & 3) == UNDEFINED_STAGE){
    state->stage = UNDEFINED_STAGE;
    state->percentage = 0;
  }
  else if ((val & 3) == MAP_STAGE){
    state->stage = MAP_STAGE;
     p = ((float)((val>>33) &0x7fffffff) /
            (float) (jc->inputVec).size()) *100;
    state->percentage = p;
  }
  else if ((val & 3) == SHUFFLE_STAGE){
    state->stage = SHUFFLE_STAGE;
    p = (float) jc->pairsInShuffle/
            (float)jc->sumy;
    state->percentage = p * 100;
  }
  else if ((val & 3) == REDUCE_STAGE){
    state->stage = REDUCE_STAGE;
    p = (float)((val>>2) &0x7fffffff) /
            (float) jc->sumy;
    state->percentage = p * 100;
  }
}

void closeJobHandle(JobHandle job)
{
  auto *jobContext = (JobContext *) job;
  waitForJob(jobContext);
  delete (jobContext);
}

