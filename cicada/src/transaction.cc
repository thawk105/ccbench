#include "include/transaction.hpp"
#include "include/common.hpp"
#include "include/timeStamp.hpp"
#include "include/debug.hpp"
#include "include/version.hpp"
#include "include/tsc.hpp"
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

void Transaction::tbegin(TimeStamp &thrts, TimeStamp &thwts, bool &firstFlag, const int &thid, const unsigned int transactionNum)
{
	Version *verTmp;

	this->thid = thid;
	this->transactionNum = transactionNum;

	if (Pro[transactionNum][0].ronly) this->ronly = true;

	//first allocate timestamp
	if (firstFlag == true) {
		firstFlag = false;
		thwts.generateTimeStampFirst(thid);
		verTmp = HashTable[0].latest.load();
		thrts.ts = verTmp->wts + 1;
		this->wts = &thwts;
		this->rts = &thrts;
		ThreadRtsArrayForGroup[this->thid] = this->rts->ts;
		
		Start[thid].num = rdtsc();
		FirstAllocateTimeStamp[thid].store(true, std::memory_order_release);

		//FirstAllocateTimeStamp の更新 -> FirstAllocateTimeStamp のチェック

		//最後にタイムスタンプを割り当てられたスレッドを検知
		//そのスレッドのwtsが最小
		int i;
		for (i = 0; i < THREAD_NUM; i++) {
			if (i == thid) continue;
			if (FirstAllocateTimeStamp[i].load(std::memory_order_acquire) == false) break;
		}
		if (i == THREAD_NUM) {
			MinWts.store(thwts.ts, std::memory_order_release);
		}
	}
	else {
		//allocate timestamp after first allocate timestamp
		thwts.generateTimeStamp(thid);
		thrts.ts = MinWts.load(std::memory_order_acquire) - 1;
		this->wts = &thwts;
		this->rts = &thrts;

		//leader thread work
		if (thid == 0) {
			Stop[thid].num = rdtsc();

			//chkClkSpan(start, stop, threshold(us))
			if (chkClkSpan(Start[thid].num, Stop[thid].num, 10 * CLOCK_PER_US)) {
				//10 * clock_per_us = 10 us
				//
				//一定時間ごとに仕事する
				//
				//work can be done by leader thread
				//compute min_wts, min_rts,
				unsigned int maxwIndex = 0;
				//リーダースレッドの余計な仕事をしているうちにtsが遅れて、
				//これから処理するトランザクションのabort確率が上がってしまうことが考えられる。
				//最速のものも記録し、それに合わせる(tanabe original)
				//(one-sided synchronization)
				uint64_t minw = ThreadWtsArray[0]->ts;
				uint64_t minr;
			   	if (GROUP_COMMIT == 0) {
			   		minr = ThreadRtsArray[0]->ts;
				} else {
					minr = ThreadRtsArrayForGroup[0];
				}
				for (int i = 0; i < THREAD_NUM; i++) {
					//MinRtsの計算も同様
					//
					//maxwInexはなるべく早めにしたい意図なので本当のmaxでなくても構わない．
					uint64_t tmp = ThreadWtsArray[i]->ts;
					if (minw > tmp) minw = tmp;

					if (GROUP_COMMIT == 0) {
						tmp = ThreadRtsArray[i]->ts;
						if (minr > tmp) minr = tmp;
					} else {
						tmp = ThreadRtsArrayForGroup[i];
						if (minr > tmp) minr = tmp;
					}

					if (ThreadWtsArray[maxwIndex]->ts < ThreadWtsArray[i]->ts) maxwIndex = i;
				}
				MinWts.store(minw, std::memory_order_release);;
				MinRts.store(minr, std::memory_order_release);
				thwts = *ThreadWtsArray[maxwIndex];

				//stopwatchの起点修正
				Start[thid].num = rdtsc();
			}
		}
		else {
			//one-sided synchronization
			Stop[thid].num = rdtsc();
			if (chkClkSpan(Start[thid].num, Stop[thid].num, 100 * CLOCK_PER_US)) {
				unsigned int maxwIndex = 0;
				//最速のものを記録し、それに合わせる
				//one-sided synchronization
				for (int i = 0; i < THREAD_NUM; i++) {
					if (ThreadWtsArray[maxwIndex]->ts < ThreadWtsArray[i]->ts) maxwIndex = i;
				}
				thwts = *ThreadWtsArray[maxwIndex];

				//stopwatchの起点修正
				Start[thid].num = rdtsc();
			}
		}
	}
}

