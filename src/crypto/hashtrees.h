#ifndef FLEX_HASHTREES
#define FLEX_HASHTREES 1

#include "crypto/hash.h"
#include <string.h>
#include <algorithm>
#include "base/util.h"
#include <boost/foreach.hpp>
#include <stdexcept>

#include "log.h"
#define LOG_CATEGORY "hashtrees.h"


typedef std::vector<unsigned char> vch_t;

class HashTreeException : public std::runtime_error
{
public:
   explicit HashTreeException(const std::string& msg)
      : std::runtime_error(msg)
   {}
};

inline uint160 TreeHash(const void *input, size_t input_len)
{
    return Hash160((uint8_t*)input, (uint8_t*)input + input_len);
}

inline uint160 AsymmetricCombine(uint160 hash1, uint160 hash2)
{
    uint8_t input[40];
    uint160 *hash1_ = (uint160*)&input[0];    
    uint160 *hash2_ = (uint160*)&input[20];
    *hash1_ = hash1;
    *hash2_ = hash2;
    return TreeHash(input, 40);
}

inline uint160 Combine(uint160 hash1, uint160 hash2)
{
    return AsymmetricCombine(hash1, hash2);
}

inline uint160 SymmetricCombine(uint160 hash1, uint160 hash2)
{
    uint8_t input[40];
    uint160 *hash1xorhash2 = (uint160*)&input[0];    
    uint160 *hhash1orhhash2 = (uint160*)&input[20];

    uint160 hhash1 = TreeHash(&hash1, 20);
    uint160 hhash2 = TreeHash(&hash2, 20);

    *hash1xorhash2 = hash1 ^ hash2;
    *hhash1orhhash2 = hhash1 | hhash2;

    return TreeHash(input, 40);
}

/************
 * HashNode
 */

static uint32_t numnodes;

template<typename T>
class HashNode;

template<typename T>
class HashNode
{
public:
    T **elements;
    
    uint32_t start, middle, end;
    uint32_t element_size;
    bool symmetric;
    uint64_t total_length;
    uint160 hash;
    bool up_to_date;

    uint32_t number;
    
    HashNode<T> *child1, *child2;
    
    HashNode(T **elementsIn, uint32_t startIn, uint32_t endIn,
             uint32_t element_size, bool symmetricIn);

    ~HashNode();
    
    void Set(uint64_t position);

    void Clear(uint64_t position);
    
    uint160 Hash(uint64_t length);

    std::vector<uint160> Branch(uint64_t position);

    bool AddElement();

    uint160 CombineHash(uint160 hash1, uint160 hash2);
};


template<typename T>
bool HashNode<T>::AddElement()
{
    if (middle)
    {
        bool added = child2->AddElement();
        if (added)
        {
            end += 1;
        }
        return added;
    }

    if (end == start + 1)
    {
        end += 1;
        up_to_date = false;
        return true;
    }

    total_length += 1;
    return false;
}

template<typename T>
HashNode<T>::HashNode(T **elementsIn, uint32_t startIn, uint32_t endIn,
                      uint32_t element_sizeIn, bool symmetricIn)
: elements(elementsIn), start(startIn),  middle(0), end(endIn),
  element_size(element_sizeIn), symmetric(symmetricIn)
{
    total_length = 0;
    number = numnodes++;
    
    up_to_date = false;
    if (end > start + 2)
    {
        middle = (end + start) / 2;
        child1 = new HashNode<T>(elements, start, middle, 
                                 element_size, symmetric);
        child2 = new HashNode<T>(elements, middle, end, 
                                 element_size, symmetric);
    }
}

template<typename T>
HashNode<T>::~HashNode()
{
    if (middle)
    {
        delete child1;
        delete child2;
    }
}

template<typename T>
void HashNode<T>::Set(uint64_t position)
{
    up_to_date = false;
    if (middle)
        if (position < middle * element_size)
        {
            child1->Set(position);
        }
        else
        {
            child2->Set(position);
        }
    else
        if (position / element_size >= start && position / element_size <= end)
        {
            elements[position / element_size]->Set(position);
        }
        else
            throw HashTreeException("node " + to_string(number) + " "
                                    + "inappropriately asked to set position "
                                    + to_string(position));
}

template<typename T>
void HashNode<T>::Clear(uint64_t position)
{
    up_to_date = false;
    if (middle)
        if (position < middle * element_size)
        {
            child1->Clear(position);
        }
        else
        {
            child2->Clear(position);
        }
    else
        if (position / element_size >= start && position / element_size <= end)
        {
            elements[position / element_size]->Clear(position);
        }
        else
            throw HashTreeException("node " + to_string(number) + " "
                                    + "inappropriately asked to clear "
                                    + "position " + to_string(position));
}

