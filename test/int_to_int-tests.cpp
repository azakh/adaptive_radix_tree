#include "gtest/gtest.h"
#include "adaptive_radix_tree.h"
#include "base_fixture.h"
#include "simple_bench.h"

#include <vector>

#include <unordered_map>
#include "sparsehash/dense_hash_map"


// Generate keys of the specified length incrementing from last bits
template<size_t kKeyLength>
class IncrementLowBitsKeyGenerator
{
    uint8_t current_[kKeyLength];
public:
    IncrementLowBitsKeyGenerator()
    {
        memset(current_, 0, sizeof(current_));
    }

    void Next(uint8_t key[kKeyLength])
    {
        for (int i = kKeyLength - 1; i >= 0; --i)
        {
            if (current_[i] == 0xFF)
            {
                key[i] = current_[i] = 0;
                continue;
            }

            key[i] = ++current_[i];
            break;
        }
    }
};
// Generate keys of the specified length incrementing from first bits
template<size_t kKeyLength>
class IncrementHighBitsKeyGenerator
{
    uint8_t current_[kKeyLength];
public:
    IncrementHighBitsKeyGenerator()
    {
        memset(current_, 0, sizeof(current_));
    }

    void Next(uint8_t key[kKeyLength])
    {
        for (int i = 0; i < kKeyLength; ++i)
        {
            if (current_[i] == 0xFF)
            {
                key[i] = current_[i] = 0;
                continue;
            }

            key[i] = ++current_[i];
            break;
        }
    }
};

union Char4ToInt
{
    Char4ToInt() : intValue(0) {}
    Char4ToInt(const char chars[5])
    {
        for (int i = 0; i < 4; ++i)
            charValue[i] = static_cast<uint8_t>(chars[i]);
    }

    uint8_t charValue[4];
    int intValue;
};

struct IntToIntAdaptiveRadixTreeTest : AdaptiveRadixTreeTest<int, int>
{
    template<class TKeyGenerator>
    static void FillIntKeysWithGenerator(std::vector<int>& keys, size_t count)
    {
        keys.resize(count * 4);

        TKeyGenerator gen;
        Char4ToInt conv;
        for (size_t i = 0; i < keys.size(); ++i)
        {
            keys[i] = conv.intValue;
            gen.Next(conv.charValue);
        }

        srand(123);
        std::random_shuffle(keys.begin(), keys.end());
        keys.resize(count);
    }
};

