#include "VirtualMemory.h"
#include "PhysicalMemory.h"

struct travStruct {
 public:
  uint64_t pageTEvict = 0;
  uint64_t maxFrame = 0;
  uint64_t fEmpty = 0;
  uint64_t emptyParent = 0;
  uint64_t fEmptyRow = 0;
  uint64_t fEvict = 0;
  uint64_t fEvictParent = 0;
  uint64_t evictRow = 0;
  uint64_t framesPath[TABLES_DEPTH] = {0};
  uint64_t maxDist = 0;
  uint64_t pageNumber = 0;
};

// ---------------------------------- function signatures ----------------------------------

void
findEmptyFrame(travStruct *ts, uint64_t frame = 0, uint64_t frame_parent = 0, uint64_t frame_row = 0, int depth = 0);

void getFrameToEvict(travStruct *ts, uint64_t frame = 0, uint64_t frameRow = 0,
                     uint64_t parent = 0, uint64_t curPageNum = 0, uint64_t curDepth = 0, uint64_t curDist = 0);

void
setStruct(travStruct *ts, bool fEmpty, uint64_t maxWeightNew = 0, uint64_t fEvictNew = 0, uint64_t fEvictParentNew = 0,
          uint64_t evictRowNew = 0, uint64_t pageTEvictNew = 0, uint64_t emptyParentNew = 0, uint64_t fEmptyNew = 0,
          uint64_t fEmptyRowNew = 0);



uint64_t getOffset(uint64_t address)
{
    return address & ((1 << OFFSET_WIDTH) - 1);
//    uint64_t d_mask = (1 << OFFSET_WIDTH) - 1;
//    uint64_t d = address & d_mask;
//    return d;
}

uint64_t getPageNum(uint64_t virtual_address)
{
  uint64_t sig_bits_of_address = virtual_address >> (uint64_t) OFFSET_WIDTH;
  auto mask = (uint64_t) ((1 << (uint64_t) (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH)) - 1);
  return sig_bits_of_address & mask;
}

void clearTable(uint64_t frameIndex)
{
  for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
    PMwrite(frameIndex * PAGE_SIZE + i, 0);
  }
}

bool fInPath(uint64_t targetFrame, travStruct *ts)
{
  for (unsigned long frame : ts->framesPath) {
    if (frame == targetFrame) {
      return true;
    }
  }
  return false;
}

void getMaxFrame(uint64_t frameIdx, travStruct *ts, int depth)
{
  if (depth != TABLES_DEPTH) {
    for (int i = 0; i < PAGE_SIZE; i++) {
      word_t val = 0;
      PMread(frameIdx * PAGE_SIZE + i, &val);
      if (val != 0) {
        if (ts->maxFrame < (uint64_t) val) {
          ts->maxFrame = (uint64_t) val;
        }
        getMaxFrame(val, ts, depth + 1);
      }
    }
  }
}

void findEmptyFrame(travStruct *ts, uint64_t frame, uint64_t frameParent, uint64_t frameRow, int depth)
{
  if (depth != TABLES_DEPTH) {
    bool isEmptyF = true;
    for (int i = 0; i < PAGE_SIZE; i++) {
      word_t val = 0;
      PMread(frame * PAGE_SIZE + i, &val);
      if (val != 0) {
        isEmptyF = false;
        findEmptyFrame(ts, val, frame, i, depth + 1);
      }
    }
    if (isEmptyF && !fInPath(frame, ts)) {
      setStruct(ts, false, 0, 0, 0, 0, 0,
                frameParent, frame, frameRow);
    }
  }
}


void setStruct(travStruct *ts, bool evict, uint64_t maxDistNew, uint64_t fEvictNew, uint64_t fEvictParentNew,
               uint64_t evictRowNew, uint64_t pageTEvictNew, uint64_t emptyParentNew, uint64_t fEmptyNew,
               uint64_t fEmptyRowNew)
{
  if (evict) {
    ts->maxDist = maxDistNew;
    ts->fEvict = fEvictNew;
    ts->fEvictParent = fEvictParentNew;
    ts->evictRow = evictRowNew;
    ts->pageTEvict = pageTEvictNew;
  } else {
    ts->emptyParent = emptyParentNew;
    ts->fEmpty = fEmptyNew;
    ts->fEmptyRow = fEmptyRowNew;
  }
}


void changeEvictCandidate(travStruct *ts, uint64_t curDist, uint64_t curPageNum,
                          uint64_t frame, uint64_t frameRow, uint64_t parent)
{
  if (curDist > ts->maxDist) {
    setStruct(ts, true, curDist, frame, parent, frameRow,
              curPageNum);
  }
}

uint64_t getMax(uint64_t dist, uint64_t dist2)
{
  if (dist > dist2) return dist;
  return dist2;
}

uint64_t calcDist(uint64_t parent, uint64_t frame)
{
  int64_t dist = parent - frame;
  if (dist < 0) {
    dist = -dist;
  }
  if ((NUM_PAGES - dist) < dist) {
    return NUM_PAGES - dist;
  }
  return dist;
}

