#include "fatals.h"
#include "ProcMap.h"
#include "Barrier.h"
#include "CTree.h"
#include "Tests.h"
#include "Transactions.h"
#include "Stats.h"

#include <iostream>
#include <cassert>
#include <iomanip>
#include <thread>
#include <vector>

#define SERIAL_TEST
#define PARALLEL_TEST
#define TRANSACTIONAL_TEST
#define THROUGHPUT_TEST

#ifdef TRANSACTIONAL_TEST
#ifndef PARALLEL_TEST
#error PARALLEL_TEST must be defined if TRANSACTIONAL_TEST is defined -- the parallel tests load the data structre.
#endif
#endif

using namespace std;

ProcessorMap * p_map = NULL;

class ThreadGoodies {
  public:
    int myID;
    int nThreads;
    PThreadLockCVBarrier * p_barrier;

  private:
};

volatile ConcurrentTree * p_concurrent_tree = NULL;

void thread_target_function(int myID, int nThreads, PThreadLockCVBarrier *p_barrier) {
    ConcurrentTree * p_tree;

    // bind thread id to physical cpu
    p_map->BindToPhysicalCPU( p_map->LogicalToPhysical( myID ) );
    initSeed (myID);

#ifdef SERIAL_TEST
    p_barrier->Arrive();

    if( myID == 0 ) {
        cout << "Beginning single-threaded tree tests." << endl;
        if( testTreeSerial() ) {
            cout << "Passed single-threaded tests." << endl;
        } else {
            fatal("Failed single-theaded tests.\n");
        }

        cout << endl;
    }
#endif // #ifdef SERIAL_TEST

#ifdef PARALLEL_TEST
    if( myID == 0 ) {
        cout << "Beginning multi-threaded tree tests. (this part is not deterministic)" << endl;
        p_concurrent_tree = new ConcurrentTree( nThreads );
    }
    p_barrier->Arrive();

    p_tree = (ConcurrentTree*) p_concurrent_tree;
    if( testTreeParallel( p_tree, myID, nThreads, *p_barrier ) ) {
        if( myID == 0 ) {
            cout << "Passed multi-threaded tests." << endl;
        }
    } else {
        fatal("Failed parallel tests -- exit initiated by thread %i\n", myID );
    }

#endif // #ifdef PARALLEL_TEST
#ifdef TRANSACTIONAL_TEST

    if( myID == 0 ) {
        clearStats();
        cout << "Beginning Transactional Torture Tests. " << endl;
    }

    p_barrier->Arrive();

    p_tree = (ConcurrentTree*) p_concurrent_tree;
    if( testTreeTransactional( p_tree, myID, nThreads, *p_barrier ) ) {
        if( myID == 0 ) {
            cout << "Passed Transactional tests." << endl << flush;;
        }
    } else {
        printStats();
        fatal("Failed Transactional tests -- exit initiated by thread %i\n", myID );
    }

    p_barrier->Arrive();
#endif // #ifdef TRANSACTIONAL_TEST
#ifdef PARALLEL_TEST
    if( myID == 0 ) {
        cout << "Transactional Torture Test Stats:" << endl;
        printStats( );
        cout << flush;
    }
#endif // #ifdef PARALLEL_TEST
#ifdef THROUGHPUT_TEST
    p_barrier->Arrive();
    if( myID == 0 ) {
        clearStats();
        p_concurrent_tree = new ConcurrentTree( nThreads );
    }

    p_barrier->Arrive();

    p_tree = (ConcurrentTree*) p_concurrent_tree;
    if( testTreeThroughput(  p_tree, myID, nThreads, *p_barrier ) ) {
        if( myID == 0 ) {
            cout << "Passed Throughput tests." << endl;
        }
    } else {
        printStats();
        fatal("Failed Throughput tests -- exit initiated by thread %i\n", myID );
    }
    p_barrier->Arrive();
    if( myID == 0 ) {
        cout << "Transactional Throughput Test Stats:" << endl;
        printStats( );
        cout << flush;
    }
#endif

    p_barrier->Arrive();
}

int main( int argc, char * argv[] ) {

    p_map = new ProcessorMap();

    int procs =  p_map->NumberOfProcessors();
    cout << "This machine has " << procs << " processors online. Their numbers are:" << endl;
    cout << setw(20) << "Logical Processor #" << "  Physical Processor #" << endl;

    for( int i=0;i<procs;i++) {
        cout << setw(20) << i << setw(20) << p_map->LogicalToPhysical(i) << setw(0) << endl;
    }

    cout << endl;
    if( NUM_ELEMENTS % procs ) {
        fatal("Please select NUM_ELEMENTS(%i) to divide evenly among all processors\n", NUM_ELEMENTS);
    }

    if( SMALL_TRANSACTION_SIZE % 2 ) {
        fatal("SMALL_TRANSACTION_SIZE(%i) must be even.\n",SMALL_TRANSACTION_SIZE);
    }

    std::vector<std::thread *> threads;
    int thread_number;
    PThreadLockCVBarrier * barrier = new PThreadLockCVBarrier( procs );

    int seed = time(NULL);
    cout << "Random seed is: " << seed << endl;


    for(thread_number=0;thread_number<procs;thread_number++) {
        threads.push_back(new std::thread(thread_target_function, thread_number,
                    procs, barrier));
    }

    for(thread_number=0;thread_number<procs;thread_number++) {
        threads[thread_number]->join();
    }

    delete barrier;
    delete p_map;

    return 0;
}

