#include "include/transaction.hpp"
#include "include/common.hpp"
#include "include/timeStamp.hpp"
#include "include/debug.hpp"
#include "include/version.hpp"
#include "include/tsc.hpp"
#include <algorithm>
#include <sys/time.h>
#include <stdio.h>
#include <fstream>
#include <string>
#include <vector>

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displaySLogSet();
extern void displayDB();

using namespace std;

void Transaction::tbegin(const unsigned int transactionNum)
{
	this->transactionNum = transactionNum;
	if (Pro[transactionNum][0].ronly) this->ronly = true;

	this->status = TransactionStatus::inflight;
	this->wts->generateTimeStamp(thid);
	this->rts->ts = MinWts.load(std::memory_order_acquire) - 1;

	//one-sided synchronization
	stop = rdtsc();
	if (chkClkSpan(start, stop, 100 * CLOCK_PER_US)) {
		unsigned int maxwIndex = 1;
		//最速のものを記録し、それに合わせる
		//one-sided synchronization
		for (unsigned int i = 2; i < THREAD_NUM; ++i) {
			if (ThreadWtsArray[maxwIndex]->ts < ThreadWtsArray[i]->ts) maxwIndex = i;
		}
		this->wts->ts = ThreadWtsArray[maxwIndex]->ts;

		//stopwatchの起点修正
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
	if (this->ronly) trts = this->rts->ts;
	else	trts = this->wts->ts;
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	//Search version
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	//簡単にスキップできるもの(先頭の方にある使えないバージョンの固まり等)は先にスキップする．
	Version *version = Table[key].latest;
	while ((version->wts.load(memory_order_acquire) > trts) || (version->status.load(std::memory_order_acquire) == VersionStatus::aborted)) {
		version = version->next.load(std::memory_order_acquire);
		//if version is not found.
		if (version == nullptr) {
			ERR;
		} 
	}
	
	//ここからは簡単にスキップできない．
	while (version->status.load(std::memory_order_acquire) != VersionStatus::committed && version->status.load(std::memory_order_acquire) != VersionStatus::precommitted) {
		//タイムスタンプ的には適用可能なので，あとはステータスが重要．
		if (version->status.load(std::memory_order_acquire) != VersionStatus::aborted) {
			if (NLR) {
				//pendingへの対応
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
				//プレコミットを利用．
				while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {}
				if (version->status.load(std::memory_order_acquire) == VersionStatus::committed || version->status.load(std::memory_order_acquire) == VersionStatus::precommitted) break;
			}
		}

		version = version->next.load(std::memory_order_acquire);
		//if version is not found.
		if (version == nullptr) {
			ERR;
		}
	}

	//hit version
	//read-onlyならnot track or validate readSet
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
	//(committed or pending)の最新versionを探して、early abort check(EAC)
	//pendingはcommitされる可能性が高いので、EACに利用。
	while (version->status.load(std::memory_order_acquire) == VersionStatus::aborted) {
		version = version->next.load(std::memory_order_acquire);
		//if version is not found.
		//ここに来るのはありえない．deleteオペを実装していないから．
		if (version == nullptr) ERR;
	}
	//hit version

	//early abort check(EAC)
	//ここまで来てるということはversion->statusはcommit or pending
	if ((version->rts.load(memory_order_acquire) > this->wts->ts) && (version->status.load(std::memory_order_acquire) == VersionStatus::committed)) {
		//validation phaseにて必ずabortされてしまうので、early abort.
		this->status = TransactionStatus::abort;
		return;	//early abort
	}
	else if (version->wts.load(memory_order_acquire) > this->wts->ts) {
		//pendingならばこのトランザクションがabortされることは確定していないが、その可能性が高いためabort.
		//committedならば，Cicadaのバージョンリストはnewest to oldestと決まっているのでアボートが確定している．
		this->status = TransactionStatus::abort;
		return;	//early abort
	}

	if (NLR) {
		while (version->status.load(std::memory_order_acquire) != VersionStatus::committed) {
			while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {
				//spin-wait
				//グループコミットの時はタイムアウトを利用しないとデッドロックが起きる．
				if (GROUP_COMMIT) {
					uint64_t spinstart = rdtsc();
					uint64_t spinstop;
					while (version->status.load(std::memory_order_acquire) == VersionStatus::pending){
						spinstop = rdtsc();
						if (chkClkSpan(spinstart, spinstop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
							earlyAbort();
							return;
						}
					}
				}	
			}
			if (version->status.load(std::memory_order_acquire) == VersionStatus::committed) break;
		
			//status == aborted
			version = version->next.load(std::memory_order_acquire);
			if (version == nullptr) {
				//デリートオペレーションを実装していないので，ここに到達はありえない．
				//到達したらガーベッジコレクションで消してはいけないものを消している．
				ERR;
			}
		}
	} 
	else if (ELR) {
		//ここにきてる時点でステータスはpending or committed or precomitted.
		while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {}
		while (version->status.load(std::memory_order_acquire) != VersionStatus::precommitted && version->status.load(std::memory_order_acquire) != VersionStatus::committed) {
			//status == aborted
			version = version->next.load(std::memory_order_acquire);
			if (version == nullptr) {
				//デリートオペレーションを実装していないので，ここに到達はありえない．
				//到達したらガーベッジコレクションで消してはいけないものを消している．
				ERR;
			}
		}
	}

	Version *newObject;
   	newObject = new Version();
	newObject->rts.store(0, memory_order_release);
	newObject->wts.store(this->wts->ts, memory_order_release);
	newObject->key = version->key;
	newObject->val = val;
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
		do {
			expected = Table[(*itr).key].latest.load();
			Version *version = expected;

			while (version->status.load(std::memory_order_acquire) != VersionStatus::committed 
			&& version->status.load(std::memory_order_acquire) != VersionStatus::precommitted) {

				while (version->status.load(std::memory_order_acquire) == VersionStatus::aborted) {
					version = version->next.load(std::memory_order_acquire);
				}

				//to avoid cascading aborts.
				//install pending version step blocks concurrent transactions that share the same visible version and have higher timestamp than (tx.ts).
				if (NLR && GROUP_COMMIT) {
					uint64_t spinstart = rdtsc();
					uint64_t spinstop;
					while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {
						spinstop = rdtsc();
						if (chkClkSpan(spinstart, spinstop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
							return false;
						}
					}
				} else {
					while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {}
				}
				//now, version->status is not pending.
			}	

			//to keep versions ordered by wts in the version list
			//if version is pending version, concurrent transaction that has lower timestamp is aborted.
			if (this->wts->ts < version->wts.load(memory_order_acquire)) {
				return false;
			}

			(*itr).newObject->next.store(Table[(*itr).key].latest.load(), std::memory_order_release);
		} while (!Table[(*itr).key].latest.compare_exchange_weak(expected, (*itr).newObject));
	}

	//Read timestamp update
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t expected;
		do {
			expected = (*itr).ver->rts.load(memory_order_acquire);
			if (expected >= this->wts->ts) break;
		} while (!(*itr).ver->rts.compare_exchange_weak(expected, this->wts->ts));
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
		if ((*itr).sourceObject->rts.load(memory_order_acquire) > this->wts->ts) {
			return false;
		}
	}

	return true;
}

void
Transaction::swal()
{
//	if (!GROUP_COMMIT) {	//non-group commit
//		if (pthread_mutex_lock(&Lock)) ERR;	// lock
//
//		int i = 0;
//		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
//			SLogSet[i] = itr->second->newObject;
//			i++;
//		}
//
//		//ログをWALバッファへ挿入後，ローカルのコミットキューへトランザクションIDとwtsをエンキュー
//
//		double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
//		uint64_t start = rdtsc();
//		while ((rdtsc() - start) < threshold) {}	//spin-wait
//		ThreadFlushedWts[this->thid].ts = this->wts->ts;
//
//		if (pthread_mutex_unlock(&Lock)) ERR;	// unlock
//	}
//	else {	//group commit
//		if (pthread_mutex_lock(&Lock)) ERR;	// lock
//		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
//			SLogSet[GROUP_COMMIT_INDEX[0].num] = itr->second->newObject;
//			GROUP_COMMIT_INDEX[0].num++;
//		}
//
//		if (GROUP_COMMIT_COUNTER[0].num == 0) {
//			GCommitStart[0].num = rdtsc();	//最初の初期化もここで代用している
//		}
//
//		GROUP_COMMIT_COUNTER[0].num++;
//
//		if (GROUP_COMMIT_COUNTER[0].num == GROUP_COMMIT) {
//			//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
//			double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
//			uint64_t start = rdtsc();
//			while ((rdtsc() - start) < threshold) {}	//spin-wait
//			ThreadFlushedWts[this->thid].ts = this->wts->ts;
//
//			//group commit pending version.
//			gcpv();
//			if (pthread_mutex_unlock(&Lock)) ERR;	// unlock
//			return;
//		}
//		else {
//			if (pthread_mutex_unlock(&Lock)) ERR;	// unlock
//		}
//	}
}

void
Transaction::pwal()
{
//	Version **logSet;
//
//	if (!GROUP_COMMIT) {
//		logSet = new Version*[writeSet.size()];
//
//		int i = 0;
//		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
//			logSet[i] = itr->second->newObject;
//			i++;
//		}
//
//		//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
//		double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
//		uint64_t start = rdtsc();
//		while ((rdtsc() - start) < threshold) {}	//spin-wait
//		ThreadFlushedWts[this->thid].ts = this->wts->ts;
//
//		delete logSet;
//	} 
//	else {
//		for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
//			PLogSet[this->thid][GROUP_COMMIT_INDEX[this->thid].num] = itr->second->newObject;
//			GROUP_COMMIT_INDEX[this->thid].num++;
//		}
//
//		if (GROUP_COMMIT_COUNTER[this->thid].num == 0) {
//			GCommitStart[this->thid].num = rdtsc();	//最初の初期化もここで代用している
//			ThreadRtsArrayForGroup[this->thid].num = this->rts->ts;
//		}
//
//		GROUP_COMMIT_COUNTER[this->thid].num++;
//
//		if (GROUP_COMMIT_COUNTER[this->thid].num == GROUP_COMMIT) {
//			//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
//			double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
//			uint64_t start = rdtsc();
//			while ((rdtsc() - start) < threshold) {}	//spin-wait	
//			ThreadFlushedWts[this->thid].ts = this->wts->ts;
//
//			gcpv();
//		} 
//	}
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
//	if (S_WAL) {
//		for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].num; ++i) {
//			SLogSet[i]->status.store(VersionStatus::committed, std::memory_order_release);
//		}
//		GROUP_COMMIT_COUNTER[0].num = 0;
//		GROUP_COMMIT_INDEX[0].num = 0;
//	}
//	else if (P_WAL) {
//		for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[this->thid].num; ++i) {
//			PLogSet[this->thid][i]->status.store(VersionStatus::committed, std::memory_order_release);
//		}
//		GROUP_COMMIT_COUNTER[this->thid].num = 0;
//		GROUP_COMMIT_INDEX[this->thid].num = 0;
//	}
}

void 
Transaction::earlyAbort()
{
	writeSet.clear();
	readSet.clear();

	if (GROUP_COMMIT) {
		chkGcpvTimeout();
	}

	AbortCounts[thid].num++;

	this->wts->set_clockBoost(CLOCK_PER_US);
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

	AbortCounts[thid].num++;

	this->wts->set_clockBoost(CLOCK_PER_US);
	this->status = TransactionStatus::abort;
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
//	if (P_WAL) {
//		GCommitStop[this->thid].num = rdtsc();
//		if (chkClkSpan(GCommitStart[this->thid].num, GCommitStop[this->thid].num, GROUP_COMMIT_TIMEOUT_US * CLOCK_PER_US)) {
//			gcpv();
//			return true;
//		}
//	}
//	else if (S_WAL) {
//		GCommitStop[0].num = rdtsc();
//		if (chkClkSpan(GCommitStart[0].num, GCommitStop[0].num, GROUP_COMMIT_TIMEOUT_US * CLOCK_PER_US)) {
//			if (pthread_mutex_lock(&Lock)) ERR;
//			gcpv();
//			if (pthread_mutex_unlock(&Lock)) ERR;
//
//			return true;
//		}
//	}
//
//	return false;
}

void 
Transaction::mainte(int proIndex)
{
	//Maintenance
	//Schedule garbage collection
	//Declare quiescent state
	//Collect garbage created by prior transactions
	
	//バージョンリストにおいてMinRtsよりも古いバージョンの中でコミット済み最新のもの
	//以外のバージョンは全てデリート可能。絶対に到達されないことが保証される.
	//
	
	if (GCFlag[thid].num == 0) {
		this->GCstop = rdtsc();
		if (chkClkSpan(this->GCstart, this->GCstop, GC_INTER_US)) {
			GCFlag[thid].num = 1;
			this->GCstart = rdtsc();
		}
	}

	int gcctr = 0;
	for (auto itr = gcSet.begin(); itr != gcSet.end(); ++itr) {
		if ((*itr).wts >= MinRts.load(memory_order_acquire)) break;

		// (a) v.wts > record.min_wts
		if ((*itr).wts <= Table[(*itr).key].min_wts) {
			gcctr++;
			continue;
			break;
		}
		//this pointer may be dangling.

		// (b) acquiring the garbage collection lock succeeds
		int lockHolder;
		bool conteFlag = false;
		do {
			if (conteFlag) break;
			lockHolder = Table[(*itr).key].gClock.load(memory_order_acquire);
			if (lockHolder != -1) {
				conteFlag = true;
				break;
			}
		} while (!Table[(*itr).key].gClock.compare_exchange_weak(lockHolder, this->thid));
		if (conteFlag) {
			conteFlag = false;
			gcctr++;
			continue;
		}

		Version *delTarget = (*itr).ver->next.load(std::memory_order_acquire);
		(*itr).ver->next.store(nullptr, std::memory_order_release);
		while (delTarget != nullptr) {
			//nextポインタ退避
			Version *tmp = delTarget->next.load(std::memory_order_acquire);
			delete delTarget;
			delTarget = tmp;
		}
		
		Table[(*itr).key].gClock.store(-1, memory_order_release);
	}
	if (gcctr != 0)
		gcSet.erase(gcSet.begin(), gcSet.begin()+gcctr);
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
		gcSet.push_back(GCElement((*itr).key, (*itr).newObject, (*itr).newObject->wts));
	}

	//ここまで来たらコミットしている
	this->wts->set_clockBoost(0);
	readSet.clear();
	writeSet.clear();

	FinishTransactions[thid].num++;
}

