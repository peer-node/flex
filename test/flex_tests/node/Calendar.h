#ifndef FLEX_CALENDAR_H
#define FLEX_CALENDAR_H


#include "Calend.h"
#include "Diurn.h"

class CreditSystem;

class Calendar
{
public:
    Calendar();

    Calendar(uint160 credit_hash, CreditSystem *credit_system);

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

    uint160 CalendWork();

    std::vector<uint160> GetCalendCreditHashes();

    bool ContainsDiurn(uint160 diurn_root);

    uint160 LastMinedCreditHash();

    MinedCredit LastMinedCredit();

    void PopulateDiurn(uint160 credit_hash, CreditSystem* credit_system);

    void PopulateCalends(uint160 credit_hash, CreditSystem *credit_system);

    void PopulateTopUpWork(uint160 credit_hash, CreditSystem *credit_system);

    uint160 TotalWork();

    uint160 TopUpWork();

    void AddTopUpWorkInDiurn(uint160 credit_work_in_diurn, uint160 total_credit_work, uint160 &total_calendar_work,
                             Calend &calend, CreditSystem* credit_system);

    bool CheckRootsAndDifficulties();

    bool CheckRoots();

    bool CheckDifficulties();

    bool CheckCurrentDiurnDifficulties();

    bool CheckCalendDifficulties();

    bool CheckExtraWorkDifficulties();

    bool CheckExtraWorkRoots();

    bool CheckCurrentDiurnRoots();

    bool CheckCalendRoots();
};


#endif //FLEX_CALENDAR_H