template<typename T>
uint160 HashNode<T>::CombineHash(uint160 hash1, uint160 hash2)
{
    if (symmetric)
    {
        return SymmetricCombine(hash1, hash2);
    }
    else
    {
        return AsymmetricCombine(hash1, hash2);
    }
}

template<typename T>
uint160 HashNode<T>::Hash(uint64_t length)
{
    if (length == 0)
        return 0;
    if (up_to_date && length == total_length)
    {
        return hash;
    }
    total_length = length;
    if (middle)
    {
        if (length >= middle * element_size)
        {
            uint160 child_hash_1 = child1->Hash(length);
            uint160 child_hash_2 = child2->Hash(length);
            hash = CombineHash(child_hash_1, child_hash_2);   
        }
        else
        {
            hash = child1->Hash(length);
        }
    }
    else
    {
        if (end == start + 1 || length < (start + 1) * element_size)
        {
            hash = elements[start]->Hash();
        }
        else
        {
            uint160 start_hash = elements[start]->Hash();
            uint160 end_hash = elements[end - 1]->Hash();
            hash = CombineHash(start_hash, end_hash);            
        }
    }

    up_to_date = true;
    return hash;
}

template<typename T>
std::vector<uint160> HashNode<T>::Branch(uint64_t position)
{
    std::vector<uint160> branch;
    if (middle)
    {
        if (position >= middle * element_size)
        {
            branch = child2->Branch(position);
            branch.push_back(child1->Hash(total_length));
        }
        else
        {
            branch = child1->Branch(position);
            branch.push_back(child2->Hash(total_length));
        }
    }
    else
    {
        if (end == start + 1)
            return branch;
        if (position / element_size != start)
        {
            branch.push_back(elements[start]->Hash());
        }
        else
        {
            branch.push_back(elements[end - 1]->Hash());
        }
    }
    return branch;
}

/*
 * HashNode
 ************/



/************
 * HashTree
 */

static uint32_t numhashtrees;

template<typename T>
class HashTree
{
public:
    std::vector<T*> elements;
    HashNode<T> *root;
    uint32_t number;
    uint32_t element_size;

    HashTree(std::vector<T*> elementsIn, 
             uint32_t element_sizeIn, 
             bool symmetric)
    : elements(elementsIn), element_size(element_sizeIn)
    {
        number = numhashtrees++;
        root = new HashNode<T>(&elements[0], 0, 
                               elements.size(), element_size, symmetric);
    }

    ~HashTree()
    {
        if (root != NULL)
            delete root;
    }

    bool AddElement(T& element)
    {
        elements.push_back(&element);
        return root->AddElement();
    }

    void Set(uint64_t position)
    {
        root->Set(position);
    }

    void Clear(uint64_t position)
    {
        root->Clear(position);
    }

    uint160 Hash(uint64_t length)
    {
        uint160 result = root->Hash(length);
        return result;      
        
    }

    std::vector<uint160> Branch(uint64_t position)
    {
        if (root->total_length == 0)
        {
            Hash(elements.size());
        }
        std::vector<uint160> branch = root->Branch(position);
        return branch;
    }
};


/*
 * HashTree
 ************/


/****************
 * DiurnalBlock
 */

class uint160Datum
{
public:
    uint160 hash;

    uint160Datum(uint160 hash): hash(hash) { }

    uint160Datum(vch_t data)
    {
        setvch(data);
    }
    
    uint160 Hash()
    {
        return hash;
    }

    vch_t getvch()
    {
        vch_t data;
        data.resize(20);
        memcpy(&data[0], UBEGIN(hash), 20);
        return data;
    }

    void setvch(vch_t data)
    {
        memcpy(UBEGIN(hash), &data[0], 20);
    }

    void Set(uint64_t dummy) { }

    void Clear(uint64_t dummy) { }
};


class DiurnalBlock
{
public:
    std::vector<uint160Datum*> hashes;
    HashTree<uint160Datum> *tree{NULL};

    DiurnalBlock(): tree(NULL) { }

    DiurnalBlock(const DiurnalBlock& other_block)
    {
        vch_t bytes = other_block.getvch();
        setvch(bytes);
    }

    ~DiurnalBlock()
    {
        if (tree)
            delete tree;
        for (uint32_t i = 0; i < hashes.size(); i++)
        {
            delete hashes[i];
        }
    }

    std::vector<uint160> Hashes() const
    {
        std::vector<uint160> hashes_;
        
        foreach_(const uint160Datum* datum, hashes)
        {
            uint160 hash = datum->hash;
            hashes_.push_back(hash);
        }
        return hashes_;
    }

