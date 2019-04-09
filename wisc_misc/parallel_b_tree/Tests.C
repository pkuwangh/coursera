#include "Tests.h"
#include "CTree.h"
#include "Barrier.h"
#include "Transactions.h"
#include "Stats.h"

#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sys/time.h>
using namespace std;

static bool
fill_vector_random(vector<int> &ints, int n_elem)
{
  ints.resize(n_elem, NOT_IN_TREE);

  cout << "Filling ints vector..." << flush;
  for(int i=0;i<n_elem;i++) {
    ints[i] = i;
  }
  cout << "Randomizing..." << flush;
  for(int i=0;i<n_elem;i++) {
    int a = rand() % n_elem;
    int temp = ints[a];
    ints[a] = ints[i];
    ints[i] = temp;
  }
  cout << "Verifying contents..." << flush;
  vector<bool> ints_present;
  ints_present.resize(n_elem, false);
  for (int i = 0; i<n_elem; i++) {
    ints_present[ints[i]] = true;
  }
  for (int i = 0; i<n_elem; i++) {
    if (!ints_present[i]) return false;
  }
  cout << "All ints present." << endl << flush;
  return true;
}

static void
report_error(int tid, int key, int expected, int data)
{
  cout << "Thread " << tid << " found an error.  Lookup of <" << key << "," << expected << "> yielded " << data << endl;
}

bool
verify_elt(int tid, ConcurrentTree * p_tree, int key, int expected)
{
  int data = p_tree->Lookup( key );
  if( data != expected ) {
    report_error(tid, key, expected, data);
    return false;
  } else {
    return true;
  }
}

bool
testTreeSerial()
{
  ConcurrentTree * p_tree;
  p_tree = new ConcurrentTree( 1 );

  vector<int> ints;
  if (!fill_vector_random(ints, NUM_ELEMENTS)) {
    return false;
  }

  cout << "Loading tree..." << flush;
  for(int i=0;i<NUM_ELEMENTS;i++) {
    p_tree->Set( ints[i], i );
  }
  cout << "Verifying presence..." << flush;
  for(int i=0;i<NUM_ELEMENTS;i++) {
    if (!verify_elt(-1, p_tree, ints[i], i)) {
      return false;
    }
  }
  cout << "Verfied." << endl << flush;

  cout << "Deleting half of elements..." << flush;
  for(int i=0;i<NUM_ELEMENTS/2;i++) {
    p_tree->Remove( ints[i] );
  }
  cout << "Verifying deletion..." << flush;
  for(int i=0; i<NUM_ELEMENTS/2;i++) {
    if (!verify_elt(-1, p_tree, ints[i], NOT_IN_TREE)) {
      return false;
    }
  }
  cout << "Checking remaining..." << flush;
  for(int i=NUM_ELEMENTS/2; i<NUM_ELEMENTS;i++) {
    if (!verify_elt(-1, p_tree, ints[i], i)) {
      return false;
    }
  }
  cout << "Verified." << endl << flush;

  cout << "Emptying rest of tree..." << flush;
  for(int i=NUM_ELEMENTS/2; i<NUM_ELEMENTS;i++) {
    p_tree->Remove( ints[i] );
  }
  cout << "Verifying..." << flush;
  for(int i=0;i<NUM_ELEMENTS;i++) {
    if (!verify_elt(-1, p_tree, i, NOT_IN_TREE)) {
      return false;
    }
  }
  cout << "Verified."<< endl << flush;

  delete p_tree;

  return true;

}

