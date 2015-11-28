#include "flexnode/calendar.h"
#include "flexnode/flexnode.h"
#include "credits/creditutil.h"

#include "log.h"
#define LOG_CATEGORY "calendar.cpp"

uint160 GetDifficulty(int64_t cumulative_adjustments)
{
    uint160 difficulty = INITIAL_DIURNAL_DIFFICULTY;
    if (cumulative_adjustments > 0)
    {
        for (int64_t i = 0; i < cumulative_adjustments; i++)
        {
            difficulty = difficulty * 999 / 1000;
        }
    }
    else
        for (int64_t i = cumulative_adjustments; i < 0; i++)
        {
            difficulty = difficulty * 1000 / 999;
        }
    return difficulty;
}


int64_t InferNumAdjustmentsFromDifficulty(uint160 difficulty)
{
    uint64_t num_adjustments = 0;
    if (difficulty == INITIAL_DIURNAL_DIFFICULTY)
        return num_adjustments;

    uint160 diff = INITIAL_DIURNAL_DIFFICULTY;

    if (difficulty > diff)
    {    
        while (difficulty > diff)
        {
            num_adjustments--;
            diff = diff * 1000 / 999;
        }
        return num_adjustments;
    }
    while (difficulty < diff)
    {
        num_adjustments++;
        diff = diff * 999 / 1000;
    }
    return num_adjustments;
}

uint160 ApplyAdjustments(uint160 diurnal_difficulty, int64_t num)
{
    int64_t previous_adjustments
        = InferNumAdjustmentsFromDifficulty(diurnal_difficulty);
    int64_t adjustments = previous_adjustments + num;
    
    uint160 result = GetDifficulty(adjustments);

    return result;
}

uint160 GetNextDiurnInitialDifficulty(MinedCredit last_calend_credit,
                                             MinedCredit calend_credit)
{

    int64_t num_block_adjustments = calend_credit.batch_number
                                     - last_calend_credit.batch_number - 1;

    uint160 diurn_final_difficulty = calend_credit.diurnal_difficulty;

 
    uint64_t previous_diurn_duration = calend_credit.timestamp
                                        - last_calend_credit.timestamp;

    int64_t diurn_adjustments;
    if (previous_diurn_duration > DIURN_DURATION)
        diurn_adjustments = 60;
    else
        diurn_adjustments = -60;
    
    return ApplyAdjustments(diurn_final_difficulty, 
                            diurn_adjustments - num_block_adjustments);
}