    vch_t getvch() const
    {
        vch_t bytes;
        foreach_(const uint160Datum* datum, hashes)
        {
            vch_t hash_bytes;
            hash_bytes.resize(20);
            uint160 hash = datum->hash;
            
            memcpy(&hash_bytes[0], UBEGIN(hash), 20);
            bytes.insert(bytes.end(), hash_bytes.begin(), hash_bytes.end());
        }
        return bytes;
    }

    void setvch(vch_t bytes)
    {
        while (hashes.size() > 0)
            RemoveLast();
        for (uint32_t i = 0; i < bytes.size(); i += 20)
        {
            uint160 hash;
            memcpy(UBEGIN(hash), &bytes[i], 20);
            Add(hash);
        }
        Hashes();
        getvch();
    }

    void Add(uint160 hash)
    {
        if (tree)
            delete tree;
        uint160Datum *datum = new uint160Datum(hash);
        hashes.push_back(datum);
        tree = new HashTree<uint160Datum>(hashes, 1, true);
    }

    uint64_t Size() const
    {
        return hashes.size();
    }

    void RemoveLast()
    {
        delete hashes[hashes.size() - 1];
        delete tree;
        hashes.resize(hashes.size() - 1);
        tree = new HashTree<uint160Datum>(hashes, 1, true);
    }

    int32_t Position(uint160 hash)
    {
        for (uint32_t i = 0; i < hashes.size(); i++)
            if (hashes[i]->hash == hash)
                return i;
        return -1;
    }

    bool Contains(uint160 hash)
    {
        return Position(hash) != -1;
    }

    uint160 Last() const
    {
        return hashes[hashes.size() - 1]->hash;
    }

    uint160 First() const
    {
        return hashes[0]->hash;
    }

    uint160 Root()
    {
        if (tree == NULL)
            return 0;
        return tree->Hash(hashes.size());
    }

    std::vector<uint160> Branch(uint160 hash)
    {
        return tree->Branch(Position(hash));
    }

    unsigned int GetSerializeSize(int nType=0, int nVersion=PROTOCOL_VERSION) 
    const
    {
        return ::GetSerializeSize(getvch(), nType, nVersion);
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType=0, int nVersion=PROTOCOL_VERSION) 
    const
    {
        ::Serialize(s, getvch(), nType, nVersion);
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType=0, int nVersion=PROTOCOL_VERSION)
    {
        std::vector<unsigned char> vch;
        ::Unserialize(s, vch, nType, nVersion);
        setvch(vch);
    }
};

/*
 * DiurnalBlock
 ****************/


class BitChain
{
public:
    uint64_t length;
    vch_t data;

    BitChain(): 
        length(0)
    { }

    void Add()
    {
        length += 1;
        if (length > 8 * data.size())
            data.push_back(0);
    }

    void Set(uint64_t position)
    {
        if (position >= length)
            return;
        data[position / 8] |= (1 << (position % 8));
    }

    void Clear(uint64_t position)
    {
        if (position >= length)
            return;
        data[position / 8] &= ~(1 << (position % 8));
    }

    bool Get(uint64_t position)
    {
        if (position >= length)
            return false;
        return data[position / 8] & (1 << (position % 8));
    }

    bool operator==(const BitChain& other) const
    {
        return length == other.length and data == other.data;
    }

    uint160 GetHash160()
    {
        if (length == 0)
            return 0;
        uint160 hash = TreeHash(&data[0], (int)ceil(length / 8.));
        return hash;
    }

    void SetLength(uint64_t new_length)
    {
        length = new_length;
        uint64_t new_size = (int)ceil(new_length / 8.);
        
        data.resize(new_size);
        
        if (length % 8 != 0)
        {
            uint32_t n = length % 8;
            uint8_t mask = (1 << n) - 1;
            data[data.size() - 1] &= mask;
        }
    }

    uint160 HashDifferent(std::vector<uint64_t> to_set, 
                          std::vector<uint64_t> to_clear,
                          uint64_t length_);

    uint160 HashDifferent(std::set<uint64_t> to_set,
                          std::set<uint64_t> to_clear,
                          uint64_t length_);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(length);
        READWRITE(data);
    )

    JSON(length, data);

    std::string ToString()
    {
        std::string result("");
        for (uint32_t i = 0; i < length; i++)
            result += to_string((int)Get(i));
        return result;
    }
};


inline uint160 BitChain::HashDifferent(std::vector<uint64_t> to_set, 
                                       std::vector<uint64_t> to_clear,
                                       uint64_t length_)
{
    BitChain other = *this;

    other.SetLength(length_);

    for (uint32_t i = 0; i < to_set.size(); i++)
        other.Set(to_set[i]);
    for (uint32_t i = 0; i < to_clear.size(); i++)
        other.Clear(to_clear[i]);
    
    uint160 hash = other.GetHash160();
    return hash;
}

