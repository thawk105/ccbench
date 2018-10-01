#include <algorithm>
#include <sys/time.h>
#include <stdio.h>
#include <fstream>
#include <string>
#include <vector>
#include <xmmintrin.h>

#include "include/transaction.hpp"
#include "include/common.hpp"
#include "include/timeStamp.hpp"
#include "include/debug.hpp"
#include "include/version.hpp"
#include "include/tsc.hpp"

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displaySLogSet();
extern void displayDB();

using namespace std;

#define MSK_TID 0b11111111

void Transaction::tbegin(bool ronly)
{
	if (ronly) this->ronly = true;
	else this->ronly = false;

	this->status = TransactionStatus::inflight;
	this->wts.generateTimeStamp(thid);
	__atomic_store_n(&(ThreadWtsArray[thid].num), this->wts.ts, __ATOMIC_RELEASE);
	this->rts = MinWts.load(std::memory_order_acquire) - 1;
	__atomic_store_n(&(ThreadRtsArray[thid].num), this->rts, __ATOMIC_RELEASE);

	//one-sided synchronization
	stop = rdtsc();
	if (chkClkSpan(start, stop, 10 * CLOCK_PER_US)) {
		uint64_t maxwts;
	   	maxwts = __atomic_load_n(&(ThreadWtsArray[1].num), __ATOMIC_ACQUIRE);
		//record the fastest one, and adjust it.
		//one-sided synchronization
		uint64_t check;
		for (unsigned int i = 2; i < THREAD_NUM; ++i) {
			check = __atomic_load_n(&(ThreadWtsArray[i].num), __ATOMIC_ACQUIRE);
			if (maxwts < check) maxwts = check;
		}
		maxwts = maxwts & (~MSK_TID);
		maxwts = maxwts | thid;
		this->wts.ts = maxwts;
		__atomic_store_n(&(ThreadWtsArray[thid].num), this->wts.ts, __ATOMIC_RELEASE);

		//modify the start position of stopwatch.
		start = rdtsc();
	}

}

int
Transaction::tread(unsigned int key)
{
	//read-own-writes
	//if n E write set
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) {
			return (*itr).newObject->val;
		}
	}
	// if n E read set
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) {
			return (*itr).ver->val;
		}
	}

	//else
	uint64_t trts;
	if (this->ronly) {
		trts = this->rts;
	}
	else	trts = this->wts.ts;
	//-----

	//Search version
	Version *version = Table[key].latest;
	while (version->wts.load(memory_order_acquire) > trts) {
		version = version->next.load(std::memory_order_acquire);
		//if version is not found.
		if (version == nullptr) {
			ERR;
		} 
	}
	
	//timestamp condition is ok. and check status
	VersionStatus loadst = version->status.load(memory_order_acquire);
	while (loadst != VersionStatus::committed && loadst != VersionStatus::precommitted) {
		if (loadst != VersionStatus::aborted) {
			if (NLR) {
				//spin wait
				if (GROUP_COMMIT) {
				this->start = rdtsc();
					while (version->status.load(std::memory_order_acquire) == VersionStatus::pending){
						this->stop = rdtsc();
						if (chkClkSpan(this->start, this->stop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
							earlyAbort();
							return false;
						}
					}
				} else {
					while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {}
				}
				if (version->status.load(std::memory_order_acquire) == VersionStatus::committed) break;
			} else if (ELR) {
				while (loadst == VersionStatus::pending) {
					loadst = version->status.load(memory_order_acquire);		
				}
				if (loadst == VersionStatus::committed || loadst == VersionStatus::precommitted) break;
			}
		}

		version = version->next.load(memory_order_acquire);
		loadst = version->status.load(memory_order_acquire);
		//if version is not found.
		if (version == nullptr) {
			ERR;
		}
	}

	//hit version
	//if read-only, not track or validate readSet
	if (this->ronly) {
		return version->val;
	}

	readSet.push_back(ReadElement(key, version));
	return version->val;
}

