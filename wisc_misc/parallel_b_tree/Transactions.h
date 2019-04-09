#ifndef TRANSACTIONS_H
#define TRANSACTIONS_H

#include <unistd.h>

using namespace std;

class ConcurrentTree;

/* Defines the names of the transactions */
const char SCAN               = 0;
const char UPDATE             = 1;
const char LOOKUP             = 2;
const char CONDITIONAL_ADD    = 3;
const char CONDITIONAL_REMOVE = 4;

/* Defines the relative likelyhood of a given transaction -- the higher the more likely the transaction */
const int SCAN_WEIGHT               = 1;
const int UPDATE_WEIGHT             = 50;
const int LOOKUP_WEIGHT             = 50;
const int CONDITIONAL_ADD_WEIGHT    = 50;
const int CONDITIONAL_REMOVE_WEIGHT = 0;
const int WEIGHT_SUM = CONDITIONAL_REMOVE_WEIGHT + CONDITIONAL_ADD_WEIGHT + LOOKUP_WEIGHT + UPDATE_WEIGHT + SCAN_WEIGHT;

const useconds_t INTER_ATOMIC_SLEEP_TIME = 100; /* Not used for scan */
const useconds_t INTER_TRANSACTION_SLEEP_TIME = 10000;

/*
 * Defines the number of elements accessed by the udpate and lookup
 * transactions -- MUST BE AN EVEN NUMBER
 */
const int SMALL_TRANSACTION_SIZE = 20;

void initSeed (int thread_id);
char randomTransaction();

int  doTortureScan( ConcurrentTree * p_tree );
void doTortureUpdate( ConcurrentTree * p_tree );
int  doTortureLookup( ConcurrentTree * p_tree );
void doTortureConditionalAdd( ConcurrentTree * p_tree );
void doTortureConditionalRemove( ConcurrentTree * p_tree );

int  doScan( ConcurrentTree * p_tree );
void doUpdate( ConcurrentTree * p_tree );
int  doLookup( ConcurrentTree * p_tree );
void doConditionalAdd( ConcurrentTree * p_tree );
void doConditionalRemove( ConcurrentTree * p_tree );


#endif // #ifndef TRANSACTIONS_H