int
Transaction::tread(int key)
{
	//read-own-writes
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	//if n E write set
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		if (itr->first == key) {
			return itr->second->newObject->val;
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
	Version *version = HashTable[key % TUPLE_NUM].latest;
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
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	//read set := read set U {n};
	if (readSet.empty()) {
		//head is empty.
		readSet[key] = version;
	}
	else {
		//重複検査
		auto includeR = readSet.find(key);
		if (includeR == readSet.end()) readSet[key] = version;
	}

	return version->val;
}

void
Transaction::twrite(int key,  int val)
{
	Version *version = HashTable[key % TUPLE_NUM].latest.load();

	//Search version (for early abort check)
	//(committed or pending)の最新versionを探して、early abort check(EAC)
	//pendingはcommitされる可能性が高いので、EACに利用。
	while (version->status.load(std::memory_order_acquire) == VersionStatus::aborted) {
		version = version->next.load(std::memory_order_acquire);
		//if version is not found.
		if (version == nullptr) {
			//ここに来るのはありえない．deleteオペを実装していないから．
			ERR;
			goto SKIP_EAC;
		}
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

SKIP_EAC:

	if (writeSet.empty()) {
		if (NLR) {
			while (version->status.load(std::memory_order_acquire) != VersionStatus::committed) {
				while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {
					//spin-wait
					//グループコミットの時はタイムアウトを利用しないとデッドロックが起きる．
					if (GROUP_COMMIT) {
						this->start = rdtsc();
						while (version->status.load(std::memory_order_acquire) == VersionStatus::pending){
							if (version->status.load(std::memory_order_acquire) != VersionStatus::pending) break;
							this->stop = rdtsc();
							if (chkClkSpan(this->start, this->stop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
								earlyAbort();
								return;
							}
						}
					}	
					if (version->status.load(std::memory_order_acquire) != VersionStatus::pending) break;
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

		ElementSet *wElement;
	   	wElement = new ElementSet;
		wElement->newObject = new Version();

		wElement->sourceObject = version;
		wElement->newObject->rts.store(0, memory_order_release);
		wElement->newObject->wts.store(this->wts->ts, memory_order_release);
		wElement->newObject->key = version->key;
		wElement->newObject->val = val;
		writeSet[key] = wElement;
		return;
	} else {
		//this->wset != nullptr
		//if n E writeSet
		auto includeW = writeSet.find(key);
		if (includeW != writeSet.end()) {
			includeW->second->newObject->val = val;
			return;
		}
		
		//else
		version = HashTable[key % TUPLE_NUM].latest.load();
		if (NLR) {
			while (version->status.load(std::memory_order_acquire) != VersionStatus::committed) {
				while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {
					//spin-wait
					//グループコミットはタイムアウトを利用しないとデッドロック
					if (GROUP_COMMIT) {
						this->start = rdtsc();
						while (version->status.load(std::memory_order_acquire) == VersionStatus::pending){
							if (version->status.load(std::memory_order_acquire) != VersionStatus::pending) break;
							this->stop = rdtsc();
							if (chkClkSpan(this->start, this->stop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
								earlyAbort();
								return;
							}
						}	
					}
					if (version->status.load(std::memory_order_acquire) == VersionStatus::committed) break;
				}
				
				//status == aborted
				version = version->next.load(std::memory_order_acquire);
				if (version == nullptr) ERR;
			}
		} else if (ELR) {
			while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {}
			while (version->status.load(std::memory_order_acquire) != VersionStatus::committed && version->status.load(std::memory_order_acquire) != VersionStatus::precommitted) {
				
				//status == aborted
				version = version->next.load(std::memory_order_acquire);
				if (version == nullptr) ERR;
			}

		}

		ElementSet *wElement;
		wElement = new ElementSet;
		wElement->newObject = new Version();

		wElement->sourceObject = version;
		wElement->newObject->rts.store(0, memory_order_release);
		wElement->newObject->wts = this->wts->ts;
		wElement->newObject->key = version->key;
		wElement->newObject->val = val;
		writeSet[key] = wElement;
		return;
	}
}

bool
Transaction::validation() 
{
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	//Install pending version
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		
		Version *expected;
		do {
			expected = HashTable[itr->second->sourceObject->key % TUPLE_NUM].latest.load();
			Version *version = expected;

			while (version->status.load(std::memory_order_acquire) != VersionStatus::committed && version->status.load(std::memory_order_acquire) != VersionStatus::precommitted) {

				while (version->status.load(std::memory_order_acquire) == VersionStatus::aborted) {
					version = version->next.load(std::memory_order_acquire);
				}
				//if version is pending version, this checking is what check concurrent transactions.

				//to keep versions ordered by wts in the version list
				//if version is pending version, concurrent transaction that has lower timestamp is aborted.
				if (this->wts->ts <= version->wts.load(memory_order_acquire)) {
					//if (this->wts->ts == version->wts.ts) printf("?\n");
					return false;
				}

				//to avoid cascading aborts.
				//install pending version step blocks concurrent transactions that share the same visible version and have higher timestamp than (tx.ts).
				if (NLR && GROUP_COMMIT) {
					this->start = rdtsc();
					while (version->status.load(std::memory_order_acquire) == VersionStatus::pending) {
						this->stop = rdtsc();
						if (chkClkSpan(this->start, this->stop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
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
			if (this->wts->ts <= version->wts.load(memory_order_acquire)) {
				return false;
			}

			itr->second->newObject->next.store(HashTable[itr->second->sourceObject->key % TUPLE_NUM].latest.load(), std::memory_order_release);
		} while (!HashTable[itr->second->sourceObject->key % TUPLE_NUM].latest.compare_exchange_weak(expected, itr->second->newObject));
	}
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	//Read timestamp update
	//
	//printf("read timestamp update\n");
	for (auto itr = readSet.begin(); itr != readSet.end(); itr++) {
		uint64_t expected;
		do {
			expected = itr->second->rts.load(memory_order_acquire);
			if (expected >= this->wts->ts) break;
		} while (!itr->second->rts.compare_exchange_weak(expected, this->wts->ts));
	}
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	//validation consistency check
	//(a) every previously visible version v of the records in the read set is the currently visible version to the transaction.
	for (auto itr = readSet.begin(); itr != readSet.end(); itr++) {
		if (ELR) {
			if (itr->second->status.load(std::memory_order_acquire) != VersionStatus::committed && itr->second->status.load(std::memory_order_acquire) != VersionStatus::precommitted) {
				return false;
			}
		} else {
			if (itr->second->status.load(std::memory_order_acquire) != VersionStatus::committed) {
				return false;
			}
		}
	}

	//(b) every currently visible version v of the records in the write set satisfies (v.rts) <= (tx.ts)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		if (itr->second->sourceObject->rts.load(memory_order_acquire) > this->wts->ts) {
			return false;
		}
	}
	//ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	return true;
}

void
Transaction::swal()
{
	if (!GROUP_COMMIT) {	//non-group commit
		if (pthread_mutex_lock(&Lock)) ERR;	// lock

		int i = 0;
		for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
			SLogSet[i] = itr->second->newObject;
			i++;
		}

		//ログをWALバッファへ挿入後，ローカルのコミットキューへトランザクションIDとwtsをエンキュー
		Message message(this->transactionNum, *this->wts);
		MessageQueue[this->thid].push(message);

		double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
		uint64_t start = rdtsc();
		while ((rdtsc() - start) < threshold) {}	//spin-wait
		ThreadFlushedWts[this->thid].ts = this->wts->ts;

		if (pthread_mutex_unlock(&Lock)) ERR;	// unlock
	}
	else {	//group commit
		if (pthread_mutex_lock(&Lock)) ERR;	// lock
		for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
			SLogSet[GROUP_COMMIT_INDEX[0].num] = itr->second->newObject;
			GROUP_COMMIT_INDEX[0].num++;
		}

		if (GROUP_COMMIT_COUNTER[0].num == 0) {
			GCommitStart[0].num = rdtsc();	//最初の初期化もここで代用している
		}

		//ログをWALバッファへ挿入後，ローカルのコミットキューへトランザクションIDとwtsをエンキュー
		Message message(this->transactionNum, *this->wts);
		MessageQueue[this->thid].push(message);

		GROUP_COMMIT_COUNTER[0].num++;

		if (GROUP_COMMIT_COUNTER[0].num == GROUP_COMMIT) {
			//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
			double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
			uint64_t start = rdtsc();
			while ((rdtsc() - start) < threshold) {}	//spin-wait
			ThreadFlushedWts[this->thid].ts = this->wts->ts;

			//group commit pending version.
			gcpv();
			if (pthread_mutex_unlock(&Lock)) ERR;	// unlock
			return;
		}
		else {
			if (pthread_mutex_unlock(&Lock)) ERR;	// unlock
		}
	}
}

void
Transaction::pwal()
{
	Version **logSet;

	if (!GROUP_COMMIT) {
		logSet = new Version*[writeSet.size()];

		int i = 0;
		for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
			logSet[i] = itr->second->newObject;
			i++;
		}

		//ログをWALバッファへ挿入後，ローカルのコミットキューへトランザクションIDとwtsをエンキュー
		Message message(this->transactionNum, *this->wts);
		MessageQueue[this->thid].push(message);

		//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
		double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
		uint64_t start = rdtsc();
		while ((rdtsc() - start) < threshold) {}	//spin-wait
		ThreadFlushedWts[this->thid].ts = this->wts->ts;

		delete logSet;
	} 
	else {
		for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
			PLogSet[this->thid][GROUP_COMMIT_INDEX[this->thid].num] = itr->second->newObject;
			GROUP_COMMIT_INDEX[this->thid].num++;
		}

		if (GROUP_COMMIT_COUNTER[this->thid].num == 0) {
			GCommitStart[this->thid].num = rdtsc();	//最初の初期化もここで代用している
			ThreadRtsArrayForGroup[this->thid] = this->rts->ts;
		}

		//ログをWALバッファへ挿入後，ローカルのコミットキューへトランザクションIDとwtsをエンキュー
		Message message(this->transactionNum, *this->wts);
		MessageQueue[this->thid].push(message);

		GROUP_COMMIT_COUNTER[this->thid].num++;

		if (GROUP_COMMIT_COUNTER[this->thid].num == GROUP_COMMIT) {
			//以下、将来のIO速度を考慮し、flushの代用として遅延を加える。
			double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
			uint64_t start = rdtsc();
			while ((rdtsc() - start) < threshold) {}	//spin-wait	
			ThreadFlushedWts[this->thid].ts = this->wts->ts;

			gcpv();
		} 
	}
}

inline void
Transaction::cpv()	//commit pending versions
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		itr->second->newObject->status.store(VersionStatus::committed, std::memory_order_release);
	}

}

void
Transaction::precpv() 
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		itr->second->newObject->status.store(VersionStatus::precommitted, std::memory_order_release);
	}

}
	
void 
Transaction::gcpv()
{
	if (S_WAL) {
		for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].num; i++) {
			SLogSet[i]->status.store(VersionStatus::committed, std::memory_order_release);
		}
		GROUP_COMMIT_COUNTER[0].num = 0;
		GROUP_COMMIT_INDEX[0].num = 0;
	}
	else if (P_WAL) {
		for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[this->thid].num; i++) {
			PLogSet[this->thid][i]->status.store(VersionStatus::committed, std::memory_order_release);
		}
		GROUP_COMMIT_COUNTER[this->thid].num = 0;
		GROUP_COMMIT_INDEX[this->thid].num = 0;
	}
}

