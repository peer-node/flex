

class FlexNode
{
public:
    DataServer dataserver;
    TradeHandler tradehandler;
    Downloader downloader;
    uint160 previous_mined_credit_hash;
    RelayHandler relayhandler;
    BitChain spent_chain;
    BatchChainer chainer;
    Calendar calendar;
    Scheduler scheduler;
    CreditPit pit;
    Wallet wallet;
    uint32_t num_credits_mined;
    uint32_t num_credits_to_mine;

    FlexNode() {}

    void Initialize()
    {
        InitializeCryptoCurrencies();
        InitializeRelayTasks();
        InitializeTradeTasks();
        downloader.Initialize();
        vch_t f;
        f.push_back((uint8_t)6);
        wallet = Wallet(f);
        pit.Initialize();
    }

    void HandleTransaction(SignedTransaction tx, CNode *peer)
    {
        chainer.HandleTransaction(tx, peer);
    }

    void HandleMinedCreditMessage(MinedCreditMessage &msg, CNode *peer)
    {
        chainer.HandleMinedCreditMessage(msg, peer);
        num_credits_mined += 1;
        if (num_credits_mined == num_credits_to_mine)
            pit.StopMining();
    }

    void HandleBadBlockMessage(BadBlockMessage &msg, CNode *peer)
    {
        chainer.HandleBadBlockMessage(msg, peer);
    }

    void MineCredits(uint32_t num_credits)
    {
        num_credits_mined = 0;
        num_credits_to_mine = num_credits;
        pit.StartMining();
    }
};