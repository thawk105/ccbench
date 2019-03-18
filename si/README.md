# Snapshot Isolation

 This protocol follow two rules.
 1. Read operation reads the committed version which was latest at beginning of the transaction.
 2. First Updater (Committer) Wins Rule.
 
First Updater (Commiter) Wins Rule enforces that when two or more transactions execute a write operation on the same record, the lifetimes of those transactions do not overlap.

Normally this protocol gets a unique value from the shared counter at the start of the transaction and at the end of the transaction.
However, touching the shared counter twice in lifetime of transaction causes very high contention for the counter and degrading performance.

Author (tanabe) gives two selection which are able to be defined at Makefile.
 1. `-DCCTR_TW` means "Centralized counter is touched twice in the lifetime of transaction." It is normally technique.
 2. `-DCCTR_ON` means "Centralized counter is touched once in the lifetime of transaction." It is special technique. Counting up of shared counter only happen when a transaction commits. Instead of getting the count from the shared counter at the start of the transaction, get the latest commit timestamps of all the worker threads via the transaction mapping table. The element of the table may be refered by multiple concurrent worker thread. So getting and updating information are done by CAS. This technique reduces contentions for shared counter but increases overhead of memory management.

Author observed that author's technique `-DCCTR_ON` was better than `-DCCTR_TW` in some YCSB-A,C. So normally it is better to set `-DCCTR_ON`.
