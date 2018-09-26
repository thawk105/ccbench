#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
#include "include/version.hpp"
#include <atomic>
#include <algorithm>
#include <bitset>

using namespace std;

SetElement *
Transaction::searchReadSet(unsigned int key) 
{
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

SetElement *
Transaction::searchWriteSet(unsigned int key) 
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

void
Transaction::tbegin()
{
	this->txid = TMT[1].prev_cstamp.load(memory_order_acquire);
	for (unsigned int i = 2; i < THREAD_NUM; ++i) {
		this->txid = max(this->txid, TMT[i].prev_cstamp.load(memory_order_acquire));
	}
	ThtxID[thid].num.store(txid, memory_order_release);
	TMT[thid].cstamp.store(0, memory_order_release);
	status = TransactionStatus::inFlight;
	TMT[thid].status.store(TransactionStatus::inFlight, memory_order_release);
	pstamp = 0;
	sstamp = UINT64_MAX;
}

int
Transaction::ssn_tread(unsigned int key)
{
	//safe retry property
	
	//if it already access the key object once.
	// w
	SetElement *inW = searchWriteSet(key);
	if (inW) {
		return inW->ver->val;
	}

	SetElement *inR = searchReadSet(key);
	if (inR) {
		return inR->ver->val;
	}

	// if v not in t.writes:
	Version *ver = Table[key].latest.load(std::memory_order_acquire);
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
		readSet.push_back(SetElement(key, ver));
	else 
		// update pi with r:w edge
		this->sstamp = min(this->sstamp, (ver->sstamp.load(memory_order_acquire) >> 1));

	uint64_t expected, desired;
	for (;;) {
		expected = ver->readers.load(memory_order_acquire);
RETRY_TREAD:
		if (expected & (1<<thid)) break;	// ビットが立ってたら抜ける
		// reader ビットを立てる
		desired = expected | (1<<thid);
		if (ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_TREAD;
	}

	verify_exclusion_or_abort();

	return ver->val;
}

void
Transaction::ssn_twrite(unsigned int key, unsigned int val)
{
	// if it already wrote the key object once.
	SetElement *inW = searchWriteSet(key);
	if (inW) {
		inW->ver->val = val;
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
	for (;;) {
		expected = Table[key].latest.load(std::memory_order_acquire);
RETRY_TWRITE:

		// w-w conflict
		// first updater wins rule
		if (expected->status.load(memory_order_acquire) == VersionStatus::inFlight) {
			this->status = TransactionStatus::aborted;
			TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
			delete desired;
			NNN;
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
		if (Table[key].latest.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_TWRITE;
	}
	desired->val = val;

	//ver->committed_prev->sstamp に TID を入れる
	uint64_t tmpTID = thid;
	tmpTID = tmpTID << 1;
	tmpTID |= 1;
	desired->committed_prev->sstamp.store(tmpTID, memory_order_release);

	// update eta with w:r edge
	this->pstamp = max(this->pstamp, desired->committed_prev->pstamp.load(memory_order_acquire));
	writeSet.push_back(SetElement(key, desired));
	
	//	avoid false positive
	auto itr = readSet.begin();
	while (itr != readSet.end()) {
		if ((*itr).key == key) {
			readSet.erase(itr);
			break;
		} else itr++;
	}

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
		pstamp = max(pstamp, (*itr).ver->committed_prev->pstamp.load(memory_order_acquire));
	}	

	// finalize pi(T)
	sstamp = min(sstamp, cstamp);
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t verSstamp = (*itr).ver->sstamp.load(memory_order_acquire);
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
		(*itr).ver->pstamp = max((*itr).ver->pstamp.load(memory_order_acquire), cstamp);
		// readers ビットを下げる
		uint64_t expected, desired;
		for(;;) {
			expected = (*itr).ver->readers.load(memory_order_acquire);
RETRY_SSN_COMMIT:
			if (!(expected & (1<<thid))) break;
			desired = expected;
			desired &= ~(1<<thid);
			if ((*itr).ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
			else goto RETRY_SSN_COMMIT;
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
		(*itr).ver->committed_prev->sstamp.store(verSstamp, memory_order_release);	
		// initialize new version
		(*itr).ver->pstamp = cstamp;
		(*itr).ver->cstamp = verCstamp;
	}

	//logging
	//?*
	
	// status, inFlight -> committed
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) 
		(*itr).ver->status.store(VersionStatus::committed, memory_order_release);

	this->status = TransactionStatus::committed;
	safeRetry = false;

	SsnLock.unlock();

	readSet.clear();
	writeSet.clear();
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
		uint64_t v_sstamp = (*itr).ver->sstamp;
		//最下位ビットが上がっていたら TID
		if (v_sstamp & 1) {
			//TID から worker を特定する
			uint8_t worker = (v_sstamp >> 1);
			// worker == thid はありえない
			if (worker == thid) {
				dispWS();
				dispRS();
				ERR;
			}
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
					while (statusTmp == TransactionStatus::committing) {
						statusTmp = TMT[worker].status.load(memory_order_acquire);
					}
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
		uint64_t rdrs = (*itr).ver->readers.load(memory_order_acquire);
		for (unsigned int worker = 1; worker <= THREAD_NUM; ++worker) {
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
						while (statusTmp == TransactionStatus::committing) {
							statusTmp = TMT[worker].status.load(memory_order_acquire);
						}
						if (statusTmp == TransactionStatus::committed) {
							this->pstamp = min(this->pstamp, TMT[worker].cstamp.load(memory_order_acquire));
						}
					}
				}
			} 
		}
		// re-read pstamp in case we missed any reader
		this->pstamp = min(this->pstamp, (*itr).ver->committed_prev->pstamp.load(memory_order_acquire));
	}

	// ssn_check_exclusion
	if (pstamp < sstamp) {
		status = TransactionStatus::committed;
		TMT[thid].sstamp.store(this->sstamp, memory_order_release);
		TMT[thid].lastcstamp.store(this->cstamp, memory_order_release);
		TMT[thid].status.store(TransactionStatus::committed, memory_order_release);
	} else {
		status = TransactionStatus::aborted;
		TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
		TMT[thid].prev_cstamp.store(this->cstamp, memory_order_release);
		return;
	}

	// update eta
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t ptmp = (*itr).ver->pstamp.load(memory_order_acquire);
		while (ptmp < this->cstamp) {
			if ((*itr).ver->pstamp.compare_exchange_weak(ptmp, this->cstamp, memory_order_acq_rel, memory_order_acquire)) break;
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
		(*itr).ver->committed_prev->sstamp.store(verSstamp, memory_order_release);
		(*itr).ver->cstamp.store(verCstamp, memory_order_release);
		(*itr).ver->pstamp.store(this->cstamp, memory_order_release);
	}

	//logging
	//?*
	
	// status, inFlight -> committed
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) 
		(*itr).ver->status.store(VersionStatus::committed, memory_order_release);

	safeRetry = false;
	TMT[thid].prev_cstamp.store(this->cstamp, memory_order_release);
	readSet.clear();
	writeSet.clear();
	return;
}

void
Transaction::abort()
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		(*itr).ver->committed_prev->sstamp.store(UINT64_MAX & ~(1), memory_order_release);
		(*itr).ver->status.store(VersionStatus::aborted, memory_order_release);
	}
	writeSet.clear();

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t expected, desired;
		for(;;) {
			expected = (*itr).ver->readers.load(memory_order_acquire);
RETRY_ABORT:
			if (!(expected & (1<<thid))) break;
			desired = expected & ~(1<<thid);
			if ((*itr).ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
			else goto RETRY_ABORT;
		}
	}
	readSet.clear();

	safeRetry = true;
}

void
Transaction::verify_exclusion_or_abort()
{
	if (this->pstamp >= this->sstamp) {
		this->status = TransactionStatus::aborted;
		TMT[thid].status.store(TransactionStatus::aborted, memory_order_release);
	}
}

void
Transaction::dispWS()
{
	cout << "th " << this->thid << " : write set : ";
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		cout << "(" << (*itr).key << ", " << (*itr).ver->val << "), ";
	}
	cout << endl;
}

void
Transaction::dispRS()
{
	cout << "th " << this->thid << " : read set : ";
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		cout << "(" << (*itr).key << ", " << (*itr).ver->val << "), ";
	}
	cout << endl;
}