void
Transaction::twrite(unsigned int key,  unsigned int val)
{
	//if n E writeSet
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) {
			(*itr).newObject->val = val;
			return;
		}
	}
		
	Version *version = Table[key].latest.load();

	//Search version (for early abort check)
	//search latest (committed or pending) version and use for early abort check
	//pending version may be committed, so use for early abort check.
	VersionStatus loadst = version->status.load(memory_order_acquire);
	// it uses loadst for reducing a number of atomic-load.
	while (loadst == VersionStatus::aborted) {
		version = version->next.load(std::memory_order_acquire);
		loadst = version->status.load(memory_order_acquire);

		//if version is not found.
		if (version == nullptr) ERR;
	}
	//hit version

	//early abort check(EAC)
	//here, version->status is commit or pending.
	if ((version->rts.load(memory_order_acquire) > this->wts.ts) && (loadst == VersionStatus::committed)) {
		//it must be aborted in validation phase, so early abort.
		this->status = TransactionStatus::abort;
		return;	
	}
	else if (version->wts.load(memory_order_acquire) > this->wts.ts) {
		//if pending, it don't have to be aborted.
		//But it may be aborted, so early abort.
		//if committed, it must be aborted in validaiton phase due to order of newest to oldest.
		this->status = TransactionStatus::abort;
		return;	
	}

	if (NLR) {
		while (loadst != VersionStatus::committed) {
			while (loadst == VersionStatus::pending) {
				//spin-wait
				//if nlr & group commit, it happens dead lock.
				//dead lock prevention is time out.
				if (GROUP_COMMIT) {
					uint64_t spinstart = rdtsc();
					uint64_t spinstop;
					while (loadst == VersionStatus::pending){
						spinstop = rdtsc();
						if (chkClkSpan(spinstart, spinstop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
							earlyAbort();
							return;
						}
						loadst = version->status.load(memory_order_acquire);
					}
				}	
				loadst = version->status.load(memory_order_acquire);
			}
			if (loadst == VersionStatus::committed) break;
		
			//status == aborted
			version = version->next.load(std::memory_order_acquire);
			loadst = version->status.load(memory_order_acquire);
			if (version == nullptr) {
				ERR;
			}
		}
	} 
	else if (ELR) {
		//here, the status is pending or committed or precomitted.
		while (loadst == VersionStatus::pending) 
			loadst = version->status.load(std::memory_order_acquire);

		while (loadst != VersionStatus::precommitted && loadst != VersionStatus::committed) {
			//status == aborted
			version = version->next.load(std::memory_order_acquire);
			if (version == nullptr) ERR;
			loadst = version->status.load(memory_order_acquire);
		}
	}

	Version *newObject;
   	newObject = new Version(0, this->wts.ts, key, val);
	writeSet.push_back(WriteElement(key, version, newObject));
	return;
}

bool
Transaction::validation() 
{
	//Install pending version
	sort(writeSet.begin(), writeSet.end());
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		
		Version *expected;
		for (;;) {
			expected = Table[(*itr).key].latest.load();
RETRY_VALI:
			Version *version = expected;

			VersionStatus loadst = version->status.load(memory_order_acquire);
			while (loadst != VersionStatus::committed && loadst != VersionStatus::precommitted) {

				while (loadst == VersionStatus::aborted) {
					version = version->next.load(std::memory_order_acquire);
					loadst = version->status.load(memory_order_acquire);
				}

				//to avoid cascading aborts.
				//install pending version step blocks concurrent transactions that share the same visible version and have higher timestamp than (tx.ts).
				if (NLR && GROUP_COMMIT) {
					uint64_t spinstart = rdtsc();
					uint64_t spinstop;
					while (loadst == VersionStatus::pending) {
						spinstop = rdtsc();
						if (chkClkSpan(spinstart, spinstop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
							return false;
						}
						loadst = version->status.load(memory_order_acquire);
					}
				} else {
					while (loadst == VersionStatus::pending) 
						loadst = version->status.load(memory_order_acquire);
				}
				//now, version->status is not pending.
			}	

			//to keep versions ordered by wts in the version list
			//if version is pending version, concurrent transaction that has lower timestamp is aborted.
			if (this->wts.ts < version->wts.load(memory_order_acquire)) {
				return false;
			}

			(*itr).newObject->status.store(VersionStatus::pending, memory_order_release);
			(*itr).newObject->next.store(Table[(*itr).key].latest.load(), memory_order_release);
			if (Table[(*itr).key].latest.compare_exchange_strong(expected, (*itr).newObject, memory_order_acq_rel, memory_order_acquire)) break;
			else goto RETRY_VALI;
		}
	}

	//Read timestamp update
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t expected;
		for (;;) {
			expected = (*itr).ver->rts.load(memory_order_acquire);
RETRY_UPDATE:
			if (expected >= this->wts.ts) break;
			if ((*itr).ver->rts.compare_exchange_strong(expected, this->wts.ts, memory_order_acq_rel, memory_order_acquire)) break;
			else goto RETRY_UPDATE;
		}
	}

	//validation consistency check
	//(a) every previously visible version v of the records in the read set is the currently visible version to the transaction.
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if (ELR) {
			if ((*itr).ver->status.load(std::memory_order_acquire) != VersionStatus::committed && (*itr).ver->status.load(std::memory_order_acquire) != VersionStatus::precommitted) {
				return false;
			}
		} else {
			if ((*itr).ver->status.load(std::memory_order_acquire) != VersionStatus::committed) {
				return false;
			}
		}
	}

	//(b) every currently visible version v of the records in the write set satisfies (v.rts) <= (tx.ts)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).sourceObject->rts.load(memory_order_acquire) > this->wts.ts) {
			return false;
		}
	}

	return true;
}