/***********
 *  Diurn
 */

    Diurn::Diurn(const Diurn& diurn)
    {
        previous_diurn_root = diurn.previous_diurn_root;
        initial_difficulty = diurn.initial_difficulty;
        current_difficulty = diurn.current_difficulty;
        diurnal_block.setvch(diurn.diurnal_block.getvch());
        credits_in_diurn = diurn.credits_in_diurn;
    }

    Diurn& Diurn::operator=(const Diurn& diurn)
    {
        previous_diurn_root = diurn.previous_diurn_root;
        initial_difficulty = diurn.initial_difficulty;
        current_difficulty = diurn.current_difficulty;
        diurnal_block.setvch(diurn.diurnal_block.getvch());
        credits_in_diurn = diurn.credits_in_diurn;
        return *this;
    }

    void Diurn::SetInitialDifficulty(uint160 difficulty)
    {
        initial_difficulty = difficulty;
        current_difficulty = difficulty;
    }

    std::vector<uint160> Diurn::Hashes() const
    {
        return diurnal_block.Hashes();
    }

    uint64_t Diurn::Size() const
    {
        return diurnal_block.Size();
    }

    void Diurn::Add(uint160 credit_hash)
    {
        diurnal_block.Add(credit_hash);
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        credits_in_diurn.push_back(msg);
        current_difficulty = ApplyAdjustments(current_difficulty, 1);
    }

    void Diurn::RemoveLast()
    {
        diurnal_block.RemoveLast();
        credits_in_diurn.pop_back();
        current_difficulty = ApplyAdjustments(current_difficulty, -1);
    }

    uint160 Diurn::Last() const
    {
        if (diurnal_block.Size() == 0)
            return 0;
        return diurnal_block.Last();
    }

    uint160 Diurn::First() const
    {
        if (diurnal_block.Size() == 0)
            return 0;
        return diurnal_block.First();
    }

    bool Diurn::Contains(uint160 hash)
    {
        return diurnal_block.Contains(hash);
    }

    uint160 Diurn::BlockRoot() const
    {
        DiurnalBlock block;
        block.setvch(diurnal_block.getvch());
        return block.Root();
    }

    uint160 Diurn::Root() const
    {
        return SymmetricCombine(previous_diurn_root, BlockRoot());
    }

    uint160 Diurn::Work() const
    {
        uint160 work = 0;
        std::vector<uint160> credit_hashes = Hashes();

        for (uint32_t i = 0; i < credits_in_diurn.size(); i++)
        {
            if (i >= credit_hashes.size())
                break;
            if (credits_in_diurn[i].mined_credit.GetHash160()
                == credit_hashes[i])
                work += credits_in_diurn[i].mined_credit.difficulty;
        }

        return work;
    }

    std::vector<uint160> Diurn::Branch(uint160 credit_hash)
    {
        if (!Contains(credit_hash))
        {
            log_ << "diurn: can't get branch for " << credit_hash
                 << " which is not in diurn\n";
        }
        std::vector<uint160> branch = diurnal_block.Branch(credit_hash);
        branch.push_back(previous_diurn_root);
        branch.push_back(Root());
        return branch;
    }

    string_t Diurn::ToString() const
    {
        stringstream ss;
        ss << "\n============== Diurn =============" << "\n";
        foreach_(uint160 credit_hash, Hashes())
        {
            MinedCredit credit = creditdata[credit_hash]["mined_credit"];
            ss << "== " << credit_hash.ToString() 
               << " (" <<  credit.difficulty.ToString() << ")" << "\n";
        }

        ss << "== " << "\n"
           << "== Previous Diurn Root:" 
           << previous_diurn_root.ToString() << "\n"
           << "== Total Work: " << Work().ToString() << "\n"
           << "== " << "\n"
           << "== Root: " << Root().ToString() << "\n"
           << "============ End Diurn ===========" << "\n";
        return ss.str();
    }

/*
 *  Diurn
 ***********/


/*************
 *  Calend
 */

    Calend::Calend(const MinedCreditMessage msg)
    {
        hash_list = msg.hash_list;
        timestamp = msg.timestamp;
        proof = msg.proof;
        mined_credit = msg.mined_credit;
    }

    uint160 Calend::MinedCreditHash()
    {
        return mined_credit.GetHash160();
    }

    uint160 Calend::TotalCreditWork()
    {
        return mined_credit.total_work + mined_credit.difficulty;
    }

    uint160 Calend::Root() const
    {
        return SymmetricCombine(mined_credit.previous_diurn_root,
                                mined_credit.diurnal_block_root);
    }

    bool Calend::operator!=(Calend calend_)
    {
        return mined_credit.GetHash160() != calend_.mined_credit.GetHash160();
    }

/*
 *  Calend
 ************/



/****************************
 *  CalendarFailureDetails
 */

    bool CalendarFailureDetails::DataRequiredForCheckIsAvailable()
    {
        if (creditdata[credit_hash].HasProperty("msg") &&
            calendardata[diurn_root].HasProperty("diurn") &&
            workdata[check.proof_hash].HasProperty("proof"))
        {
            return true;
        }
        return false;
    }

    void CalendarFailureDetails::RequestDataRequiredForCheck(CNode* peer)
    {
        std::vector<uint160> requested_batches;
        requested_batches.push_back(credit_hash);
        peer->PushMessage("reqinitdata", "getbatches", 
                          requested_batches);
    }

    bool CalendarFailureDetails::VerifyInvalid()
    {
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        Diurn diurn = calendardata[diurn_root]["diurn"];
        if (!diurn.Contains(credit_hash))
            return false;
        return check.VerifyInvalid();
    }