void getFrameToEvict(travStruct *ts, uint64_t frame, uint64_t frameRow, uint64_t parent,
                     uint64_t curPageNum, uint64_t curDepth, uint64_t curDist)
{
  if (curDepth == TABLES_DEPTH) {
    curDist = getMax(curDist, calcDist(ts->pageNumber, curPageNum));
    // get the dist from his parent with formula
    changeEvictCandidate(ts, curDist, curPageNum, frame, frameRow, parent);
  }
  if (curDepth != TABLES_DEPTH) {
    for (int row = 0; row < PAGE_SIZE; row++) {
      word_t val = 0;
      PMread(frame * PAGE_SIZE + row, &val);
      if (val != 0) {
        getFrameToEvict(ts, val, row, frame, (curPageNum << OFFSET_WIDTH) + row,
                        curDepth + 1, curDist);
      }
    }
  }

}

void evict(travStruct *ts)
{
  PMevict(ts->fEvict, ts->pageTEvict);
  PMwrite(ts->fEvictParent * PAGE_SIZE + ts->evictRow, 0);
}

uint64_t getAndEvict(travStruct *ts)
{
  getFrameToEvict(ts);
  evict(ts);
  uint64_t frameToReturn = ts->fEvict;
  setStruct(ts, true);
  return frameToReturn;
}

word_t findFrame(travStruct *ts)
{
  getMaxFrame(0, ts, 0);
  if (ts->maxFrame + 1 < NUM_FRAMES) {
    return ts->maxFrame + 1;
  }
  findEmptyFrame(ts);
  uint64_t frameToReturn;
  if (ts->fEmpty == 0) {
    frameToReturn = getAndEvict(ts);
  } else {
    PMwrite(ts->emptyParent * PAGE_SIZE + ts->fEmptyRow, 0);
    frameToReturn = ts->fEmpty;
    setStruct(ts, false);
  }
  return frameToReturn;
}

void goThroughTables(uint64_t virtualAddress, uint64_t *offset, word_t *curFrame,
                     word_t *nextFrame, uint64_t *bitsToShift, travStruct &ts)
{
  uint64_t offSetAddr;
  for (int i = 0; i < TABLES_DEPTH - 1; i++) {
    offSetAddr = (virtualAddress >> *bitsToShift);
    *offset = getOffset(offSetAddr);
    PMread((*curFrame) * PAGE_SIZE + *offset, nextFrame);
    if (!(*nextFrame)) {
      word_t f1 = findFrame(&ts);
      clearTable(f1);
      PMwrite((*curFrame) * PAGE_SIZE + *offset, f1);
      *nextFrame = f1;
    }
    *bitsToShift -= OFFSET_WIDTH;
    *curFrame = *nextFrame;
    ts.framesPath[i] = *nextFrame;
  }
}

uint64_t getPhysAdd(uint64_t virtualAddress)
{
  if (TABLES_DEPTH == 0) {
    return virtualAddress;
  }
  travStruct ts{};
  uint64_t offset = 0;
  word_t curFrame = 0;
  word_t nextFrame = 0;
  uint64_t bitsToShift = OFFSET_WIDTH * (TABLES_DEPTH);
  ts.pageNumber = virtualAddress >> OFFSET_WIDTH;
  goThroughTables(virtualAddress, &offset, &curFrame, &nextFrame, &bitsToShift, ts);
  uint64_t offSetAddr;
  offset = getOffset((virtualAddress >> bitsToShift));
  PMread(curFrame * PAGE_SIZE + offset, &nextFrame);
  if (nextFrame == 0) {
    word_t f2 = findFrame(&ts);
    PMrestore(f2, getPageNum(virtualAddress));
    PMwrite(curFrame * PAGE_SIZE + offset, f2);
    nextFrame = f2;
  }
  bitsToShift -= OFFSET_WIDTH;
  offSetAddr = (virtualAddress >> bitsToShift);
  return nextFrame * PAGE_SIZE + getOffset(offSetAddr);
}

void VMinitialize()
{
  clearTable(0);
}

bool validateAdd(uint64_t virtualAddress)
{
  if (VIRTUAL_MEMORY_SIZE == 0 || RAM_SIZE == 0 || (TABLES_DEPTH == 0 && NUM_FRAMES == 0) ||
          TABLES_DEPTH > NUM_FRAMES || (long long) virtualAddress >= VIRTUAL_MEMORY_SIZE) {
    return false;
  }
  return true;
}

int VMread(uint64_t virtualAddress, word_t *value)
{
  if (!validateAdd(virtualAddress)) {
    return 0;
  }
  uint64_t pAdd = getPhysAdd(virtualAddress);
  PMread(pAdd, value);
  return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value)
{
  if (!validateAdd(virtualAddress)) {
    return 0;
  }
  uint64_t pAdd = getPhysAdd(virtualAddress);
  PMwrite(pAdd, value);
  return 1;
}