void
Transaction::swal()
{
	if (!GROUP_COMMIT) {	//non-group commit
		SwalLock.w_lock();

		int i = 0;
		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
			SLogSet[i] = (*itr).newObject;
			i++;
		}

		double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
		uint64_t start = rdtsc();
		while ((rdtsc() - start) < threshold) {}	//spin-wait
		ThreadFlushedWts[this->thid].ts = this->wts.ts;

		SwalLock.w_unlock();
	}
	else {	//group commit
		SwalLock.w_lock();
		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
			SLogSet[GROUP_COMMIT_INDEX[0].num] = (*itr).newObject;
			GROUP_COMMIT_INDEX[0].num++;
		}

		if (GROUP_COMMIT_COUNTER[0].num == 0) {
			GCommitStart[0].num = rdtsc();	//最初の初期化もここで代用している
		}

		GROUP_COMMIT_COUNTER[0].num++;

		if (GROUP_COMMIT_COUNTER[0].num == GROUP_COMMIT) {
			double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
			uint64_t start = rdtsc();
			while ((rdtsc() - start) < threshold) {}	//spin-wait
			ThreadFlushedWts[this->thid].ts = this->wts.ts;

			//group commit pending version.
			gcpv();
		}
		SwalLock.w_unlock();
	}
}

void
Transaction::pwal()
{
	if (!GROUP_COMMIT) {
		int i = 0;
		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
			PLogSet[thid][i] = (*itr).newObject;
			i++;
		}

		//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
		double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
		uint64_t start = rdtsc();
		while ((rdtsc() - start) < threshold) {}	//spin-wait
		ThreadFlushedWts[this->thid].ts = this->wts.ts;
	} 
	else {
		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
			PLogSet[thid][GROUP_COMMIT_INDEX[thid].num] = (*itr).newObject;
			GROUP_COMMIT_INDEX[this->thid].num++;
		}

		if (GROUP_COMMIT_COUNTER[this->thid].num == 0) {
			GCommitStart[this->thid].num = rdtsc();	//最初の初期化もここで代用している
			ThreadRtsArrayForGroup[this->thid].num = this->rts;
		}

		GROUP_COMMIT_COUNTER[this->thid].num++;

		if (GROUP_COMMIT_COUNTER[this->thid].num == GROUP_COMMIT) {
			//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
			double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
			uint64_t start = rdtsc();
			while ((rdtsc() - start) < threshold) {}	//spin-wait	
			ThreadFlushedWts[this->thid].ts = this->wts.ts;

			gcpv();
		} 
	}
}

inline void
Transaction::cpv()	//commit pending versions
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		(*itr).newObject->status.store(VersionStatus::committed, std::memory_order_release);
	}

}

void
Transaction::precpv() 
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		(*itr).newObject->status.store(VersionStatus::precommitted, std::memory_order_release);
	}

}
	
void 
Transaction::gcpv()
{
	if (S_WAL) {
		for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].num; ++i) {
			SLogSet[i]->status.store(VersionStatus::committed, memory_order_release);
		}
		GROUP_COMMIT_COUNTER[0].num = 0;
		GROUP_COMMIT_INDEX[0].num = 0;
	}
	else if (P_WAL) {
		for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[thid].num; ++i) {
			PLogSet[thid][i]->status.store(VersionStatus::committed, memory_order_release);
		}
		GROUP_COMMIT_COUNTER[thid].num = 0;
		GROUP_COMMIT_INDEX[thid].num = 0;
	}
}

