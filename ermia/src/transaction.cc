#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
#include "include/version.hpp"
#include <atomic>
#include <algorithm>
#include <bitset>

using namespace std;

void
Transaction::tbegin(const int &thid)
{
	this->thid = thid;
	this->txid = Lsn.load(std::memory_order_acquire);	
	ThtxID[thid].num.store(txid, memory_order_release);

	TMT[thid].cstamp.store(0, memory_order_release);
	TMT[thid].status.store(TransactionStatus::inFlight, memory_order_release);
}

int
Transaction::ssn_tread(int key)
{
	//safe retry property
	// it can be retried immediately, minimizing both wasted work and latency.
	// If the version has not yet been overwritten, it will be added to T's read set and checked for late-arriving overwrites during pre-commit.
	// (tanabe) overwrite というのは update による new version creation ?, それとも最大読み込みスタンプ(pstamp) ?
	// 			とりあえずここでは new version creation と仮定する．
	//if (safeRetry == true && readSet[key]->sstamp.load(memory_order_acquire) == (UINT64_MAX - 1)) {
	//	return readSet[key]->val;
	//}
	
	//if it already access the key object once.
	// w
	auto includeW = writeSet.find(key);
	if (includeW != writeSet.end()) {
		return includeW->second->val;
	}

	// if v not in t.writes:
	Version *ver = Table[key % TUPLE_NUM].latest.load(std::memory_order_acquire);
	if (ver->status.load(memory_order_acquire) != VersionStatus::committed) {
		ver = ver->committed_prev;
	}
	uint64_t verCstamp = ver->cstamp.load(memory_order_acquire);
	while (txid < (verCstamp >> 1)) {
		//printf("txid %d, (verCstamp >> 1) %d\n", txid, verCstamp >> 1);
		//fflush(stdout);
		ver = ver->committed_prev;
		if (ver == NULL) ERR;
		verCstamp = ver->cstamp.load(memory_order_acquire);
	}

	if (ver->sstamp.load(memory_order_acquire) == (UINT64_MAX - 1))
		// no overwrite yet
		readSet[key] = ver;	
	else 
		// update pi with r:w edge
		this->sstamp = min(this->sstamp, (ver->sstamp.load(memory_order_acquire) >> 1));

	uint64_t expected, desired;
	do {
		expected = ver->readers.load(memory_order_acquire);
		if (expected & (1<<thid)) break;	// ビットが立ってたら抜ける
		// reader ビットを立てる
		desired = expected | (1<<thid);
	} while (ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel));

	verify_exclusion_or_abort();

	return ver->val;
}

void
Transaction::ssn_twrite(int key, int val)
{
	// if it already wrote the key object once.
	auto includeW = writeSet.find(key);
	if (includeW != writeSet.end()) {
		includeW->second->val = val;
		return;
	}

	// if v not in t.writes:
	//
	//first-updater-wins rule
	//Forbid a transaction to update  a record that has a committed head version later than its begin timestamp.
	
	Version *expected, *desired;
	desired = new Version();
	uint64_t tmptid = this->thid;
	tmptid = tmptid << 1;
	tmptid |= 1;
	desired->cstamp.store(tmptid, memory_order_release);	// read operation, write operation, ガベコレからアクセスされるので， CAS 前に格納

	Version *vertmp;
	do {
		expected = Table[key % TUPLE_NUM].latest.load(std::memory_order_acquire);
		//if (expected == nullptr) {
		//	ERR;
		//}

		// w-w conflict
		// first updater wins rule
		if (expected->status.load(memory_order_acquire) == VersionStatus::inFlight) {
			this->status = TransactionStatus::aborted;
			TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
			delete desired;
			return;
		}
		
		// 先頭バージョンが committed バージョンでなかった時
		vertmp = expected;
		while (vertmp->status.load(memory_order_acquire) != VersionStatus::committed) {
			vertmp = vertmp->committed_prev;
			if (vertmp == nullptr) ERR;
		}

		// vertmp は commit済み最新バージョン
		uint64_t verCstamp = vertmp->cstamp.load(memory_order_acquire);
		if (txid < (verCstamp >> 1)) {	
			//	write - write conflict, first-updater-wins rule.
			// Writers must abort if they would overwirte a version created after their snapshot.
			this->status = TransactionStatus::aborted;
			TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
			delete desired;
			return;
		}

		desired->prev = expected;
		desired->committed_prev = vertmp;

	} while (!Table[key % TUPLE_NUM].latest.compare_exchange_weak(expected, desired, memory_order_acq_rel));
	desired->val = val;

	//ver->committed_prev->sstamp に TID を入れる
	uint64_t tmpTID = thid;
	tmpTID = tmpTID << 1;
	tmpTID |= 1;
	desired->committed_prev->sstamp.store(tmpTID, memory_order_release);

	// update eta with w:r edge
	this->pstamp = max(this->pstamp, desired->committed_prev->pstamp.load(memory_order_acquire));
	writeSet[key] = desired;
	readSet.erase(key);	//	avoid false positive

	verify_exclusion_or_abort();
}

