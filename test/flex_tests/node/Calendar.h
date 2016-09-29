#ifndef FLEX_CALENDAR_H
#define FLEX_CALENDAR_H


#include <src/credits/CreditInBatch.h>
#include "Calend.h"
#include "Diurn.h"
#include "test/flex_tests/node/data_handler/CalendarFailureDetails.h"

class CreditSystem;

class Calendar
{
public:
    Calendar();

    Calendar(uint160 msg_hash, CreditSystem *credit_system);

    std::vector<Calend> calends;
    std::vector<std::vector<MinedCreditMessage> > extra_work;
    Diurn current_diurn;
    std::vector<MinedCreditMessage> current_extra_work;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(calends);
        READWRITE(current_diurn);
        READWRITE(extra_work);
    )

    JSON(calends, current_diurn, extra_work);

    bool operator==(const Calendar& other) const
    {
        return calends == other.calends and current_diurn == other.current_diurn and extra_work == other.extra_work;
    }

    bool operator!=(const Calendar& other) const
    {
        return not (*this == other);
    }

    uint160 CalendWork();

    bool ContainsDiurn(uint160 diurn_root);

    uint160 LastMinedCreditMessageHash();

    void PopulateDiurn(uint160 credit_hash, CreditSystem* credit_system);

    void PopulateCalends(uint160 credit_hash, CreditSystem *credit_system);

    void PopulateTopUpWork(uint160 credit_hash, CreditSystem *credit_system);

    uint160 TotalWork();

    uint160 TopUpWork();

    void AddTopUpWorkInDiurn(uint160 credit_work_in_diurn, uint160 total_credit_work, uint160 &total_calendar_work,
                             Calend &calend, CreditSystem* credit_system);

    bool CheckRootsAndDifficulties();

    bool CheckDiurnRootsAndCreditHashes();

    bool CheckCurrentDiurnDifficulties();

    bool CheckExtraWorkDifficulties();

    bool CheckCreditHashesInExtraWork();

    bool CheckCreditMessageHashesInCurrentDiurn();

    bool CheckDiurnRoots();

    bool CheckExtraWorkDifficultiesForDiurn(Calend calend, std::vector<MinedCreditMessage> msgs);

    static bool CheckDifficultiesOfConsecutiveSequenceOfMinedCreditMessages(std::vector<MinedCreditMessage> msgs);

    static bool CheckCreditMessageHashesInSequenceOfConsecutiveMinedCreditMessages(std::vector<MinedCreditMessage> msgs);

    bool CheckProofsOfWorkInCurrentDiurn(CreditSystem* credit_system);

    bool CheckProofsOfWorkInCalends(CreditSystem *credit_system);

    bool CheckProofsOfWorkInExtraWork(CreditSystem *credit_system);

    bool CheckProofsOfWork(CreditSystem *credit_system);

    void AddToTip(MinedCreditMessage &msg, CreditSystem *credit_system);

    void RemoveLast(CreditSystem *credit_system);

    bool ValidateCreditInBatch(CreditInBatch credit_in_batch);

    bool ValidateCreditInBatchUsingCurrentDiurn(CreditInBatch credit_in_batch);

    bool ValidateCreditInBatchUsingPreviousDiurn(CreditInBatch credit_in_batch, std::vector<uint160> long_branch);

    bool CheckCalendDifficulties(CreditSystem *credit_system);

    bool CheckDifficulties(CreditSystem *credit_system);

    bool CheckRootsAndDifficulties(CreditSystem *credit_system);

    bool SpotCheckWork(CalendarFailureDetails &details);

    std::vector<uint160> GetCalendHashes();

    MinedCreditMessage LastMinedCreditMessage();
};


#endif //FLEX_CALENDAR_H
