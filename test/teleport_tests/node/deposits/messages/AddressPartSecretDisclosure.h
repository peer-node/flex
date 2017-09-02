#ifndef TELEPORT_ADDRESSPARTSECRETDISCLOSURE_H
#define TELEPORT_ADDRESSPARTSECRETDISCLOSURE_H

#include "AddressPartSecret.h"

class AddressPartSecretDisclosure
{
public:
    std::array<uint256, SECRETS_PER_ADDRESS_PART> secrets_encrypted_for_first_auditors;
    std::array<uint256, SECRETS_PER_ADDRESS_PART> secrets_encrypted_for_second_auditors;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(secrets_encrypted_for_first_auditors);
        READWRITE(secrets_encrypted_for_second_auditors);
    );
};


#endif //TELEPORT_ADDRESSPARTSECRETDISCLOSURE_H
