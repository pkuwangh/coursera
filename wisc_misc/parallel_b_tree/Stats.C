#include "Stats.h"

#include <stdlib.h>
#include <iostream>
#include <iomanip>

using namespace std;

mutex statslock;

int nScans = 0;
int nUpdates = 0;
int nLookups = 0;
int nCAdds = 0;
int nCRemoves = 0;

int nScanAborts = 0;
int nUpdateAborts = 0;
int nLookupAborts = 0;
int nCAddAborts = 0;
int nCRemoveAborts = 0;

struct timeval starttime;
struct timeval endtime;

int nAborts = 0;
int nCommits = 0;

void clearStats() {
    nScans = 0;
    nUpdates = 0;
    nLookups = 0;
    nCAdds = 0;
    nCRemoves = 0;

    nScanAborts = 0;
    nUpdateAborts = 0;
    nLookupAborts = 0;
    nCAddAborts = 0;
    nCRemoveAborts = 0;

    nAborts = 0;
    nCommits = 0;

}

void printStats() {

    cout << "TOTAL COMMITS: " << nCommits << endl << "\t";
    cout << "  Scan=" << nScans;
    cout << "  Update=" << nUpdates;
    cout << "  Lookup=" << nLookups;
    cout << "  C-Add=" << nCAdds;
    cout << "  C-Remove=" << nCRemoves;
    cout << endl;
    cout << "Aborts=" << nAborts << endl << "\t";
    cout << "  Scan=" << nScanAborts;
    cout << "  Update=" << nUpdateAborts;
    cout << "  Lookup=" << nLookupAborts;
    cout << "  C-Add=" << nCAddAborts;
    cout << "  C-Remove=" << nCRemoveAborts << endl;
    cout << endl;

    long long start_usecs   = starttime.tv_sec * 1000000 + starttime.tv_usec;
    long long   end_usecs   =   endtime.tv_sec * 1000000 +   endtime.tv_usec;
    long long elapsed_usecs = end_usecs - start_usecs;
    double elapsedtime = ((double) elapsed_usecs) / 1000000.0;

    cout << "Elapsed Time: " << elapsedtime << " s" << endl;
    double tps = ((double) nCommits ) / (( double) elapsedtime );
    cout << "Throughput: " << tps << " transactions per second." << endl;
    cout << endl;
}


