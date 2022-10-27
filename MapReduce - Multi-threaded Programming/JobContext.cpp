#include "JobContext.h"
#include <iostream>

#include "MapReduceFramework.h"
void* mapper(void *context);

JobContext::JobContext(const MapReduceClient &client,
                       const InputVec &inputVec, OutputVec &outputVec,
                       int multiThreadLevel
)
        :stage(UNDEFINED_STAGE),
        client(client),
          outputVec(outputVec),
         multiThreadLevel(multiThreadLevel),
         threadContexts(),
         numOfThreadsToProcess(multiThreadLevel),
         totalPairs(0),
         vecToReduce(),
         threads(multiThreadLevel),
         barrier(Barrier(multiThreadLevel)),
         inputVec(inputVec),
         shuffledVec(),
         threadsJoined(false)

{
  stage = UNDEFINED_STAGE;
  numOfThreadsToProcess = multiThreadLevel;
  totalPairs = 0;
  initContexts();
  if (pthread_mutex_init(&reduceMutex, nullptr) != 0){
    std::cerr<<"System Err.."<<std::endl;
    exit(1);
  }
  if (pthread_mutex_init(&emitMutex, nullptr) != 0){
    std::cerr<<"System Err.."<<std::endl;
    exit(1);
  }
  if (pthread_mutex_init(&waitMutex, nullptr) != 0){
    std::cerr<<"System Err.."<<std::endl;
    exit(1);
  }
}

JobContext::~JobContext()
{
  for (auto *th : threadContexts) {
    delete th;
  }
  threadContexts.clear();
  if (pthread_mutex_destroy(&reduceMutex) != 0){
    std::cerr<<"System Err.."<<std::endl;
    exit(1);
  }

  if (pthread_mutex_destroy(&emitMutex) != 0){
    std::cerr<<"System Err.."<<std::endl;
    exit(1);
  }
  if (pthread_mutex_destroy(&waitMutex) != 0){
    std::cerr<<"System Err..3"<<std::endl;
    exit(1);
  }
}

MapReduceClient &JobContext::getClient()
{
  return const_cast<MapReduceClient &>(client);
}

InputVec &JobContext::getInputVec()
{
  return const_cast<InputVec &>(inputVec);
}

OutputVec &JobContext::getOutputVec()
{
  return outputVec;
}

stage_t JobContext::getStage()
{
  return stage;
}

void JobContext::incTotalPairs()
{
  totalPairs++;
}

void JobContext::decTotalPairs()
{
  totalPairs--;
}

int JobContext::getTotalPairs()
{
  return totalPairs;
}

pthread_mutex_t JobContext::getReduceMutex()
{
  return reduceMutex;
}

pthread_mutex_t& JobContext::getEmitMutex()
{
  return emitMutex;
}

pthread_mutex_t JobContext::getWaitMutex()
{
  return waitMutex;
}

IntermediateVec *JobContext::getVecToReduce()
{
  return &vecToReduce;
}

std::vector<IntermediateVec*> *JobContext::getShuffledVec()
{
  return &shuffledVec;
}

void JobContext::setStage(stage_t stage_new)
{
  atomicVar = stage_new;
  stage = stage_new;
}


int JobContext::getMulti()
{
  return multiThreadLevel;
}

void JobContext::initContexts()
{
  for (int i = 0; i< multiThreadLevel;i++) {
    threadContexts.push_back(new ThreadContext(i,this));
  }
}

void JobContext::createThreads()
{
  auto data = threads.data();
  for (int i = 0; i < multiThreadLevel; i++) {
    if (pthread_create(data + i, nullptr, mapper,threadContexts[i])) {
      std::cerr << PTHREAD_CREATE_ERROR;
      exit(1);
    }
  }
}

int JobContext::getNumOfThreadsToProcess()
{
  return numOfThreadsToProcess;
}

void JobContext::shuffle()
{
  for(auto th :threadContexts){
    sumy += th->getIntermediateVec().size();
  }

  while (pairsInShuffle < sumy) {
    K2 *maxKey = getMaxKey();
    IntermediateVec *curLastVec = new std::vector<std::pair<K2*, V2*>>;
    for (int i = 0; i < numOfThreadsToProcess; ) {
      if (threadContexts[i]->getIntermediateVec().empty()) {
        i++;
        continue;
      }

      if (!((*maxKey) < *(threadContexts[i]->getIntermediateVec().back().first)) &&
              !(*(threadContexts[i]->getIntermediateVec().back().first) < (*maxKey))) {
        curLastVec->push_back(threadContexts[i]->getIntermediateVec().back());
        threadContexts[i]->getIntermediateVec().pop_back();
      }
      else{
        i++;
      }
    }
    pairsInShuffle += curLastVec->size();
    shuffledVec.push_back(curLastVec);
  }
}

K2 *JobContext::getMaxKey()
{
  K2 *maxKey = nullptr;
  for (int i = 0; i < numOfThreadsToProcess; i++) {
    if (threadContexts[i]->getIntermediateVec().empty() ||
            threadContexts[i]->getIntermediateVec().back().first == nullptr) {
      continue;
    }
    if (maxKey == nullptr || (*maxKey) < (*(threadContexts[i]->getIntermediateVec().back().first))) {
      maxKey = threadContexts[i]->getIntermediateVec().back().first;
    }
  }
  return maxKey;
}

void JobContext::reduce(void *threadContext)
{
  pthread_mutex_lock(&reduceMutex);
  while (!shuffledVec.empty()) {
    IntermediateVec* curVec = shuffledVec.back();
    shuffledVec.pop_back();
    pthread_mutex_unlock(&reduceMutex);
    uint64_t curVecSize = curVec->size();
    getClient().reduce(curVec, threadContext);
    atomicVar+= (curVecSize<<2);
    delete curVec;
    pthread_mutex_lock(&reduceMutex);
  }
  pthread_mutex_unlock(&reduceMutex);
}

void* mapper(void *context)
{
  ThreadContext *threadContext = (ThreadContext *) context;
  JobContext *job = threadContext->getJob();
  job->atomicVar.fetch_or(stage_t::MAP_STAGE);
  threadContext->map();
  threadContext->sort();
  threadContext->getJob()->barrier.barrier();
  if (threadContext->getId() == 0) {
    job->setStage(stage_t::SHUFFLE_STAGE);
    job->shuffle();
    job->setStage(stage_t::REDUCE_STAGE);
    threadContext->getJob()->barrier.barrier();
  } else {
    threadContext->getJob()->barrier.barrier();
  }
  job->reduce(threadContext);
  return nullptr;
}
