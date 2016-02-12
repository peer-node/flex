#ifndef FLEX_CALENDAR
#define FLEX_CALENDAR

#include "credits/credits.h"
#include "database/memdb.h"
#include "net/net.h"
#include "mining/work.h"
#include "relays/relaystate.h"

#include "log.h"
#define LOG_CATEGORY "calendar.h"

#define DIURN_DURATION (24 * 60 * 60 * 1000000ULL)
#define INITIAL_DIURNAL_DIFFICULTY 500000000ULL

class CalendarException : public std::runtime_error
{
public:
   explicit CalendarException(const std::string& msg)
      : std::runtime_error(msg)
   {}
};

uint160 GetDifficulty(int64_t cumulative_adjustments);

int64_t InferNumAdjustmentsFromDifficulty(uint160 difficulty);

uint160 ApplyAdjustments(uint160 diurnal_difficulty, int64_t num);

uint160 GetNextDiurnInitialDifficulty(MinedCredit last_calend_credit,
                                      MinedCredit calend_credit);


class Diurn
{
public:
    uint160 previous_diurn_root;
    DiurnalBlock diurnal_block;
    std::vector<MinedCreditMessage> credits_in_diurn;
    uint160 initial_difficulty;
    uint160 current_difficulty;

    Diurn():
        previous_diurn_root(0),
        initial_difficulty(INITIAL_DIURNAL_DIFFICULTY),
        current_difficulty(INITIAL_DIURNAL_DIFFICULTY)
    { }

    Diurn(uint160 previous_diurn_root):
        previous_diurn_root(previous_diurn_root)
    { }

    Diurn(const Diurn& diurn);

    Diurn& operator=(const Diurn& diurn);

    string_t ToString() const;

    void SetInitialDifficulty(uint160 difficulty);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(previous_diurn_root);
        READWRITE(diurnal_block);
        READWRITE(credits_in_diurn);
        READWRITE(initial_difficulty);
        READWRITE(current_difficulty);
    )

    std::vector<uint160> Hashes() const;

    uint64_t Size() const;

    void Add(uint160 credit_hash);

    void RemoveLast();

    uint160 Last() const;

    uint160 First() const;

    bool Contains(uint160 hash);

    uint160 BlockRoot() const;

    uint160 Root() const;

    uint160 Work() const;

    std::vector<uint160> Branch(uint160 credit_hash);
};


class Calend : public MinedCreditMessage
{
public:
    Calend () {}

    Calend(const MinedCreditMessage msg);

    uint160 MinedCreditHash();

    uint160 TotalCreditWork();

    uint160 Root() const;

    bool operator!=(Calend calend_);
};

class Calendar;

class CalendarFailureDetails
{
public:
    uint160 diurn_root;
    uint160 credit_hash;
    TwistWorkCheck check;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(diurn_root);
        READWRITE(credit_hash);
        READWRITE(check);
    )

    bool DataRequiredForCheckIsAvailable();

    void RequestDataRequiredForCheck(CNode* peer);

    bool VerifyInvalid();
};


typedef std::pair<MinedCredit, TwistWorkProof> CreditAndProof;
typedef std::vector<CreditAndProof> CreditAndProofList;

class Calendar
{
public:
    std::vector<Calend> calends;
    std::vector<CreditAndProofList> extra_work;
    Diurn current_diurn;
    CreditAndProofList current_extra_work;

    Calendar() { }

    Calendar(const Calendar& othercalendar);

    Calendar(uint160 credit_hash);

    ~Calendar()
    { }

    string_t ToString() const;

    void PopulateTopUpWork(uint160 credit_hash);

    void PopulateCalends(uint160 credit_hash);

    void PopulateDiurn(uint160 credit_hash);
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(calends);
        READWRITE(current_diurn);
        READWRITE(extra_work);
    )

    uint64_t Size() const;

    uint160 Hash160();

    bool ContainsDiurn(uint160 diurn_root);

    std::vector<uint160> GetCalendCreditHashes();

    uint160 GetLastCalendCreditHash();

    void HandleCalend(MinedCreditMessage& msg);

    void HandleBatch(MinedCreditMessage& msg);

    void HandleMinedCreditMessage(MinedCreditMessage& msg);

    void StoreTopUpWorkIfNecessary(MinedCreditMessage& msg);

    void AddMinedCreditToTip(MinedCreditMessage& msg);

    void FinishCurrentCalendAndStartNext(MinedCreditMessage msg);

    void RemoveLast();

    bool CheckRootsAndDifficulty();

    bool Validate();

    bool SpotCheckWork(CalendarFailureDetails& details);

    uint160 CalendWork() const;

    bool CheckTopUpWorkProofs();

    bool CheckTopUpWorkQuantities();

    bool CheckTopUpWorkCredits();

    bool CheckTopUpWork();

    uint160 TopUpWork() const;

    void TrimTopUpWork();

    void DropTopUpWorkCredits(uint32_t num_credits_to_drop);

    uint160 TotalWork() const;

    uint160 PreviousDiurnRoot() const;

    uint160 LastMinedCreditHash() const;

    MinedCredit LastMinedCredit();

    bool CreditInBatchHasValidConnection(CreditInBatch& credit);
};


#endif