/*
 *  CalendarFailureDetails
 ****************************/


/**************
 *  Calendar
 */

    Calendar::Calendar(const Calendar& othercalendar)
    {
        calends = othercalendar.calends;
        current_diurn = othercalendar.current_diurn;
        extra_work = othercalendar.extra_work;
        current_extra_work = othercalendar.current_extra_work;
    }

    Calendar::Calendar(uint160 credit_hash)
    {
        if (credit_hash == 0)
            return;
        PopulateCalends(credit_hash);
        PopulateDiurn(credit_hash);
        PopulateTopUpWork(credit_hash);
    }

    string_t Calendar::ToString() const
    {
        stringstream ss;
        ss << "\n============== Calendar =============" << "\n"
           << "== Latest Credit Hash: " 
           << LastMinedCreditHash().ToString() << "\n"
           << "==" << "\n"
           << "== Calends:" << "\n";

        foreach_(Calend calend, calends)
            ss << "== " << calend.mined_credit.GetHash160().ToString() 
               << " (" << calend.mined_credit.diurnal_difficulty.ToString()
               << ")" << " root: "
               << calend.Root().ToString() << "\n";
        
        ss << "==" << "\n"
           << "== Current Diurn:" << "\n";
        
        foreach_(MinedCreditMessage msg, current_diurn.credits_in_diurn)
        {
            ss << "== " << msg.mined_credit.GetHash160().ToString()
               << " (" <<  msg.mined_credit.difficulty.ToString() 
               << ")" << "\n";
        }
        
        ss << "==" << "\n"
           << "== Top Up Work:" << "\n";

        for (uint32_t i = 0; i < extra_work.size(); i++)
        {
            CreditAndProofList list = extra_work[i];
            for (uint32_t j = 0; j < list.size(); j++)
            {
                ss << "== Calend " << i << "   "
                   << list[j].first.GetHash160().ToString() 
                   << " (" << list[j].first.difficulty.ToString() 
                    << ")"<< "\n";
            }
        }
        ss << "== " << "\n"
           << "== Total Work: " << TotalWork().ToString() << "\n"
           << "============ End Calendar ===========" << "\n";
        return ss.str();
    }

    void Calendar::PopulateCalends(uint160 credit_hash)
    {
        log_ << "populating calends: " << credit_hash << "\n";

        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
        uint160 diurn_root;
        if (msg.proof.DifficultyAchieved() > mined_credit.diurnal_difficulty)
            diurn_root = SymmetricCombine(mined_credit.diurnal_block_root,
                                          mined_credit.previous_diurn_root);
        else
            diurn_root = mined_credit.previous_diurn_root;

        while (diurn_root != 0)
        {
            uint160 calend_hash = calendardata[diurn_root]["credit_hash"];
            MinedCreditMessage calend_msg = creditdata[calend_hash]["msg"];
            Calend calend(calend_msg);
            calends.push_back(calend);

            diurn_root = calend.mined_credit.previous_diurn_root;
            MinedCredit calend_credit = calend_msg.mined_credit;
        }
        std::reverse(calends.begin(), calends.end());
    }

    uint64_t Calendar::Size() const
    {
        return calends.size();
    }

    bool Calendar::ContainsDiurn(uint160 diurn_root)
    {
        foreach_(const Calend& calend, calends)
        {
            if (calend.Root() == diurn_root)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<uint160> Calendar::GetCalendCreditHashes()
    {
        std::vector<uint160> calend_credit_hashes;

        foreach_(Calend& calend, calends)
            calend_credit_hashes.push_back(calend.mined_credit.GetHash160());

        return calend_credit_hashes;
    }

    uint160 Calendar::GetLastCalendCreditHash()
    {
        if (calends.size() == 0)
            return 0;
        return calends.back().mined_credit.GetHash160();
    }

    uint160 Calendar::Hash160()
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return ::Hash160(ss.begin(), ss.end());
    }

    void Calendar::HandleBatch(MinedCreditMessage& msg)
    {
        uint160 difficulty = msg.proof.DifficultyAchieved();

        log_ << "not a calend: " << difficulty 
             << " < " << msg.mined_credit.diurnal_difficulty << "\n";

        if (msg.mined_credit.previous_diurn_root
            == current_diurn.previous_diurn_root
            && msg.mined_credit.previous_mined_credit_hash
               == current_diurn.Last())
        {
            uint160 credit_hash = msg.mined_credit.GetHash160();
            log_ << "adding to current diurn: " << credit_hash << "\n";
            current_diurn.Add(credit_hash);
        }
        else
        {
            log_ << "WARNING: not adding to diurn:\n"
                 << "diurn roots match: "
                 << msg.mined_credit.previous_diurn_root << " vs "
                 << current_diurn.previous_diurn_root << "\n"
                 << "prev credit hashes match:"
                 << msg.mined_credit.previous_mined_credit_hash
                 << " vs " << current_diurn.Last() << "\n"
                 << "calendar is " <<  ToString() << "\n";
        }        
    }

    void Calendar::HandleMinedCreditMessage(MinedCreditMessage& msg)
    {
        log_ << "HandleMinedCreditMessage(): handling: " << msg.mined_credit;

        uint160 difficulty = msg.proof.DifficultyAchieved();
        
        if (difficulty >= msg.mined_credit.diurnal_difficulty)
        {
            HandleCalend(msg);
        }
        else
        {
            HandleBatch(msg);
            StoreTopUpWorkIfNecessary(msg);
        }
        log_ << "HandleMinedCreditMessage(): final calendar is "
             << *this << "\n";
    }

    void Calendar::StoreTopUpWorkIfNecessary(MinedCreditMessage& msg)
    {
        if (CalendWork() 
            + msg.mined_credit.diurnal_difficulty
            + TopUpWork()
                                <= msg.mined_credit.total_work 
                                   + msg.mined_credit.difficulty)
            current_extra_work.push_back(std::make_pair(msg.mined_credit, 
                                                        msg.proof));
        TrimTopUpWork();
    }

    void Calendar::AddMinedCreditToTip(MinedCreditMessage& msg)
    {
        if (msg.mined_credit.previous_mined_credit_hash
            != LastMinedCreditHash())
        {
            throw WrongBatchException("Calendar tip: "
                                      + LastMinedCreditHash().ToString()
                                      + " does not precede "
                                      + msg.mined_credit.ToString());
        }
        else
        {
            HandleMinedCreditMessage(msg);
        }
    }

    void Calendar::FinishCurrentCalendAndStartNext(MinedCreditMessage msg)
    {
        uint160 calend_hash = GetLastCalendCreditHash();

        MinedCredit calend_credit = creditdata[calend_hash]["mined_credit"];
        uint160 next_difficulty
            = GetNextDiurnInitialDifficulty(calend_credit, msg.mined_credit);

        calends.push_back(Calend(msg));

        
        uint160 previous_diurn_root = current_diurn.Root();
        
        calendardata[previous_diurn_root]["diurn"] = current_diurn;

        uint160 credit_hash = msg.mined_credit.GetHash160();

        RecordRelayState(GetRelayState(credit_hash), credit_hash);

        Diurn previous_diurn = current_diurn;

        current_diurn = Diurn(previous_diurn_root);

        current_diurn.Add(credit_hash);

        current_diurn.SetInitialDifficulty(next_difficulty);
    }

    void Calendar::RemoveLast()
    {
        if (current_diurn.Size() == 0)
            return;
        MinedCredit last_credit = LastMinedCredit();

        *this = Calendar(last_credit.previous_mined_credit_hash);
    }

    bool Calendar::CheckRootsAndDifficulty()
    {
        if (calends.size() > 0
            && calends[0].mined_credit.diurnal_difficulty 
               != ApplyAdjustments(INITIAL_DIURNAL_DIFFICULTY,
                                   calends[0].mined_credit.batch_number - 1))
        {
            log_ << "calends.size() = " << calends.size()
                 << ", but diurnal_difficulty = "
                 << calends[0].mined_credit.diurnal_difficulty
                 << " != " 
                 << ApplyAdjustments(INITIAL_DIURNAL_DIFFICULTY,
                                     calends[0].mined_credit.batch_number - 1)
                 << "\n";
            return false;
        }

        for (uint32_t i = 0; i < calends.size(); i++)
        {    
            if (i > 0 && calends[i].mined_credit.previous_diurn_root 
                          != calends[i - 1].Root())
            {
                log_ << "step " << i 
                     << ": previous_diurn_root != previous root\n";
                return false;
            }

            uint160 difficulty = calends[i].mined_credit.diurnal_difficulty;

            if (i > 1)
            {
                uint160 correct_initial_difficulty
                  = ::GetNextDiurnInitialDifficulty(calends[i - 2].mined_credit,
                                                    calends[i - 1].mined_credit);
                int64_t diurn_size;

                diurn_size = calends[i].mined_credit.batch_number
                              - calends[i - 1].mined_credit.batch_number - 1;

                uint160 correct_final_difficulty
                    = ApplyAdjustments(correct_initial_difficulty, diurn_size);
                
                if (difficulty != correct_final_difficulty)
                {
                    log_ << "step " << i << ": diff check: " 
                         << difficulty << " != " << correct_final_difficulty
                         << "\n";
                    return false;
                }
            }
            log_ << "checking difficulty of " << i << "th proof\n";
            if (calends[i].proof.DifficultyAchieved() < difficulty)
            {
                log_ << "step " << i << ": proof difficulty failed: "
                     << calends[i].proof.DifficultyAchieved()
                     << " vs" << difficulty << "\n";
                return false;
            }
            log_<< "proof ok\n";
        }
        if (current_diurn.Size() > 0)
        {
            log_ << "checking current diurn\n";
            uint160 last_credit_hash = current_diurn.Last();
            log_ << "last credit hash is " << last_credit_hash << "\n";

            if (creditdata[last_credit_hash].HasProperty("msg"))
            {
                if (current_diurn.previous_diurn_root != PreviousDiurnRoot())
                {
                    log_ << "current diurn says previous diurn root was "
                         << current_diurn.previous_diurn_root
                         << "but calendar says it was " << PreviousDiurnRoot()
                         << "\n";
                    return false;
                }
            }
        }
        else
        {
            if (calends.size() != 0)
                return false;
        }
        log_ << "calendar difficulty and roots ok\n";
        return true;
    }

    bool Calendar::Validate()
    {
        CalendarFailureDetails details;
        details.credit_hash = 0;
        
        foreach_(const Calend& calend, calends)
        {
            if (calendardata[calend.Root()].HasProperty("failure_details"))
            {
                log_ << "Validate(): found recorded failure details\n";
                details = calendardata[calend.Root()]["failure_details"];
                return false;
            }
        }
        if (!CheckRootsAndDifficulty())
        {
            log_ << "roots and difficulty check failed\n";
            return false;
        }
        return true;
    }

    bool Calendar::SpotCheckWork(CalendarFailureDetails& details)
    {
        for (uint32_t i = 0; i < calends.size(); i++)
        {
            TwistWorkCheck check = calends[i].proof.SpotCheck();
            
            if (!check.Valid())
            {
                uint160 diurn_root = calends[i].Root();
                details.diurn_root = diurn_root;
                details.credit_hash = calends[i].mined_credit.GetHash160();
                details.check = check;
                calendardata[diurn_root]["failure_details"] = details;
                return false;
            }
        }
        return true;
    }

    uint160 Calendar::CalendWork() const
    {
        uint160 calend_work = 0;

        foreach_(const Calend& calend, calends)
            calend_work += calend.mined_credit.diurnal_difficulty;

        return calend_work;
    }

    bool Calendar::CheckTopUpWorkProofs()
    {
        foreach_(CreditAndProofList& credit_and_proof_list, extra_work)
        {
            foreach_(CreditAndProof& credit_and_proof, credit_and_proof_list)
            {
                MinedCredit credit = credit_and_proof.first;
                TwistWorkProof proof = credit_and_proof.second;

                if (proof.WorkDone() != credit.difficulty ||
                    !proof.SpotCheck().Valid())
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool Calendar::CheckTopUpWorkQuantities()
    {
        if (extra_work.size() != calends.size())
            return false;

        MinedCredit credit, next_credit;
        TwistWorkProof proof;

        uint160 last_credit_work = 0;
        for (uint32_t i = 0; i < extra_work.size(); i++)
        {
            CreditAndProofList credit_and_proof_list = extra_work[i];

            uint160 top_up_work = 0;
            foreach_(CreditAndProof& credit_and_proof, credit_and_proof_list)
                top_up_work += credit_and_proof.second.WorkDone();

            uint160 calend_work = calends[i].mined_credit.diurnal_difficulty;
            uint160 credit_work = calends[i].TotalCreditWork()
                                    - last_credit_work;

            if (calend_work > credit_work && top_up_work > 0)
                return false;
            if (calend_work + top_up_work < credit_work)
                return false;
        }
        return true;
    }

    bool Calendar::CheckTopUpWorkCredits()
    {
        MinedCredit credit, next_credit;
        TwistWorkProof proof;

        uint32_t calend_number = 0;
        foreach_(CreditAndProofList credit_and_proof_list, extra_work)
        {
            credit.batch_number = 0;
           
            foreach_(CreditAndProof& credit_and_proof, credit_and_proof_list)
            {
                next_credit = credit_and_proof.first;
                proof = credit_and_proof.second;

                bool ok = next_credit.difficulty == proof.WorkDone() &&
                          (credit.batch_number == 0 ||
                           (next_credit.previous_mined_credit_hash 
                                                == credit.GetHash160() &&
                            next_credit.batch_number == credit.batch_number + 1));
                if (!ok)
                    return false;
                    
                credit = next_credit;
            }
            if (calends[calend_number].mined_credit.previous_mined_credit_hash
                    != credit.GetHash160())
                return false;

            calend_number += 1;
        }
        return true;
    }

    bool Calendar::CheckTopUpWork()
    {
        return CheckTopUpWorkQuantities() &&
               CheckTopUpWorkCredits()    &&
               CheckTopUpWorkProofs();
    }

    uint160 Calendar::TopUpWork() const
    {
        uint160 top_up_work(0);

        if (calends.size() != extra_work.size())
            throw CalendarException("Extra work size does not match "
                                    "calends size: " 
                                    + to_string(extra_work.size()) + " vs "
                                    + to_string(calends.size()));

        for (uint32_t i = 0; i < extra_work.size(); i++)
        {
            CreditAndProofList credit_and_proof_list = extra_work[i];

            foreach_(CreditAndProof& credit_and_proof, credit_and_proof_list)
            {
                top_up_work += credit_and_proof.first.difficulty;
            }
            
            if (credit_and_proof_list.size() > 0)
                top_up_work += calends[i].mined_credit.difficulty;
        }
        return top_up_work;
    }

    void Calendar::PopulateTopUpWork(uint160 credit_hash)
    {
        extra_work.resize(0);
        MinedCredit last_credit = LastMinedCredit();
        if (credit_hash != last_credit.GetHash160())
        {
            log_ << "Warning! Calendar's final credit hash is"
                 << last_credit.GetHash160() << " not "
                 << credit_hash << "\n";
        }
        uint160 total_credit_work = last_credit.total_work 
                                    + last_credit.difficulty;
        uint160 work_in_calends = CalendWork();

        uint160 total_calendar_work = work_in_calends + current_diurn.Work();
        
        if (Size() > 0)
            total_calendar_work -= calends.back().mined_credit.difficulty;

        for (int32_t i = calends.size() - 1; i >= 0; i--)
        {
            CreditAndProofList list;
            MinedCredit credit = calends[i].mined_credit;
            uint160 calend_work = credit.diurnal_difficulty;
            
            uint160 credit_work
                = i == 0
                  ? calends[i].TotalCreditWork()
                  : calends[i].TotalCreditWork()
                    - calends[i - 1].TotalCreditWork();

            uint160 previous_hash;
            while (credit_work > calend_work &&
                   total_credit_work > total_calendar_work &&
                   credit.previous_mined_credit_hash != 0)
            {
                previous_hash = credit.previous_mined_credit_hash;

                MinedCreditMessage msg = creditdata[previous_hash]["msg"];
                credit = msg.mined_credit;
                list.push_back(std::make_pair(credit, msg.proof));
                credit_work -= credit.difficulty;
                total_calendar_work += credit.difficulty;
            }
            extra_work.push_back(list);
        }
        std::reverse(extra_work.begin(), extra_work.end());
        foreach_(MinedCreditMessage msg, current_diurn.credits_in_diurn)
            StoreTopUpWorkIfNecessary(msg);
    }

    void Calendar::HandleCalend(MinedCreditMessage& msg)
    {
        log_ << "HandleCalend: " << msg;

        log_ << "mined credit is " << msg.mined_credit << "\n";

        uint160 credit_hash = msg.mined_credit.GetHash160();

        calendardata[credit_hash]["is_calend"] = true;
        log_ << "recorded fact that " << credit_hash << " is a calend\n";
        Calend calend(msg);

        calendardata[credit_hash]["calend"] = calend;
        creditdata[flexnode.spent_chain.GetHash160()]["chain"]
            = flexnode.spent_chain;

        uint160 diurn_root
            = SymmetricCombine(msg.mined_credit.diurnal_block_root,
                               msg.mined_credit.previous_diurn_root);

        calendardata[diurn_root]["credit_hash"] = credit_hash;

        log_ << "associated credit hash " << credit_hash 
             << " with diurn root " << diurn_root << "\n";

        uint160 difficulty = msg.proof.DifficultyAchieved();

        if (msg.mined_credit.previous_diurn_root
                == current_diurn.previous_diurn_root
            && difficulty >= current_diurn.current_difficulty)
        {
            FinishCurrentCalendAndStartNext(msg);
            extra_work.push_back(current_extra_work);
            current_extra_work.resize(0);
            TrimTopUpWork();
            log_ << "Calendar: total work: "
                 << TotalWork() << " vs credit work " 
                 << (LastMinedCredit().difficulty
                     + LastMinedCredit().total_work) << "\n";
        }
        else
        {
            log_ << "Not finishing diurn: "
                 << msg.mined_credit.previous_diurn_root << " != "
                 << current_diurn.previous_diurn_root
                 << " or " << difficulty << " < "
                 << current_diurn.current_difficulty << "\n";
        }
        log_ << "HandleCalend(): exiting: final calendar is" << ToString();
    }

    void Calendar::TrimTopUpWork()
    {
        uint160 calendar_work = TotalWork();
        uint160 credit_work = LastMinedCredit().total_work
                                + LastMinedCredit().difficulty;
        if (calendar_work <= credit_work)
            return;

        uint160 surplus = calendar_work - credit_work;

        uint160 work_trimmed(0);
        uint32_t num_credits_to_drop = 0;

        for (uint32_t i = 0; i < extra_work.size(); i++)
        {
            CreditAndProofList credit_and_proof_list = extra_work[i];
            for (uint32_t j = 0; j < credit_and_proof_list.size(); j++)
            {
                MinedCredit credit = credit_and_proof_list[j].first;
                if (work_trimmed + credit.difficulty > surplus)
                    return DropTopUpWorkCredits(num_credits_to_drop);
                work_trimmed += credit.difficulty;
                num_credits_to_drop += 1;
            }
        }
    }

    void Calendar::DropTopUpWorkCredits(uint32_t num_credits_to_drop)
    {
        uint32_t num_credits_dropped = 0;
        for (uint32_t i = 0; i < extra_work.size(); i++)
        {
            if (num_credits_dropped + extra_work[i].size()
                <= num_credits_to_drop)
            {
                num_credits_dropped += extra_work[i].size();
                extra_work[i].resize(0);
            }
            else
            {
                int32_t to_drop = num_credits_to_drop
                                    - num_credits_dropped
                                    - extra_work[i].size();
                if (to_drop <= 0)
                    return;
                num_credits_dropped += to_drop;
                extra_work[i].resize(extra_work[i].size() - to_drop);
            }
        }
    }

    void Calendar::PopulateDiurn(uint160 credit_hash)
    {
        log_ << "populating diurn: " << credit_hash << "\n";
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];

        current_diurn.current_difficulty = mined_credit.diurnal_difficulty;

        if (msg.proof.DifficultyAchieved() > mined_credit.diurnal_difficulty)
        {
            current_diurn.Add(credit_hash);
            current_diurn.previous_diurn_root
                = SymmetricCombine(mined_credit.previous_diurn_root,
                                     mined_credit.diurnal_block_root);
            log_ << "just this credit_hash in diurn\n";
            return;
        }

        std::vector<uint160> credit_hashes;
        credit_hashes.push_back(credit_hash);   

        uint160 diurn_root = 0;

        current_diurn.previous_diurn_root = mined_credit.previous_diurn_root;

        do
        {
            credit_hash = PreviousCreditHash(credit_hash);
            if (!creditdata[credit_hash].HasProperty("mined_credit"))
                break;
            mined_credit = creditdata[credit_hash]["mined_credit"];
            log_ << "previous credit was " << mined_credit;
            diurn_root = SymmetricCombine(mined_credit.previous_diurn_root,
                                          mined_credit.diurnal_block_root);
            credit_hashes.push_back(credit_hash);
            log_ << "preceding diurn root was " << diurn_root << "\n"
                 << "waiting for " 
                 << current_diurn.previous_diurn_root << "\n";
        }
        while (diurn_root != current_diurn.previous_diurn_root &&
               mined_credit.previous_mined_credit_hash != 0);

        std::reverse(credit_hashes.begin(), credit_hashes.end());
        foreach_(const uint160& hash, credit_hashes)
            current_diurn.Add(hash);

        log_ << "finished populating diurn\n";
    }

    uint160 Calendar::TotalWork() const
    {
        uint160 total_work = CalendWork();
        total_work += TopUpWork();
        total_work += current_diurn.Work();
        
        if (Size() > 0)
        {
            total_work -= calends.back().mined_credit.difficulty;
        }
        
        return total_work;
    }

    uint160 Calendar::PreviousDiurnRoot() const
    {
        if (current_diurn.Size() > 1)
        {
            MinedCredit mined_credit
                = creditdata[current_diurn.Last()]["mined_credit"];
            return mined_credit.previous_diurn_root;
        }
        if (current_diurn.Size() == 1 && calends.size() > 0)
        {
            MinedCredit mined_credit
                = creditdata[current_diurn.First()]["mined_credit"];
            return SymmetricCombine(mined_credit.previous_diurn_root,
                                    mined_credit.diurnal_block_root);
        }
        return 0;
    }

    uint160 Calendar::LastMinedCreditHash() const
    {
        if (current_diurn.Size() > 0)
            return current_diurn.Last();
        if (calends.size() == 0)
            return 0;
        Calend last_calend = calends.back();
        return last_calend.mined_credit.GetHash160();
    }

    MinedCredit Calendar::LastMinedCredit()
    {
        uint160 credit_hash = LastMinedCreditHash();
        MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
        return mined_credit;
    }
    
/*
 *  Calendar
 **************/

