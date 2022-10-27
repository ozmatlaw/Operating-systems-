#ifndef EX3_JOBCONTEXT_H
#define EX3_JOBCONTEXT_H

#include <csignal>
#include <atomic>
#include "MapReduceFramework.h"
#include "ThreadContext.h"
#include "Barrier.h"


#define PTHREAD_CREATE_ERROR "system error: pthread_create error\n"
#define THREAD_JOIN_ERR "system error: error on pthread_join\n"

class JobContext {
 private:
  stage_t stage;
  const MapReduceClient &client;
  OutputVec &outputVec;
  int multiThreadLevel;
  std::vector<ThreadContext *> threadContexts;
  int numOfThreadsToProcess;
  int totalPairs;
  IntermediateVec vecToReduce;

 public:
  std::vector<pthread_t> threads;
  Barrier barrier;
  const InputVec &inputVec;
  size_t sumy= 0;
  size_t pairsInShuffle= 0;
  std::vector<IntermediateVec*> shuffledVec;
  pthread_mutex_t reduceMutex;
  pthread_mutex_t emitMutex;
  pthread_mutex_t waitMutex;
  std::atomic<int> inputAtomic{0};
  std::atomic<uint64_t> atomicVar{0};
  bool threadsJoined;
  JobContext(const MapReduceClient &client, const InputVec &inputVec,
             OutputVec &outputVec, int multiThreadLevel);

  ~JobContext();

  MapReduceClient &getClient();

  InputVec &getInputVec();

  OutputVec &getOutputVec();

  stage_t getStage();

  int getMulti();

  void initContexts();

  void createThreads();

  void addMapping(K2 *k, V2 *v);

  void incAtomic();

  pthread_mutex_t getReduceMutex();

  pthread_mutex_t& getEmitMutex();

  pthread_mutex_t getWaitMutex();

  void incTotalPairs();

  void decTotalPairs();

  int getTotalPairs();

  int getNumOfThreadsToProcess();

  void decNumToProcess();

  void setStage(stage_t stage_new);

  void shuffle();

  void reduce(void *threadContext);

  K2 *getMaxKey();


  IntermediateVec *getVecToReduce();
  std::vector<IntermediateVec*> * getShuffledVec();
};

#endif //EX3_JOBCONTEXT_H