void 
Transaction::earlyAbort()
{
	wSetClean();

	if (GROUP_COMMIT) {
		chkGcpvTimeout();
	}

	this->wts->set_clockBoost(CLOCK_PER_US);
	this->status = TransactionStatus::abort;
}

void
Transaction::abort()
{
	//pending versionのステータスをabortedに変更
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		itr->second->newObject->status.store(VersionStatus::aborted, std::memory_order_release);
	}

	wSetClean();

	if (GROUP_COMMIT) {
		chkGcpvTimeout();
	}

	this->wts->set_clockBoost(CLOCK_PER_US);
	this->status = TransactionStatus::abort;
}

void
Transaction::wSetClean()
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		delete itr->second;
	}
}

void
Transaction::displayWset()
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		printf("%d ", itr->second->newObject->key);
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
			if (pthread_mutex_lock(&Lock)) ERR;
			gcpv();
			if (pthread_mutex_unlock(&Lock)) ERR;

			return true;
		}
	}

	return false;
}

void 
Transaction::mainte(int proIndex)
{
	//Maintenance
	//Schedule garbage collection
	//Declare quiescent state
	//Collect garbage created by prior transactions
	
	if (GARBAGE_COLLECTION_INTERVAL_US > 0) {
		GCollectionStop[this->thid].num = rdtsc();
		if (!chkClkSpan(GCollectionStart[this->thid].num, GCollectionStop[this->thid].num, GARBAGE_COLLECTION_INTERVAL_US * CLOCK_PER_US)) return;
		GCollectionStart[this->thid].num = rdtsc();
	}

	//バージョンリストにおいてMinRtsよりも古いバージョンの中でコミット済み最新のもの
	//以外のバージョンは全てデリート可能。絶対に到達されないことが保証される.
	//safe garbage collection
	//
	//write setで取り扱ったバージョンリストを対象にする
	Version *version = nullptr;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		int lockHolder;
		bool conteFlag = false;
		do {
			if (conteFlag) break;
			lockHolder = HashTable[itr->first % TUPLE_NUM].gClock.load(memory_order_acquire);
			if (lockHolder != -1) {
				conteFlag = true;
				break;
			}
		} while (!HashTable[itr->first % TUPLE_NUM].gClock.compare_exchange_weak(lockHolder, this->thid));
		if (conteFlag) {
			conteFlag = false;
			continue;
		}


		version = HashTable[itr->second->newObject->key % TUPLE_NUM].latest.load(memory_order_acquire);
		//デリートオペレーション等でバージョンリストが存在しない場合
		if (version == nullptr) {
			// delete ope を実装していないので、到達不能
			ERR;
			// delete ope を実装していた場合、獲得していたロックを解放して次の要素を扱う。
			/*
			HashTable[itr->first % TUPLE_NUM].gClock.store(-1, memory_order_release);
			continue;
			*/
		}

		//バージョンリストが存在する場合

		while (version->wts.load(memory_order_acquire) >= MinRts.load(std::memory_order_acquire)) {
			version = version->next.load(std::memory_order_acquire);
			if (version == nullptr) break;
		}
		if (version == nullptr) {
			//削除可能バージョン無し
			HashTable[itr->first % TUPLE_NUM].gClock.store(-1, memory_order_release);
			continue;
		}
		//ここに到達したら、versionは
		//バージョンリストにおいてMinRtsよりも古いバージョンの中で最新のもの
		//
		//次にその中で最新のコミット済みを検索。それ以降が削除可能
		while (version->status.load(std::memory_order_acquire) != VersionStatus::committed) {
			version = version->next.load(std::memory_order_acquire);
			if (version == nullptr) break;
		}
		if (version == nullptr) {
			//削除可能バージョン無し
			HashTable[itr->first % TUPLE_NUM].gClock.store(-1, memory_order_release);
			continue;
		}
		//ここに到達したら、versionは
		//バージョンリストにおいてMinRtsよりも古いコミット済みバージョンの中で最新のもの
		//このversion->nextから削除可能。

		Version *delTarget = version->next.load(std::memory_order_acquire);
		version->next.store(nullptr, std::memory_order_release);
		while (delTarget != nullptr) {
			//nextポインタ退避
			Version *tmp = delTarget->next.load(std::memory_order_acquire);
			delTarget->next.store(nullptr, std::memory_order_release);
			delete delTarget;
			delTarget = tmp;
		}
		
		HashTable[itr->first % TUPLE_NUM].gClock.store(-1, memory_order_release);
	}

	wSetClean();
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

	//ここまで来たらコミットしている
	this->wts->set_clockBoost(0);
}

void
Transaction::noticeToClients()
{
	//一般的には，MessageのcommitWtsと全ワーカースレッドのWALバッファにおける最小のFlushedWtsを比較して，
	//FlushedWtsより古いcommitWtsに関してクライアントへ通知する
	//
	//Cicadaでは利用したpre-commited Versionがcommittedになっているか確認し，
	//なっていれば通知で良い？（検討）
	//
	while(!MessageQueue[this->thid].empty()){
		MessageQueue[this->thid].pop();
	}
}