void
Transaction::ssn_commit()
{
	SsnLock.lock();

	cstamp = Lsn.fetch_add(1);	//	begin pre-commit
	cstamp++;

	// finalize eta(T)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		pstamp = max(pstamp, itr->second->committed_prev->pstamp.load(memory_order_acquire));
	}	

	// finalize pi(T)
	sstamp = min(sstamp, cstamp);
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t verSstamp = itr->second->sstamp.load(memory_order_acquire);
		//最下位ビットが上がっていたら，並行トランザクションに上書きされている．
		//しかし，シリアルSSNにおいては，このトランザクションよりも必ず後に
		//validation (maybe commit) されるので，絶対にスキップできる．
		//その上，最下位ビットが上がっていたら，それはスタンプではなく TIDである．
		if (verSstamp & 1) continue;
		sstamp = min(sstamp, verSstamp >> 1);
	}

	// ssn_check_exclusion
	if (pstamp < sstamp) status = TransactionStatus::committed;
	else {
		status = TransactionStatus::aborted;
		SsnLock.unlock();
		return;
	}

	// update eta
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		itr->second->pstamp = max(itr->second->pstamp.load(memory_order_acquire), cstamp);
		// readers ビットを下げる
		uint64_t expected, desired;
		do {
			expected = itr->second->readers.load(memory_order_acquire);
			if (!(expected & (1<<thid))) break;
			desired = expected;
			desired &= ~(1<<thid);
		} while (itr->second->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel));

	}

	// update pi
	uint64_t verSstamp = this->sstamp;
	verSstamp = verSstamp << 1;
	verSstamp &= ~(1);

	uint64_t verCstamp = cstamp;
	verCstamp = verCstamp << 1;
	verCstamp &= ~(1);

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		itr->second->committed_prev->sstamp.store(verSstamp, memory_order_release);	
		// initialize new version
		itr->second->pstamp = cstamp;
		itr->second->cstamp = verCstamp;
	}

	//logging
	//?*
	
	// status, inFlight -> committed
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) 
		itr->second->status.store(VersionStatus::committed, memory_order_release);

	this->status = TransactionStatus::committed;
	safeRetry = false;

	SsnLock.unlock();

	FinishTransactions[thid].num++;
	return;
}