bool testTreeParallel( ConcurrentTree * p_tree, int tid, int nThreads, PThreadLockCVBarrier& barrier ) {
  int chunksize = NUM_ELEMENTS / nThreads;
  int lower_bound = chunksize * tid;
  int upper_bound = chunksize * (tid+1);

  static vector<int> ints;

  if( tid == 0 ) {
    if (!fill_vector_random(ints, NUM_ELEMENTS)) {
      return false;
    }
  }

  barrier.Arrive();
  if( tid == 0 ) {
    cout << "Loading tree (multi-threaded)..." << flush;
  }
  barrier.Arrive();

  for(int i=0;i<chunksize;i++) {
    int key = ints[lower_bound+i];
    int data = lower_bound+i;
    p_tree->Set( key, data );
  }

  barrier.Arrive();
  if( tid == 0 ) {
    cout << "Loaded." << endl << flush;
    cout << "Verifying (single-threaded)..." << flush;
    for(int i=0;i<NUM_ELEMENTS;i++) {
      if (!verify_elt(-1, p_tree, ints[i], i)) {
        return false;
      }
    }
    cout << "Verfied." << endl << flush;
    cout << "Verifying (multi-threaded)..." << flush;
  }
  barrier.Arrive();

  for(int i=0;i<NUM_ELEMENTS;i++) {
    if (!verify_elt(tid, p_tree, ints[i], i)) {
      return false;
    }
  }

  barrier.Arrive();
  if( tid == 0 ) {
    cout << "Verified. " << endl;
    cout << "Deleting every other element (multi-threaded)..." << flush;
  }
  barrier.Arrive();

  for(int i=0;i<chunksize;i++) {
    if (ints[lower_bound+i] % 2) {
      p_tree->Remove( ints[lower_bound+i] );
    }
  }

  barrier.Arrive();
  if( tid == 0 ) {
    cout << "Done." << endl;
    cout << "Verifying (single-threaded)..." << flush;
    for(int i=0;i<NUM_ELEMENTS;i++) {
      int expected = (ints[i] % 2) ? NOT_IN_TREE : i;
      if (!verify_elt(-1, p_tree, ints[i], expected)) {
        return false;
      }
    }
    cout << "Verfied." << endl << flush;
    cout << "Verifying (multi-threaded)..." << flush;
  }
  barrier.Arrive();

  for(int i=0;i<NUM_ELEMENTS;i++) {
    int expected = (ints[i] % 2) ? NOT_IN_TREE : i;
    if (!verify_elt(tid, p_tree, ints[i], expected)) {
      return false;
    }
  }

  barrier.Arrive();
  if( tid == 0 ) {
    cout << "Done." << endl;
    cout << "Reloading tree (multi-threaded) for transactional tests..." << flush ;
  }
  barrier.Arrive();



  for(int i=0;i<chunksize;i++) {
    if (ints[lower_bound+i] % 2) {
      int key = ints[lower_bound+i];
      int data = lower_bound+i;
      p_tree->Set( key, data );
    }
  }

  barrier.Arrive();
  if( tid == 0 ) {
    cout << "Done." << endl << flush;
    cout << "Verifying (single-threaded)..." << flush;
    for(int i=0;i<NUM_ELEMENTS;i++) {
      if (!verify_elt(-1, p_tree, ints[i], i)) {
        return false;
      }
    }
    cout << "Verfied." << endl << flush;
  }
  barrier.Arrive();

  return true;
}

volatile int transactions_completed = 0;
volatile int true_sum;
std::mutex testingLock;

bool testTreeTransactional( ConcurrentTree * p_tree, int tid, int nThreads, PThreadLockCVBarrier& barrier ) {

  bool success = true;
  int sum;

  if( tid == 0 ) {
    true_sum = doTortureScan( p_tree );
    gettimeofday( &starttime, NULL );
  }

  barrier.Arrive();

  /*
   * It just so happens that the tree will sum to true_sum if serializability was met.
   * Sum = true_sum doesn't GUARANTEE that serializability was met, but Sum != true_sum
   * definately indicates serializability WASNT met.
   */

  /* Note that the stopping criteria is not exact */
  while( transactions_completed < NUM_TRANSACTIONS ) {
    char t = randomTransaction();
    switch(t) {
    case SCAN:               sum = doTortureScan( p_tree );
                             if( sum != true_sum ) {
                               testingLock.lock();
                               cout << "Thread " << tid << " detected a serializability violation. (sum = " << sum << ", true_sum=" << true_sum << ")" << endl;
                               testingLock.unlock();
                               success = false;
                             }
                             INCREMENT_STAT( nScans );
                             break;
    case UPDATE:             doTortureUpdate( p_tree );
                             INCREMENT_STAT( nUpdates );
                             break;
    case LOOKUP:             doTortureLookup( p_tree );
                             INCREMENT_STAT( nLookups );
                             break;
    case CONDITIONAL_ADD:    doTortureConditionalAdd( p_tree );
                             INCREMENT_STAT( nCAdds );
                             break;
    case CONDITIONAL_REMOVE: doTortureConditionalRemove( p_tree );
                             INCREMENT_STAT( nCRemoves );
                             break;
    default:
      success = false; break;
    }

    /* Another one bites the dust... */
    testingLock.lock();
    transactions_completed++;
    // Uncomment this if you're paranoid about forward progress
    if( transactions_completed % 50 == 0 ) cout << "Thread " << tid << " completed transaction " << transactions_completed << endl;
    testingLock.unlock();
    INCREMENT_STAT( nCommits );
  }

  barrier.Arrive();

  if( tid == 0 ) {
    gettimeofday( &endtime, NULL );

    /*
     * It just so happens that the tree will sum to true_sum if serializability was met.
     * Sum = true_sum doesn't GUARANTEE that serializability was met, but Sum != true_sum
     * definately indicates serializability WASNT met.
     */
    sum = doTortureScan( p_tree );
    if( sum != true_sum ) {
		cout << "Thread " << tid << " detected a serializability violation, sum=" << sum << ", true_sum=" << true_sum << endl;
      success = false;
    }
  }

  return success;
}