TEST_F(IntToIntAdaptiveRadixTreeTest, Constructor_ConstructEmptyTree)
{
    EXPECT_EQ(0, tree_.size());
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_OnePair_CreatesOneNode)
{
    std::pair<tree::iterator, bool> result = tree_.insert(0, -1);
    EXPECT_EQ(1, tree_.size());
    EXPECT_EQ(true, result.second);
    EXPECT_EQ(-1, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_WithExistingKey_ReturnsExistingNode)
{
    tree_.insert(0, -1);
    std::pair<tree::iterator, bool> result = tree_.insert(0, -1);
    EXPECT_EQ(1, tree_.size());
    EXPECT_EQ(false, result.second);
    EXPECT_EQ(-1, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_TwoPairs_SplitsRootNode)
{
    tree_.insert(0, -1);

    Char4ToInt secondKey("\0\x0f\0\0");
    std::pair<tree::iterator, bool> result = tree_.insert(secondKey.intValue, 0);
    EXPECT_EQ(2, tree_.size());
    EXPECT_EQ(true, result.second);
    EXPECT_EQ(0, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_ThreePairs_AddsNodeToRoot)
{
    tree_.insert(0, 0);

    Char4ToInt secondKey("\0\x0f\0\0");
    tree_.insert(secondKey.intValue, 1);

    Char4ToInt thirdKey("\0\x01\0\0");
    std::pair<tree::iterator, bool> result = tree_.insert(thirdKey.intValue, 3);
    EXPECT_EQ(3, tree_.size());
    EXPECT_EQ(true, result.second);
    EXPECT_EQ(3, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_ChildGrowsFrom4to256)
{
    std::vector<int> keys;
    // Here we test tree in configuration root-0/1-n1-n2, where nN nodes can grow.
    // This is to test low-level nodes promotion when prefix path is not changing much.
    FillIntKeysWithGenerator<IncrementLowBitsKeyGenerator<4> >(keys, 4 * 1024 * 1024);

    TIME_AUTO("insert")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            tree_.insert(keys[i], i);
            EXPECT_EQ(i + 1, tree_.size());
        }
    }
    FlushCache();
    TIME_AUTO("find")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            tree::iterator findIt = tree_.find(keys[i]);
            //EXPECT_TRUE(findIt != tree_.end());
            EXPECT_EQ(i, findIt.second);
        }
    }


    std::unordered_map<int, int> unordered_map_tree;
    TIME_AUTO("unordered_map_insert")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            unordered_map_tree.insert(std::make_pair(keys[i], i));
            EXPECT_EQ(i + 1, unordered_map_tree.size());
        }
    }
    FlushCache();
    TIME_AUTO("unordered_map_find")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            std::unordered_map<int, int>::iterator findIt = unordered_map_tree.find(keys[i]);
            //EXPECT_TRUE(findIt != unordered_map_tree.end());
            EXPECT_EQ(i, findIt->second);
        }
    }


    google::dense_hash_map<int, int> dense_map_tree;
    dense_map_tree.set_empty_key(0xFFFFFFFF);
    TIME_AUTO("dense_map_insert")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            dense_map_tree.insert(std::make_pair(keys[i], i));
            EXPECT_EQ(i + 1, dense_map_tree.size());
        }
    }
    FlushCache();
    TIME_AUTO("dense_map_find")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            google::dense_hash_map<int, int>::iterator findIt = dense_map_tree.find(keys[i]);
            //EXPECT_TRUE(findIt != dense_map_tree.end());
            EXPECT_EQ(i, findIt->second);
        }
    }
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_ChildGrowsFrom4to256_ForwardKey)
{
    std::vector<int> keys;
    // Here we test tree in configuration root-n1-n2-0, where nN nodes can grow 
    // This is to test top-level nodes promotion.
    FillIntKeysWithGenerator<IncrementHighBitsKeyGenerator<4> >(keys, 4 * 1024 * 1024);

    TIME_AUTO("insert")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            tree_.insert(keys[i], i);
            EXPECT_EQ(i + 1, tree_.size());
        }
    }
    FlushCache();
    TIME_AUTO("find")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            tree::iterator findIt = tree_.find(keys[i]);
            EXPECT_TRUE(findIt != tree_.end());
            EXPECT_EQ(i, findIt.second);
        }
    }

    std::unordered_map<int, int> unordered_map_tree;
    TIME_AUTO("unordered_map_insert")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            unordered_map_tree.insert(std::make_pair(keys[i], i));
            EXPECT_EQ(i + 1, unordered_map_tree.size());
        }
    }
    FlushCache();
    TIME_AUTO("unordered_map_find")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            std::unordered_map<int, int>::iterator findIt = unordered_map_tree.find(keys[i]);
            //EXPECT_TRUE(findIt != unordered_map_tree.end());
            EXPECT_EQ(i, findIt->second);
        }
    }

    google::dense_hash_map<int, int> dense_map_tree;
    dense_map_tree.set_empty_key(0xFFFFFFFF);
    TIME_AUTO("dense_map_insert")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            dense_map_tree.insert(std::make_pair(keys[i], i));
            EXPECT_EQ(i + 1, dense_map_tree.size());
        }
    }
    FlushCache();
    TIME_AUTO("dense_map_find")
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            google::dense_hash_map<int, int>::iterator findIt = dense_map_tree.find(keys[i]);
            //EXPECT_TRUE(findIt != dense_map_tree.end());
            EXPECT_EQ(i, findIt->second);
        }
    }
}

TEST_F(IntToIntAdaptiveRadixTreeTest, find)
{
    tree_.insert(0, -1);
    EXPECT_EQ(1, tree_.size());

    tree::iterator it = tree_.find(0);
    EXPECT_TRUE(it != tree_.end());
    EXPECT_EQ(-1, it.second);
}