void
Transaction::ssn_parallel_commit()
{
	this->status = TransactionStatus::committing;
	TMT[thid].status.store(TransactionStatus::committing);

	uint64_t ctmp = Lsn.fetch_add(1);
	ctmp++;
	this->cstamp = ctmp;
	TMT[thid].cstamp.store(ctmp, memory_order_release);
	
	// begin pre-commit
	
	// finalize pi(T)
	this->sstamp = min(this->sstamp, this->cstamp);
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t v_sstamp = itr->second->sstamp;
		//最下位ビットが上がっていたら TID
		if (v_sstamp & 1) {
			//TID から worker を特定する
			uint8_t worker = (v_sstamp >> 1);
			// worker == thid はありえない
			if (worker == thid) ERR;
			// もし worker が inFlight (read phase 実行中) だったら
			// このトランザクションよりも新しい commit timestamp でコミットされる．
			// 従って pi となりえないので，スキップ．
			// そうでないなら，チェック
			if (TMT[worker].status.load(memory_order_acquire) == TransactionStatus::committing) {
				// worker は ssn_parallel_commit() に突入している
				// cstamp が確実に取得されるまで待機 (cstamp は初期値 0)
				ctmp = TMT[worker].cstamp.load(memory_order_acquire);
				while (ctmp == 0) {
					ctmp = TMT[worker].cstamp.load(memory_order_acquire);
					// 以下の処理はデバッグとして有用だった．
					// ワーカーの終了時間を迎え，最後のトランザクション処理となったとする．
					// 並行ワーカーは(アボート or コミットで)リトライし，
					// cstamp = 0 と初期化され, read phase でアボートして終了条件を満たしたとしたら，この while は無限ループになってしまう．
					if (TMT[worker].status.load(memory_order_acquire) == TransactionStatus::aborted) break;
				}
				if (TMT[worker].status.load(memory_order_acquire) == TransactionStatus::aborted) continue;

				// worker->cstamp が this->cstamp より小さいなら pi になる可能性あり
				if (ctmp < this->cstamp) {
					TransactionStatus statusTmp = TMT[worker].status.load(memory_order_acquire);
					// parallel_commit 終了待ち（sstamp確定待ち)
					TMT_waitFor[thid].num.store(worker, memory_order_release);
					while (statusTmp == TransactionStatus::committing) {
						// 別のワーカーと，parallel_commit 終了待ちでコンフリクトしてデッドロックすることがあるので，対策をする
						// 対策 - immediate restart
						// 相手が自分を待っていたら，自分をアボートする
						
						uint8_t tmp = TMT_waitFor[worker].num.load(memory_order_acquire);
						while (tmp != 0) {
							if (tmp == thid) {
								TMT_waitFor[thid].num.store(0, memory_order_release);
								status = TransactionStatus::aborted;
								TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
								return;
							}
							tmp = TMT_waitFor[tmp].num.load(memory_order_acquire);
						}
						statusTmp = TMT[worker].status.load(memory_order_acquire);
					}
					TMT_waitFor[thid].num.store(0, memory_order_release);
					if (statusTmp == TransactionStatus::committed) {
						this->sstamp = min(this->sstamp, TMT[worker].sstamp.load(memory_order_acquire));
					}
				}
			}
		} else {
			this->sstamp = min(this->sstamp, v_sstamp >> 1);
		}
	}

	// finalize eta
	uint64_t one = 1;
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {

		//for r in v.prev.readers
		uint64_t rdrs = itr->second->readers.load(memory_order_acquire);
		for (int worker = 1; worker <= THREAD_NUM; ++worker) {
			if ((rdrs & (one << worker)) ? 1 : 0) {
				TransactionStatus statusTmp = TMT[worker].status.load(memory_order_acquire);
				// 並行 reader が committing なら無視できない．
				// inFlight なら無視できる．なぜならそれが commit する時にはこのトランザクションよりも
				// 大きい cstamp を有し， eta となりえないから．
				while (statusTmp == TransactionStatus::committing) {
					ctmp = TMT[worker].cstamp.load(memory_order_acquire);
					while (ctmp == 0) {
						ctmp = TMT[worker].cstamp.load(memory_order_acquire);
						// 以下の処理はデバッグとして有用だった．
						// ワーカーの終了時間を迎え，最後のトランザクション処理となったとする．
						// 並行トランザクションはアボートしてリトライし，
						// cstamp = 0 と初期化され, read phase でアボートして終了条件を満たしたとしたら，この while は無限ループになってしまう．
						if (TMT[worker].status.load(memory_order_acquire) == TransactionStatus::aborted) continue;
					}
					if (TMT[worker].status.load(memory_order_acquire) == TransactionStatus::aborted) continue;
					
					// worker->cstamp が this->cstamp より小さいなら eta になる可能性あり
					if (ctmp < this->cstamp) {
						TransactionStatus statusTmp = TMT[worker].status.load(memory_order_acquire);
						// parallel_commit 終了待ち (sstamp 確定待ち)
						TMT_waitFor[thid].num.store(worker, memory_order_release);
						while (statusTmp == TransactionStatus::committing) {
							// 別のワーカーと，parallel_commit 終了待ちでコンフリクトしてデッドロックすることがあるので，対策をする
							// 対策 - immediate restart

							uint8_t tmp = TMT_waitFor[worker].num.load(memory_order_acquire);
							while (tmp != 0) {
									if (tmp == thid) {
										TMT_waitFor[thid].num.store(0, memory_order_release);
										status = TransactionStatus::aborted;
										TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
										return;
									}

									tmp = TMT_waitFor[tmp].num.load(memory_order_acquire);
							}
							statusTmp = TMT[worker].status.load(memory_order_acquire);
						}
						TMT_waitFor[thid].num.store(0, memory_order_release);
						if (statusTmp == TransactionStatus::committed) {
							this->pstamp = min(this->pstamp, TMT[worker].cstamp.load(memory_order_acquire));
						}
					}
				}
			} 
		}
		// re-read pstamp in case we missed any reader
		this->pstamp = min(this->pstamp, itr->second->committed_prev->pstamp.load(memory_order_acquire));
	}

	// ssn_check_exclusion
	if (pstamp < sstamp) {
		status = TransactionStatus::committed;
		TMT[thid].sstamp.store(this->sstamp, memory_order_release);
		TMT[thid].status.store(TransactionStatus::committed, memory_order_release);
	} else {
		status = TransactionStatus::aborted;
		TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
		return;
	}

	// update eta
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t ptmp = itr->second->pstamp.load(memory_order_acquire);
		while (ptmp < this->cstamp) {
			if (itr->second->pstamp.compare_exchange_weak(ptmp, this->cstamp, memory_order_acq_rel)) break;
			else ptmp = itr->second->pstamp.load(memory_order_acquire);
		}
	}


	// update pi
	uint64_t verSstamp = this->sstamp;
	verSstamp = verSstamp << 1;
	verSstamp &= ~(1);

	uint64_t verCstamp = cstamp;
	verCstamp = verCstamp << 1;
	verCstamp &= ~(1);

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		itr->second->committed_prev->sstamp.store(verSstamp, memory_order_release);
		itr->second->cstamp.store(verCstamp, memory_order_release);
		itr->second->pstamp.store(this->cstamp, memory_order_release);
	}

	//logging
	//?*
	
	// status, inFlight -> committed
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) 
		itr->second->status.store(VersionStatus::committed, memory_order_release);

	safeRetry = false;
	FinishTransactions[thid].num++;
	return;
}

void
Transaction::abort()
{
	AbortCounts[thid].num++;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		itr->second->committed_prev->sstamp.store(UINT64_MAX & ~(1), memory_order_release);
		itr->second->status.store(VersionStatus::aborted, memory_order_release);
	}
	writeSet.clear();

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t expected, desired;
		do {
			expected = itr->second->readers.load(memory_order_acquire);
			if (!(expected & (1<<thid))) break;
			desired = expected & ~(1<<thid);
		} while (itr->second->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel));
	}
	readSet.clear();

	safeRetry = true;
}

void
Transaction::rwsetClear()
{
	//printf("thread #%d: readSet size -> %d\n", thid, readSet.size());
	//printf("thread #%d: writeSet size -> %d\n", thid, writeSet.size());

	readSet.clear();
	writeSet.clear();

	//printf("thread #%d: readSet size -> %d\n", thid, readSet.size());
	//printf("thread #%d: writeSet size -> %d\n", thid, writeSet.size());

	return;
}

void
Transaction::verify_exclusion_or_abort()
{
	if (this->pstamp >= this->sstamp) {
		this->status = TransactionStatus::aborted;
		TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
	}
}