void loadTreeRange( ConcurrentTree * p_tree, int lb, int ub ) {
  int length = ub - lb;
  if( length <= 3 ) {
    switch(length) {
    case 0: break;
    case 3: p_tree->Set( lb+2, lb+2 );
    case 2: p_tree->Set( lb+1, lb+1 ); /* Intentionally no breaks here ... */
    case 1: p_tree->Set( lb, lb );
    default: break;
    }
  } else {
    int lower_lb, lower_ub, upper_lb, upper_ub;
    lower_lb = lb;
    upper_ub = ub;
    lower_ub = (length/2) + lower_lb;
    upper_lb = lower_ub+1;

    p_tree->Set( lower_ub, lower_ub );

    loadTreeRange( p_tree, lower_lb, lower_ub );
    loadTreeRange( p_tree, upper_lb, upper_ub );
  }
}

bool testTreeThroughput( ConcurrentTree * p_tree, int tid, int nThreads,
                         PThreadLockCVBarrier& barrier ) {
  transactions_completed = 0;
  bool success = true;
  int sum;

  barrier.Arrive();

  /* Load the tree... do this part serially for maximum speed */
  if( tid == 0 ) {
    cout << "Loading the tree for throughput tests..." << flush;
    loadTreeRange( p_tree, 0, NUM_ELEMENTS_THROUGHPUT );
    cout << "Done. " << endl << flush;

    gettimeofday( &starttime, NULL );
  }

  barrier.Arrive();
  barrier.Arrive();

  /*
   * It just so happens that the tree will sum to true_sum if serializability was met.
   * Sum = true_sum doesn't GUARANTEE that serializability was met, but Sum != true_sum
   * definately indicates serializability WASNT met.
   */

  /* Note that the stopping criteria is not exact */
  while( transactions_completed < NUM_TRANSACTIONS ) {
    char t = randomTransaction();
    switch(t) {
    case SCAN:               sum = doScan( p_tree );
                             INCREMENT_STAT( nScans );
                             break;
    case UPDATE:             doUpdate( p_tree );
                             INCREMENT_STAT( nUpdates );
                             break;
    case LOOKUP:             doLookup( p_tree );
                             INCREMENT_STAT( nLookups );
                             break;
    case CONDITIONAL_ADD:    doConditionalAdd( p_tree );
                             INCREMENT_STAT( nCAdds );
                             break;
    case CONDITIONAL_REMOVE: doConditionalRemove( p_tree );
                             INCREMENT_STAT( nCRemoves );
                             break;
    default:
      success = false; break;
    }

    /* Another one bites the dust... */
    testingLock.lock();
    transactions_completed++;
    // Uncomment this if you're paranoid about forward progress
    if( transactions_completed % 50 == 0 ) cout << "Thread " << tid << " completed transaction " << transactions_completed << endl;
    testingLock.unlock();
    INCREMENT_STAT( nCommits );
    usleep( INTER_TRANSACTION_SLEEP_TIME );
  }

  barrier.Arrive();

  if( tid == 0 ) {
    gettimeofday( &endtime, NULL );
  }

  barrier.Arrive();
  return success;
}

