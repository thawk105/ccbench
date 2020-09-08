/**
 * @file interface.h
 * @brief transaction engine interface
 */

#pragma once

#include "scheme.h"
#include "scheme_global.h"
#include "tuple.h"

#define STRING(macro) #macro          // NOLINT
#define MAC2STR(macro) STRING(macro)  // NOLINT

namespace ccbench {
/**
 * @brief abort and end the transaction.
 *
 * do local set/cache clear, try gc.
 * @param[in] token the token retrieved by enter()
 * @pre it did enter -> ... -> tx_begin -> some access
 * operation(update/insert/search/delete) or no operation
 * @post execute leave to leave the session or tx_begin to start next
 * transaction.
 * @return Status::OK success.
 */
Status abort(Token token);  // NOLINT

/**
 * @brief close the specified scan_cache
 * @param token [in] the token retrieved by enter()
 * @param storage [in] the storage handle retrieved by register_storage() or
 * get_storage()
 * @param handle [in] identify the specific scan_cache.
 * @return Status::OK It succeeded.
 * @return Status::WARN_INVALID_HANDLE The @a handle is invalid.
 */
Status close_scan(Token token, Storage storage,  // NOLINT
                         ScanHandle handle);

/**
 * @brief silo's(SOSP2013) validation protocol. If this function return ERR_
 * status, this called abort function.
 * @param token retrieved by enter()
 * @pre executed enter -> tx_begin -> transaction operation.
 * @post execute leave to leave the session or tx_begin to start next
 * transaction.
 * @return Status::ERR_VALIDATION This means read validation failure and it
 * already executed abort(). After this, do tx_begin to start next transaction
 * or leave to leave the session.
 * @return Status::ERR_WRITE_TO_DELETED_RECORD This transaction was interrupted
 * by some delete transaction between read phase and validation phase. So it
 * called abort.
 * @return Status::OK success.
 */
Status commit(Token token);  // NOLINT

/**
 * @brief Delete the all records.
 * @pre This function is called by a single thread and does't
 * allow moving of other threads.
 * @details This function executes tx_begin(Token token)
 * internaly, so it doesn't need to call tx_begin(Token token).
 * Also it doesn't need to call enter/leave around calling this
 * function. Because this function calls enter/leave
 * appropriately.
 * @return Status::WARN_ALREADY_DELETE There are no records.
 * @return Status::OK success
 * @return Return value of commit function. If it return this,
 * you can retry delete_all_records meaning to resume this
 * function.
 */
[[maybe_unused]] Status delete_all_records();  // NOLINT

/**
 * @brief delete the record for the given key
 * @param token [in] the token retrieved by enter()
 * @param storage [in] the storage handle retrieved by register_storage() or
 * get_storage()
 * @param key the key of the record for deletion
 * @pre it already executed enter.
 * @post nothing. This function never do abort.
 * @return Status::WARN_NOT_FOUND No corresponding record in masstree. If you
 * have problem by WARN_NOT_FOUND, you should do abort.
 * @return Status::OK success.
 * @return Status::WARN_CANCEL_PREVIOUS_OPERATION it canceled an update/insert
 * operation before this fucntion and did delete operation.
 */
Status delete_record(Token token, Storage st, std::string_view key);

/**
 * @brief delete existing storage and records under the storage.
 * @param storage [in] the storage handle retrieved with register_storage() or
 * get_storage()
 * @return Status::OK if successful
 * @return Status::ERR_NOT_FOUND If the storage is not registered with the given
 * name
 */
[[maybe_unused]] Status delete_storage(Storage storage);  // NOLINT

/**
 * @brief enter session
 * @param[out] token output parameter to return the token
 * @pre Maximum degree of parallelism of this function without leave is the size
 * of kThreadTable, KVS_MAX_PARALLEL_THREADS.
 * @post When it ends this session, do leave(Token token).
 * @return Status::OK
 * @return Status::ERR_SESSION_LIMIT There are no capacity of session.
 */
Status enter(Token &token);  // NOLINT

/**
 * @brief do delete operations for all records, join core threads and delete the
 * remaining garbage (heap) objects.
 * @pre It already did init() and invoked core threads.
 * @details It do delete operations for all records.
 * init() did invoking core threads detached. So it should join those threads.
 * This function serves that joining after doing those delete operations.
 * Then, it delete the remaining garbage (heap) object by using private
 * interface.
 */
void fin();

/**
 * @brief get existing storage handle
 * @param name the name of the storage
 * @param storage output parameter to pass the storage handle,
 * that is used for the subsequent calls related with the storage.
 * @return Status::OK if successful
 * @return Status::ERR_NOT_FOUND If the storage is not registered with the given
 * name
 */
[[maybe_unused]] Status get_storage(std::string_view name,  // NOLINT
                                           Storage &storage);

/**
 * @brief initialize shirakami environment
 * @details When it starts to use this system, in other words, it starts to
 * build database, it must be executed first.
 * @param[in] log_directory_path of WAL directory.
 * @return Status::ERR_INVALID_ARGS The args as a log directory path is invalid.
 * Some files which has the same path exist.
 * @return Status::OK
 */
Status init(                                                // NOLINT
        std::string_view log_directory_path = MAC2STR(PROJECT_ROOT));  // NOLINT

/**
 * @brief insert the record with given key/value
 * @param token [in] the token retrieved by enter()
 * @param storage [in] the storage handle retrieved by register_storage() or
 * get_storage()
 * @param key the key of the inserted record
 * @param val the value of the inserted record
 * @return Status::WARN_ALREADY_EXISTS The records whose key is the same as @a
 * key exists in MTDB, so this function returned immediately.
 * @return Status::OK success
 * @return Status::WARN_WRITE_TO_LOCAL_WRITE it already executed
 * update/insert/upsert, so it update the local write set object.
 */
Status insert(Token token, Storage st, std::string_view key, std::string_view val, std::align_val_t val_align, Tuple** tuple_out = nullptr);
Status insert(Token token, Storage st, Tuple&& tuple, Tuple** tuple_out = nullptr);

/**
 * @brief leave session
 * @details It return the objects which was got at enter function to
 * kThreadTable.
 * @param token retrieved by enter()
 * @return Status::OK if successful
 * @return Status::WARN_NOT_IN_A_SESSION If the session is already ended.
 */
Status leave(Token token);  // NOLINT

/**
 * @brief This function preserve the specified range of masstree
 * @param[in] token the token retrieved by enter()
 * @param[in] storage the storage handle retrieved by register_storage() or
 * get_storage()
 * @param[in] left_key
 * @param[in] l_exclusive
 * @param[in] right_key
 * @param[in] r_exclusive
 * @param[out] handle the handle to identify scanned result. This handle will be
 * deleted at abort function.
 * @return Status::WARN_SCAN_LIMIT The scan could find some records but could
 * not preserve result due to capacity limitation.
 * @return Status::WARN_NOT_FOUND The scan couldn't find any records.
 * @return Status::OK the some records was scanned.
 */
Status open_scan(Token token, Storage storage,  // NOLINT
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        ScanHandle &handle);

/**
 * @brief This function reads the one records from the scan_cache
 * which was created at open_scan function.
 * @details The read record is returned by @result.
 * @param token [in] the token retrieved by enter()
 * @param storage [in] the storage handle retrieved by register_storage() or
 * get_storage()
 * @param handle [in] input parameters to identify the specific scan_cache.
 * @param result [out] output parmeter to pass the read record.
 * @return Status::WARN_ALREADY_DELETE The read targets was deleted by delete
 * operation of this transaction.
 * @return Status::WARN_CONCURRENT_DELETE The read targets was deleted by delete
 * operation.
 * @return Status::WARN_INVALID_HANDLE The @a handle is invalid.
 * @return Status::WARN_READ_FROM_OWN_OPERATION It read the records from it's
 * preceding write (insert/update/upsert) operation in the same tx.
 * @return Status::WARN_SCAN_LIMIT It have read all records in the scan_cache.
 * @return Status::OK It succeeded.
 */
Status read_from_scan(Token token, Storage storage, ScanHandle handle, Tuple **result); // NOLINT

/**
 * @brief register new storage, which is used to separate the KVS's key space,
 * any records in the KVS belong to only one storage
 * @param name of the storage
 * @param len_name the length of the name
 * @param storage output parameter to pass the storage handle,
 * that is used for the subsequent calls related with the storage.
 * @return Status::OK if successful
 */
[[maybe_unused]] Status register_storage(char const *name,  // NOLINT
                                                std::size_t len_name,
                                                Storage &storage);

/**
 * @brief search with the given key range and return the found tuples
 * @param[in] token the token retrieved by enter()
 * @param[in] storage the storage handle retrieved by register_storage() or
 * get_storage()
 * @param[in] left_key the key to indicate the beginning of the range, null if
 * the beginning is open
 * @param l_exclusive indicate whether the lkey is exclusive
 * (i.e. the record whose key equal to lkey is not included in the result)
 * @param[in] right_key the key to indicate the ending of the range, null if the
 * end is open
 * @param[in] r_exclusive indicate whether the rkey is exclusive
 * @param[out] result output parameter to pass the found Tuple pointers.
 * Empty when nothing is found for the given key range.
 * Returned tuple pointers are valid untill commit/abort.
 * @return Status::OK success.
 * @return Status::WARN_ALREADY_DELETE The read targets was deleted by delete
 * operation of this transaction.
 * @return Status::WARN_CONCURRENT_DELETE The read targets was deleted by delete
 * operation.
 */
Status scan_key(Token token, Storage storage,  // NOLINT
                       std::string_view left_key, bool l_exclusive,
                       std::string_view right_key, bool r_exclusive,
                       std::vector<const Tuple *> &result);

/**
 * @brief This function checks the size resulted at open_scan with the @a
 * handle.
 * @param token [in] the token retrieved by enter()
 * @param storage [in] the storage handle retrieved by register_storage() or
 * get_storage()
 * @param handle [in] the handle to identify scanned result. This handle will be
 * deleted at abort function.
 * @param size [out] the size resulted at open_scan with the @a handle .
 * @return Status::WARN_INVALID_HANDLE The @a handle is invalid.
 * @return Status::OK success.
 */
[[maybe_unused]] Status scannable_total_index_size(  // NOLINT
        Token token,                                            // NOLINT
        Storage storage, ScanHandle &handle, std::size_t &size);

/**
 * @brief search with the given key and return the found tuple
 * @param token [in] the token retrieved by enter()
 * @param storage [in] the storage handle retrieved by register_storage() or
 * get_storage()
 * @param key the search key
 * @param tuple output parameter to pass the found Tuple pointer.
 * The ownership of the address which is pointed by the tuple is in shirakami.
 * So upper layer from shirakami don't have to be care.
 * nullptr when nothing is found for the given key.
 * @return Status::OK success.
 * @return Status::WARN_ALREADY_DELETE The read targets was deleted by delete
 * operation of this transaction.
 * @return Status::WARN_NOT_FOUND no corresponding record in masstree. If you
 * have problem by WARN_NOT_FOUND, you should do abort.
 * @return Status::WARN_CONCURRENT_DELETE The read targets was deleted by delete
 * operation of concurrent transaction.
 */
Status search_key(Token token, Storage storage,  // NOLINT
                         std::string_view key, Tuple **tuple);

/**
 * @brief Recovery by single thread.
 * @details This function isn't thread safe.
 * @pre It must decide correct wal directory name decided by
 * change_wal_directory function before it executes recovery.
 */
[[maybe_unused]] void single_recovery_from_log();

/**
 * @brief update the record for the given key
 * @param token [in] the token retrieved by enter()
 * @param storage [in] the storage handle retrieved by register_storage() or
 * get_storage()
 * @param key the key of the updated record
 * @param val the value of the updated record
 * @return Status::OK if successful
 * @return Status::WARN_NOT_FOUND no corresponding record in masstree. If you
 * have problem by WARN_NOT_FOUND, you should do abort.
 * @return Status::WARN_WRITE_TO_LOCAL_WRITE It already executed update/insert,
 * so it update the value which is going to be updated.
 */
Status update(Token token, Storage st, std::string_view key, std::string_view val, std::align_val_t val_align, Tuple** tuple_out = nullptr);
Status update(Token token, Storage st, Tuple&& tuple, Tuple** tuple_out = nullptr);

/**
 * @brief update the record for the given key, or insert the key/value if the
 * record does not exist
 * @param[in] token the token retrieved by enter()
 * @param[in] storage the storage handle retrieved by register_storage() or
 * get_storage()
 * @param key the key of the upserted record
 * @param val the value of the upserted record
 * @return Status::OK success
 * @return Status::WARN_WRITE_TO_LOCAL_WRITE It already did
 * insert/update/upsert, so it overwrite its local write set.
 */
Status upsert(Token token, Storage st, std::string_view key, std::string_view val, std::align_val_t val_align);
Status upsert(Token token, Storage st, Tuple&& tuple);

}  // namespace ccbench
