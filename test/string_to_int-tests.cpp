#include "gtest/gtest.h"
#include "adaptive_radix_tree.h"
#include "base_fixture.h"
#include "simple_bench.h"

#include <vector>
#include <map>
#include <fstream>

#include <unordered_map>
#include "sparsehash/dense_hash_map"


struct StringToIntAdaptiveRadixTreeTest : AdaptiveRadixTreeTest<std::string, int>
{
	static void load_keys_from_file(const std::string& path, std::vector<std::string>& keys)
	{
		keys.clear();

		std::ifstream f(path);
		std::string line;
		while (std::getline(f, line))
		{
			if (!line.empty())
				keys.push_back(line);
		}
	}
};

TEST_F(StringToIntAdaptiveRadixTreeTest, Constructor_ConstructEmptyTree)
{
	EXPECT_EQ(0, tree_.size());
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_OnePair_CreatesOneNode)
{
	std::pair<tree::iterator, bool> result = tree_.insert("", -1);
	EXPECT_EQ(1, tree_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(-1, result.first->value);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_WithExistingKey_ReturnsExistingNode)
{
	tree_.insert("", -1);
	std::pair<tree::iterator, bool> result = tree_.insert("", -1);
	EXPECT_EQ(1, tree_.size());
	EXPECT_EQ(false, result.second);
	EXPECT_EQ(-1, result.first->value);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_TwoPairs_SplitsRootNode)
{
	tree_.insert("", -1);

	const char* secondKey = "a";
	std::pair<tree::iterator, bool> result = tree_.insert(secondKey, 0);
	EXPECT_EQ(2, tree_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(0, result.first->value);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_ThreePairs_AddsNodeToRoot)
{
	tree_.insert("", 0);

	const char* secondKey = "a";
	tree_.insert(secondKey, 1);

	const char* thirdKey = "b";
	std::pair<tree::iterator, bool> result = tree_.insert(thirdKey, 3);
	EXPECT_EQ(3, tree_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(3, result.first->value);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_and_find_KeysFromWordsDictionary)
{
	std::vector<std::string> keys;
	load_keys_from_file("words.txt", keys);
	EXPECT_GT(keys.size(), 0);

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
			EXPECT_EQ(i, findIt->value);
		}
	}

	std::map<std::string, int> map_tree;
	TIME_AUTO("map_insert")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			map_tree.insert(std::make_pair(keys[i], i));
		}
	}
	FlushCache();
	TIME_AUTO("map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::map<std::string, int>::iterator findIt = map_tree.find(keys[i]);
			//EXPECT_TRUE(findIt != map_tree.end());
			EXPECT_EQ(i, findIt->second);
		}
	}

	std::unordered_map<std::string, int> unordered_map_tree;
	TIME_AUTO("unordered_map_insert")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			unordered_map_tree.insert(std::make_pair(keys[i], i));
		}
	}
	FlushCache();
	TIME_AUTO("unordered_map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::unordered_map<std::string, int>::iterator findIt = unordered_map_tree.find(keys[i]);
			//EXPECT_TRUE(findIt != unordered_map_tree.end());
			EXPECT_EQ(i, findIt->second);
		}
	}

	google::dense_hash_map<std::string, int> dense_map_tree;
	dense_map_tree.set_empty_key("");
	TIME_AUTO("dense_map_insert")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			unordered_map_tree.insert(std::make_pair(keys[i], i));
		}
	}
	FlushCache();
	TIME_AUTO("dense_map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::unordered_map<std::string, int>::iterator findIt = unordered_map_tree.find(keys[i]);
			//EXPECT_TRUE(findIt != unordered_map_tree.end());
			EXPECT_EQ(i, findIt->second);
		}
	}
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_and_find_KeysFromUUIDDictionary)
{
	std::vector<std::string> keys;
	load_keys_from_file("uuid.txt", keys);
	EXPECT_GT(keys.size(), 0);

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
			EXPECT_EQ(i, findIt->value);
		}
	}


	std::map<std::string, int> map_tree;
	TIME_AUTO("map_insert")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			map_tree.insert(std::make_pair(keys[i], i));
		}
	}
	FlushCache();
	TIME_AUTO("map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::map<std::string, int>::iterator findIt = map_tree.find(keys[i]);
			//EXPECT_TRUE(findIt != map_tree.end());
			EXPECT_EQ(i, findIt->second);
		}
	}

	std::unordered_map<std::string, int> unordered_map_tree;
	TIME_AUTO("unordered_map_insert")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			unordered_map_tree.insert(std::make_pair(keys[i], i));
		}
	}
	FlushCache();
	TIME_AUTO("unordered_map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::unordered_map<std::string, int>::iterator findIt = unordered_map_tree.find(keys[i]);
			//EXPECT_TRUE(findIt != unordered_map_tree.end());
			EXPECT_EQ(i, findIt->second);
		}
	}

	google::dense_hash_map<std::string, int> dense_map_tree;
	dense_map_tree.set_empty_key("");
	TIME_AUTO("dense_map_insert")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			unordered_map_tree.insert(std::make_pair(keys[i], i));
		}
	}
	FlushCache();
	TIME_AUTO("dense_map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::unordered_map<std::string, int>::iterator findIt = unordered_map_tree.find(keys[i]);
			//EXPECT_TRUE(findIt != unordered_map_tree.end());
			EXPECT_EQ(i, findIt->second);
		}
	}
}

TEST_F(StringToIntAdaptiveRadixTreeTest, find)
{
	tree_.insert("", -1);
	EXPECT_EQ(1, tree_.size());

	tree::iterator it = tree_.find("");
	EXPECT_TRUE(it != tree_.end());
	EXPECT_EQ(-1, it->value);
}
