#ifndef STATS_H
#define STATS_H

#include <mutex>
#include <time.h>

extern std::mutex statslock;

#define INCREMENT_STAT( name ) { statslock.lock(); name++; statslock.unlock(); }

extern int nScans;
extern int nUpdates;
extern int nLookups;
extern int nCAdds;
extern int nCRemoves;

extern int nScanAborts;
extern int nUpdateAborts;
extern int nLookupAborts;
extern int nCAddAborts;
extern int nCRemoveAborts;

extern struct timeval starttime;
extern struct timeval endtime;

extern int nAborts;
extern int nCommits;

void clearStats();
void printStats();

#endif