inline uint160 BitChain::HashDifferent(std::set<uint64_t> to_set,
                                       std::set<uint64_t> to_clear,
                                       uint64_t length_)
{
    BitChain other = *this;

    other.SetLength(length_);

    for (auto position : to_set)
        other.Set(position);
    for (auto position : to_clear)
        other.Clear(position);

    uint160 hash = other.GetHash160();
    return hash;
}

static uint32_t numelements;

class OrderedDataElement
{
public:
    uint64_t position;
    vch_t data;
    uint32_t number;

    OrderedDataElement(vch_t input): position(0)
    {
        number = numelements++;
        data = input;
    }

    OrderedDataElement(uint64_t positionIn, vch_t input)
    {
        number = numelements++;
        position = positionIn;
        data = vch_t(input);
    }

    ~OrderedDataElement()
    {
        
    }

    uint160 DataHash()
    {
        return TreeHash(&data[0], data.size());
    }

    uint160 Hash()
    {
        vch_t position_plus_data;
        position_plus_data.resize(4 + data.size());
        memcpy(&position_plus_data[0], &position, 4);
        memcpy(&position_plus_data[0], &data[0], data.size());
        uint160 hash = TreeHash(&position_plus_data[0], position_plus_data.size());
        
        return hash;
    }

    void Set(uint64_t position) { }
    void Clear(uint64_t position) { }
};


class OrderedHashTree
{
public:
    std::vector<OrderedDataElement*> elements;
    HashTree<OrderedDataElement> *tree;
    bool positions_assigned;
    uint64_t offset;

    OrderedHashTree(): tree(NULL), positions_assigned(false), offset(0ULL)
    {
    }

    OrderedHashTree(uint64_t offsetIn)
    : tree(NULL), positions_assigned(false), offset(offsetIn)
    { 
     
    }

    ~OrderedHashTree()
    {
        if (tree)
            delete tree;
        for (uint32_t i = 0; i < elements.size(); i++)
            delete elements[i];
    }

    void AddElement(vch_t data)
    {
        AddOrderedDataElement(new OrderedDataElement(data));
    }

    vch_t GetData(uint64_t position)
    {
        return (*elements[position - offset]).data;
    }

    void AddOrderedDataElement(OrderedDataElement *element)
    {
        elements.push_back(element);
        positions_assigned = false;
    }

    void BuildTree()
    {
        if (tree)
            delete tree;
        AssignPositions();
        tree = new HashTree<OrderedDataElement>(elements, 1, true);
    }

    void AssignPositions()
    {
        std::vector<uint160> these_hashes;
        std::vector<uint160> sorted_hashes;
        for (uint32_t i = 0; i < elements.size(); i++)
        {
            elements[i]->position = 0;
            these_hashes.push_back(elements[i]->DataHash());
        }

        sorted_hashes = these_hashes;

        sort(sorted_hashes.begin(), sorted_hashes.end());

        for (uint32_t i = 0; i < elements.size(); i++)
            {
            elements[i]->position = offset +
                distance(sorted_hashes.begin(),
                    find(sorted_hashes.begin(), sorted_hashes.end(),
                                                these_hashes[i]));
            }
        std::vector<OrderedDataElement*> sorted_elements;
        sorted_elements.resize(elements.size());
        for (uint32_t i = 0; i < elements.size(); i++)
        {
            sorted_elements[elements[i]->position - offset] = elements[i];
        }
        elements = sorted_elements;
        positions_assigned = true;
    }

    int32_t Position(vch_t data)
    {
        if (!positions_assigned)
            AssignPositions();
        for (uint32_t i = 0; i < elements.size(); i++)
            if (data == elements[i]->data)
                return elements[i]->position;
        return -1;
    }

    std::vector<uint160> Branch(uint32_t position)
    {
        if (!tree || !positions_assigned)
            BuildTree();

        return tree->Branch(position - offset);
    }

    uint160 Root()
    {
        if (!tree || !positions_assigned)
            BuildTree();
        return tree->Hash(elements.size());
    }
};

inline uint160 EvaluateBranchWithHash(std::vector<uint160> branch,
                                      uint160 hash,
                                      uint32_t start_position = 0)
{
    for (uint32_t k = start_position; k + 1 < branch.size(); k++)
        hash = SymmetricCombine(hash, branch[k]);
    return hash;
}

inline bool VerifyBranchFromOrderedHashTree(uint32_t position,
                                            vch_t element,
                                            std::vector<uint160> branch,
                                            uint160 root)
{
    if (branch.size() == 0 or root != branch.back())
        return false;

    uint160 hash = OrderedDataElement(position, element).Hash();

    return EvaluateBranchWithHash(branch, hash) == root;
}


#endif
