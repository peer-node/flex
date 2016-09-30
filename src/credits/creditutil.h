#ifndef TELEPORT_CREDITUTIL
#define TELEPORT_CREDITUTIL

#include "database/memdb.h"
#include "teleportnode/wallet.h"
#include "net/net.h"
#include "relays/relaystate.h"

#define BLOCK_MEMORY_FACTOR 17      // 19 = 4 Gb, 18 = 2 Gb, 17 = 1 Gb &c.
#define MINED_CREDIT 1
#define CREDIT_TRANSFER 2
#define CREDIT_NUGGET 5000000
#define NUGGETS_PER_BATCH 1
#define LINKS_PER_PROOF 512
#define SEGMENTS_PER_PROOF 64


static CMemoryDB batchhashesdb;


uint160 KeyHashFromKeyData(vch_t keydata);

bool CreditInBatchHasValidConnectionToCalendar(CreditInBatch& credit);

std::vector<uint160> GetDiurnBranch(uint160 credit_hash);

uint160 GetCalendCreditHash(uint160 credit_hash);

uint160 GetNextCalendCreditHash(uint160 credit_hash);

vector<uint160> GetCreditHashesSinceCalend(uint160 calend_credit_hash,
                                           uint160 final_hash);

uint160 PreviousCreditHash(uint160 credit_hash);

std::vector<Credit> GetCreditsFromHashes(std::vector<uint160> hashes);

std::vector<uint64_t> GetBitsSetByTxs(std::vector<uint160> txs);

std::vector<uint64_t> GetBitsSetInBatch(uint160 credit_hash);

bool SeenBefore(uint160 credit_hash);

bool InMainChain(uint160 credit_hash);

void AddToMainChain(uint160 credit_hash);

void RemoveFromMainChain(uint160 credit_hash);

void ClearMainChain();

MinedCredit LatestMinedCredit();

BitChain GetSpentChain(uint160 credit_hash);

bool GetSpentChainDifferences(uint160 previous_mined_credit_hash,
                              uint160 target_credit_hash, 
                              std::vector<uint64_t> &to_set,
                              std::vector<uint64_t> &to_clear);

uint160 GetFork(uint160 hash1, uint160 hash2, uint32_t& stepsback);

bool HaveSameCalend(uint160 hash1, uint160 hash2);

bool TxContainsSpentInputs(SignedTransaction tx);

bool ChangeSpentChain(BitChain *spent_chain,
                      uint160 previous_credit_hash,
                      uint160 target_credit_hash);

bool PreviousBatchWasCalend(MinedCreditMessage& msg);

std::vector<Credit> GetCreditsFromMsg(MinedCreditMessage& msg);

bool TransactionsConflict(SignedTransaction tx1, SignedTransaction tx2);

bool GetTxDifferences(uint160 previous_credit_hash,
                      uint160 target_credit_hash,
                      std::vector<uint160> &to_add,
                      std::vector<uint160> &to_delete);

uint64_t TransactionDepth(uint160 txhash, uint160 current_credit_hash);

int64_t GetFeeSize(SignedTransaction tx);

void SetChildBlock(uint160 parent_hash, uint160 child_hash);

void RecordBatch(const MinedCreditMessage& msg);

void UpdateSpentChain(UnsignedTransaction tx);

uint64_t SpentChainLength(uint160 mined_credit_hash);

std::string string_to_hex(const std::string& input);

CreditBatch ReconstructBatch(MinedCreditMessage& msg);

std::vector<Credit> GetFeesOwed(RelayState state);

vector<uint160> MissingDataNeededToCalculateFees(RelayState state);

void StoreHash(uint160 hash);

bool IsCalend(uint160 mined_credit_hash);

void StoreMinedCredit(MinedCredit mined_credit);

void StoreMinedCreditMessage(MinedCreditMessage msg);

void StoreTransaction(SignedTransaction tx);

void StoreRecipients(CreditBatch& batch);

uint32_t GetStub(uint160 hash);

std::vector<CreditInBatch>
GetCreditsPayingToRecipient(std::vector<uint32_t> stubs,
                            uint64_t since_when);

std::vector<CreditInBatch> GetCreditsPayingToRecipient(uint160 keyhash);

uint64_t GetKnownPubKeyBalance(Point pubkey);

void RemoveCreditsOlderThan(uint64_t time_);

uint160 GetNextDifficulty(MinedCredit credit);

uint160 GetNextDifficulty(uint160 mined_credit_hash);

void RemoveBlocksAndChildren(uint160 credit_hash);

uint160 GetLastHash(CDataStore& datastore, string_t dimension);

uint160 MostWorkBatch();

uint160 MostReportedWorkBatch();

void FetchDependencies(std::vector<uint160> dependencies, CNode *peer);

string_t HistoryString(uint160 credit_hash);

#endif