#include <src/crypto/secp256k1point.h>
#include <src/crypto/bignum_hashes.h>
#include "RelayPublicKeySet.h"
#include "Relay.h"

std::vector<Point> RelayPublicKeySet::PublicKeySixteenths()
{
    std::vector<Point> public_key_sixteenths;
    for (auto& row : key_points)
        public_key_sixteenths.push_back(row[0]);
    return public_key_sixteenths;
}

void RelayPublicKeySet::Populate(MemoryDataStore &keydata)
{
    PopulateFirstElementOfEachRow(keydata);
    PopulateEachRowUsingTheSecretCorrespondingToTheFirstElement(keydata);
}

void RelayPublicKeySet::PopulateFirstElementOfEachRow(MemoryDataStore &keydata)
{
    for (uint64_t i = 0; i < 16; i++)
    {
        CBigNum secret;
        secret.Randomize(Secp256k1Point::Modulus());
        Point public_key_part(secret);
        keydata[public_key_part]["privkey"] = secret;
        key_points.push_back(std::vector<Point>{public_key_part});
    }
}

void RelayPublicKeySet::PopulateEachRowUsingTheSecretCorrespondingToTheFirstElement(MemoryDataStore &keydata)
{
    for (auto &row : key_points)
        row = GenerateRowOfPointsAndStoreCorrespondingSecretParts(row[0], keydata);
}

std::vector<Point> RelayPublicKeySet::GenerateRowOfPointsAndStoreCorrespondingSecretParts(Point point,
                                                                                          MemoryDataStore &keydata)
{
    std::vector<Point> row{point};
    CBigNum secret_first_part = keydata[point]["privkey"];
    for (uint32_t i = 1; i <= 3; i++)
    {
        CBigNum next_secret_part = Hash(secret_first_part + i);
        Point next_public_part(next_secret_part);
        keydata[next_public_part]["privkey"] = next_secret_part;

        row.push_back(next_public_part);
    }
    return row;
}

bool RelayPublicKeySet::VerifyGeneratedPoints(MemoryDataStore &keydata)
{
    if (key_points.size() != 16)
        return false;

    for (uint64_t row_position = 0; row_position < 16; row_position++)
        if (not VerifyRowOfGeneratedPoints(row_position, keydata))
            return false;

    return true;
}

bool RelayPublicKeySet::VerifyRowOfGeneratedPoints(uint64_t row_position, MemoryDataStore &keydata)
{
    auto &row = key_points[row_position];
    if (keydata[row[0]].HasProperty("privkey"))
    {
        auto generated_row = GenerateRowOfPointsAndStoreCorrespondingSecretParts(row[0], keydata);
        if (row != generated_row)
            return false;
    }
    return true;
}

CBigNum RelayPublicKeySet::GenerateReceivingPrivateKey(Point point_corresponding_to_secret, MemoryDataStore &keydata)
{
    CBigNum seed = Hash(point_corresponding_to_secret);
    CBigNum receiving_key = 0;

    for (auto &row : key_points)
        for (auto &point : row)
        {
            CBigNum secret = keydata[point]["privkey"];
            seed = Hash(seed);
            receiving_key += (seed % 65536) * secret;
        }
    return receiving_key % Secp256k1Point::Modulus();
}

CBigNum RelayPublicKeySet::GenerateReceivingPrivateKeyQuarter(Point point_corresponding_to_secret,
                                                              uint8_t key_quarter_position, MemoryDataStore &keydata)
{
    CBigNum seed = Hash(point_corresponding_to_secret);
    CBigNum receiving_key = 0;

    for (uint32_t key_sixteenth_position = 0; key_sixteenth_position < key_points.size(); key_sixteenth_position++)
        for (auto &point : key_points[key_sixteenth_position])
        {
            seed = Hash(seed);
            if (key_sixteenth_position / 4 == key_quarter_position)
            {
                CBigNum secret = keydata[point]["privkey"];
                receiving_key += (seed % 65536) * secret;
            }
        }
    return receiving_key % Secp256k1Point::Modulus();
}

Point RelayPublicKeySet::GenerateReceivingPublicKey(Point point_corresponding_to_secret)
{
    CBigNum seed = Hash(point_corresponding_to_secret);
    Point receiving_key(0);

    for (auto &row : key_points)
        for (auto &point : row)
        {
            seed = Hash(seed);
            receiving_key += (seed % 65536) * point;
        }
    return receiving_key;
}

Point RelayPublicKeySet::GenerateReceivingPublicKeyQuarter(Point point_corresponding_to_secret,
                                                           uint8_t key_quarter_position)
{
    CBigNum seed = Hash(point_corresponding_to_secret);
    Point receiving_key(0);

    for (uint32_t key_sixteenth_position = 0; key_sixteenth_position < key_points.size(); key_sixteenth_position++)
        for (auto &point : key_points[key_sixteenth_position])
        {
            seed = Hash(seed);
            if (key_sixteenth_position / 4 == key_quarter_position)
                receiving_key += (seed % 65536) * point;
        }
    return receiving_key;
}

CBigNum RelayPublicKeySet::Encrypt(CBigNum secret)
{
    Point receiving_key = GenerateReceivingPublicKey(Point(secret));
    CBigNum shared_secret = Hash(secret * receiving_key);
    return secret ^ shared_secret;
}

CBigNum RelayPublicKeySet::Decrypt(CBigNum encrypted_secret, MemoryDataStore &keydata,
                                   Point point_corresponding_to_secret)
{
    CBigNum receiving_key = GenerateReceivingPrivateKey(point_corresponding_to_secret, keydata);
    CBigNum shared_secret = Hash(receiving_key * point_corresponding_to_secret);
    CBigNum decrypted_secret = encrypted_secret ^ shared_secret;

    if (Point(decrypted_secret) != point_corresponding_to_secret)
        return 0;

    return decrypted_secret;
}

bool RelayPublicKeySet::ValidateSizes()
{
    if (key_points.size() != 16)
        return false;
    for (auto &row : key_points)
        if (row.size() != 4)
            return false;
    return true;
}
