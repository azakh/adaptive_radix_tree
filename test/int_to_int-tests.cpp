#include "gtest/gtest.h"
#include "adaptive_radix_tree.h"

#include <vector>


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

struct IntToIntAdaptiveRadixTreeTest : public ::testing::Test
{
	IntToIntAdaptiveRadixTreeTest()
	{
	}

	~IntToIntAdaptiveRadixTreeTest()
	{
	}

	template<class TKeyGenerator>
	void FillIntKeysWithGenerator(std::vector<int>& keys, size_t count)
	{
		keys.resize(count);

		TKeyGenerator gen;
		Char4ToInt conv;
		for (size_t i = 0; i < count; ++i)
		{
			keys[i] = conv.intValue;
			gen.Next(conv.charValue);
		}
	}

	typedef adaptive_radix_tree<int, int> tree_int_int;
	adaptive_radix_tree<int, int> tree_int_int_;
};

TEST_F(IntToIntAdaptiveRadixTreeTest, Constructor_ConstructEmptyTree)
{
	EXPECT_EQ(0, tree_int_int_.size());
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_OnePair_CreatesOneNode)
{
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert(0, -1);
	EXPECT_EQ(1, tree_int_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(-1, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_WithExistingKey_ReturnsExistingNode)
{
	tree_int_int_.insert(0, -1);
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert(0, -1);
	EXPECT_EQ(1, tree_int_int_.size());
	EXPECT_EQ(false, result.second);
	EXPECT_EQ(-1, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_TwoPairs_SplitsRootNode)
{
	tree_int_int_.insert(0, -1);

	Char4ToInt secondKey("\0\x0f\0\0");
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert(secondKey.intValue, 0);
	EXPECT_EQ(2, tree_int_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(0, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_ThreePairs_AddsNodeToRoot)
{
	tree_int_int_.insert(0, 0);

	Char4ToInt secondKey("\0\x0f\0\0");
	tree_int_int_.insert(secondKey.intValue, 1);

	Char4ToInt thirdKey("\0\x01\0\0");
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert(thirdKey.intValue, 3);
	EXPECT_EQ(3, tree_int_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(3, result.first.second);
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_ChildGrowsFrom4to256)
{
	std::vector<int> keys;
	// Here we test tree in configuration root-0/1-n1-n2, where nN nodes can grow.
	// This is to test low-level nodes promotion when prefix path is not changing much.
	FillIntKeysWithGenerator<IncrementLowBitsKeyGenerator<4> >(keys, 2 * 256 * 256);

	for (size_t i = 0; i < keys.size(); ++i)
	{
		tree_int_int_.insert(keys[i], i);
		EXPECT_EQ(i + 1, tree_int_int_.size());
	}

	for (size_t i = 0; i < keys.size(); ++i)
	{
		tree_int_int::iterator findIt = tree_int_int_.find(keys[i]);
		EXPECT_TRUE(findIt != tree_int_int_.end());
		EXPECT_EQ(i, findIt.second);
	}
}

TEST_F(IntToIntAdaptiveRadixTreeTest, insert_ChildGrowsFrom4to256_ForwardKey)
{
	std::vector<int> keys;
	// Here we test tree in configuration root-n1-n2-0, where nN nodes can grow 
	// This is to test top-level nodes promotion.
	FillIntKeysWithGenerator<IncrementHighBitsKeyGenerator<4> >(keys, 2 * 256 * 256);

	for (size_t i = 0; i < keys.size(); ++i)
	{
		tree_int_int_.insert(keys[i], i);
		EXPECT_EQ(i + 1, tree_int_int_.size());
	}

	for (size_t i = 0; i < keys.size(); ++i)
	{
		tree_int_int::iterator findIt = tree_int_int_.find(keys[i]);
		EXPECT_TRUE(findIt != tree_int_int_.end());
		EXPECT_EQ(i, findIt.second);
	}
}

TEST_F(IntToIntAdaptiveRadixTreeTest, find)
{
	tree_int_int_.insert(0, -1);
	EXPECT_EQ(1, tree_int_int_.size());

	tree_int_int::iterator it = tree_int_int_.find(0);
	EXPECT_TRUE(it != tree_int_int_.end());
	EXPECT_EQ(-1, it.second);
}
