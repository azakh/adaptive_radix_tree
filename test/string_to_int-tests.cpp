#include "gtest/gtest.h"
#include "adaptive_radix_tree.h"

#include <vector>

struct StringToIntAdaptiveRadixTreeTest : public ::testing::Test
{
	StringToIntAdaptiveRadixTreeTest()
	{
	}

	~StringToIntAdaptiveRadixTreeTest()
	{
	}

	typedef adaptive_radix_tree<const char*, int> tree_int_int;
	adaptive_radix_tree<const char*, int> tree_int_int_;
};

TEST_F(StringToIntAdaptiveRadixTreeTest, Constructor_ConstructEmptyTree)
{
	EXPECT_EQ(0, tree_int_int_.size());
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_OnePair_CreatesOneNode)
{
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert("", -1);
	EXPECT_EQ(1, tree_int_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(-1, result.first.second);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_WithExistingKey_ReturnsExistingNode)
{
	tree_int_int_.insert("", -1);
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert("", -1);
	EXPECT_EQ(1, tree_int_int_.size());
	EXPECT_EQ(false, result.second);
	EXPECT_EQ(-1, result.first.second);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_TwoPairs_SplitsRootNode)
{
	tree_int_int_.insert("", -1);

	const char* secondKey = "a";
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert(secondKey, 0);
	EXPECT_EQ(2, tree_int_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(0, result.first.second);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_ThreePairs_AddsNodeToRoot)
{
	tree_int_int_.insert("", 0);

	const char* secondKey = "a";
	tree_int_int_.insert(secondKey, 1);

	const char* thirdKey = "b";
	std::pair<tree_int_int::iterator, bool> result = tree_int_int_.insert(thirdKey, 3);
	EXPECT_EQ(3, tree_int_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(3, result.first.second);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, find)
{
	tree_int_int_.insert("", -1);
	EXPECT_EQ(1, tree_int_int_.size());

	tree_int_int::iterator it = tree_int_int_.find("");
	EXPECT_TRUE(it != tree_int_int_.end());
	EXPECT_EQ(-1, it.second);
}
