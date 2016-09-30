#include "teleportnode/orphanage.h"
#include "database/data.h"
#include "credits/creditutil.h"
#include "database/memdb.h"
#include <iostream>
#include <stdio.h>


using namespace std;

CMemoryDB tx_db;
CMemoryDB keydb;

CMemoryDB txtypedb;

string strTCR("TCR");
vch_t TCR(strTCR.begin(), strTCR.end());

BitChain spent_chain;

std::vector<CNode*> vNodes;

class TestMessage
{
public:
    uint160 hash;
    vch_t text;
    vch_t signature;

    TestMessage()
    { 
        string_t string_text("This should come");
        text = vch_t(string_text.begin(), string_text.end());
        hash = 64;
    }

    static string_t Type() { return "test"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(text);
    )

    DEPENDENCIES();

    Point VerificationKey() const
    {
        return Point(SECP256K1, 100);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class TestMessage2
{
public:
    uint160 hash;
    vch_t text;
    vch_t signature;
    TestMessage earlier_message;

    TestMessage2() { }

    TestMessage2(TestMessage earlier_message):
        earlier_message(earlier_message)
    { 
        string_t string_text("before this");
        text = vch_t(string_text.begin(), string_text.end());
        hash = earlier_message.GetHash160();
    }

    static string_t Type() { return "test2"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(text);
        READWRITE(hash);
    )

    DEPENDENCIES(hash);

    Point VerificationKey() const
    {
        return Point(SECP256K1, 100);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


template <typename T>
bool HandleTestMessage(T msg)
{
    string_t text(msg.text.begin(), msg.text.end());
    cout << "---> " << text << endl;
    return text.size() > 10;
}


class TestHandler : public MessageHandlerWithOrphanage
{
public:
    const char* channel;

    TestHandler()
    {
        TestHandler::channel = "test_channel";
    }

    HandleClass_(TestMessage);
    HandleClass_(TestMessage2);
    
    void HandleMessage(CDataStream ss, CNode *peer)
    {
        string message_type;
        ss >> message_type;
        ss >> message_type;

        handle_stream_(TestMessage, ss, peer);
        handle_stream_(TestMessage2, ss, peer);
    }

    void HandleMessage(uint160 message_hash)
    {
        string_t message_type = msgdata[message_hash]["type"];
        
        handle_hash_(TestMessage, message_hash);
        handle_hash_(TestMessage2, message_hash);
    }

    void HandleTestMessage(TestMessage msg)
    {
        ::HandleTestMessage(msg);
    }

    void HandleTestMessage2(TestMessage2 msg)
    {
        ::HandleTestMessage(msg);
    }
};


bool TestMessageHandlerWithOrphanage()
{
    TestHandler test_handler;

    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    TestMessage test_message;
    string_t type("test"), type2("test2");

    vch_t vchType(type.begin(), type.end());
    vch_t vchType2(type2.begin(), type2.end());

    uint160 message_hash = test_message.GetHash160();

    //test_handler.HandleMessage(message_hash);

    TestMessage2 test_message2(test_message);
    

    ss << vchType2 << vchType2 << test_message2;
    test_handler.HandleMessage(ss, NULL);
    
    ss = CDataStream(SER_NETWORK, CLIENT_VERSION);
    ss << vchType << vchType << test_message;
    test_handler.HandleMessage(ss, NULL);
    return true;
}


int main()
{
    printf("passed test: %d\n", TestMessageHandlerWithOrphanage());
    return 0;
}