void 
Transaction::earlyAbort()
{
	writeSet.clear();
	readSet.clear();

	if (GROUP_COMMIT) {
		chkGcpvTimeout();
	}

	this->wts.set_clockBoost(CLOCK_PER_US);
	this->status = TransactionStatus::abort;
}

void
Transaction::abort()
{
	//pending versionのステータスをabortedに変更
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		(*itr).newObject->status.store(VersionStatus::aborted, std::memory_order_release);
	}

	if (GROUP_COMMIT) {
		chkGcpvTimeout();
	}

	readSet.clear();
	writeSet.clear();

	this->wts.set_clockBoost(CLOCK_PER_US);
}

void
Transaction::displayWset()
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		printf("%d ", (*itr).newObject->key);
	}
	cout << endl;
}

bool
Transaction::chkGcpvTimeout()
{
	if (P_WAL) {
		GCommitStop[this->thid].num = rdtsc();
		if (chkClkSpan(GCommitStart[this->thid].num, GCommitStop[this->thid].num, GROUP_COMMIT_TIMEOUT_US * CLOCK_PER_US)) {
			gcpv();
			return true;
		}
	}
	else if (S_WAL) {
		GCommitStop[0].num = rdtsc();
		if (chkClkSpan(GCommitStart[0].num, GCommitStop[0].num, GROUP_COMMIT_TIMEOUT_US * CLOCK_PER_US)) {
			SwalLock.w_lock();
			gcpv();
			SwalLock.w_unlock();

			return true;
		}
	}

	return false;
}

void 
Transaction::mainte()
{
	//Maintenance
	//Schedule garbage collection
	//Declare quiescent state
	//Collect garbage created by prior transactions
	
	//バージョンリストにおいてMinRtsよりも古いバージョンの中でコミット済み最新のもの
	//以外のバージョンは全てデリート可能。絶対に到達されないことが保証される.
	//

	if (GCFlag[thid].num == 0) {
		while (gcq.size() > 0) {
			if (gcq.front().wts >= MinRts.load(memory_order_acquire)) break;
		
			// (a) acquiring the garbage collection lock succeeds
			uint8_t zero = 0;
			if (!Table[gcq.front().key].gClock.compare_exchange_weak(zero, this->thid)) {
				// fail acquiring the lock
				gcq.pop();
				continue;
			}

			// (b) v.wts > record.min_wts
			if (gcq.front().wts <= Table[gcq.front().key].min_wts) {
				gcq.pop();
				continue;
				break;
			}
			//this pointer may be dangling.

			Version *delTarget = gcq.front().ver->next.load(std::memory_order_acquire);

			// the thread detaches the rest of the version list from v
			gcq.front().ver->next.store(nullptr, std::memory_order_release);
			// updates record.min_wts
			Table[gcq.front().key].min_wts.store(gcq.front().ver->wts, memory_order_release);

			while (delTarget != nullptr) {
				//nextポインタ退避
				Version *tmp = delTarget->next.load(std::memory_order_acquire);
				delete delTarget;
				delTarget = tmp;
			}
			
			// releases the lock
			Table[gcq.front().key].gClock.store(0, memory_order_release);
			gcq.pop();
		}
	}

	this->GCstop = rdtsc();
	if (chkClkSpan(this->GCstart, this->GCstop, GC_INTER_US)) {
		GCFlag[thid].num = 1;
		this->GCstart = rdtsc();
	}
}

void
Transaction::writePhase()
{
	//early lock release
	//ログを書く前に保有するバージョンステータスをprecommittedにする．
	if (ELR)  precpv();

	//write phase
	//log write set & possibly group commit pending version
	if (P_WAL) pwal();
	if (S_WAL) swal();

	if (!GROUP_COMMIT) {
		cpv();
	} else {
		//check time out of commit pending versions
		chkGcpvTimeout();
	}

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		gcq.push(GCElement((*itr).key, (*itr).newObject, (*itr).newObject->wts));
	}

	//ここまで来たらコミットしている
	this->wts.set_clockBoost(0);
	readSet.clear();
	writeSet.clear();
}

