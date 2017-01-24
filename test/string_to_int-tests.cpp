#include "gtest/gtest.h"
#include "adaptive_radix_tree.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <chrono>


class AutoReportTimeProperty
{
	typedef std::chrono::steady_clock performance_clock;
	const char* property_name_;
	performance_clock::time_point start_;
	bool started_;
public:
	AutoReportTimeProperty(const char* property_name)
		: property_name_(property_name)
		, started_(false)
	{
	}

	bool Next()
	{
		if (!started_)
		{
			started_ = true;
			start_ = performance_clock::now();
			return true;
		}

		int64_t elapsed_ns = std::chrono::duration<int64_t, std::nano>(performance_clock::now() - start_).count();
		::testing::Test::RecordProperty(property_name_, elapsed_ns / 1000000LL);
		started_ = false;
		return false;
	}
};

#define TIME_AUTO(propertyName) for (AutoReportTimeProperty auto_report_time_property(propertyName); auto_report_time_property.Next();)

struct StringToIntAdaptiveRadixTreeTest : public ::testing::Test
{
	StringToIntAdaptiveRadixTreeTest()
	{
	}

	~StringToIntAdaptiveRadixTreeTest()
	{
	}

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

	typedef adaptive_radix_tree<std::string, int> tree_string_int;
	tree_string_int tree_string_int_;
};

TEST_F(StringToIntAdaptiveRadixTreeTest, Constructor_ConstructEmptyTree)
{
	EXPECT_EQ(0, tree_string_int_.size());
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_OnePair_CreatesOneNode)
{
	std::pair<tree_string_int::iterator, bool> result = tree_string_int_.insert("", -1);
	EXPECT_EQ(1, tree_string_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(-1, result.first.second);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_WithExistingKey_ReturnsExistingNode)
{
	tree_string_int_.insert("", -1);
	std::pair<tree_string_int::iterator, bool> result = tree_string_int_.insert("", -1);
	EXPECT_EQ(1, tree_string_int_.size());
	EXPECT_EQ(false, result.second);
	EXPECT_EQ(-1, result.first.second);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_TwoPairs_SplitsRootNode)
{
	tree_string_int_.insert("", -1);

	const char* secondKey = "a";
	std::pair<tree_string_int::iterator, bool> result = tree_string_int_.insert(secondKey, 0);
	EXPECT_EQ(2, tree_string_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(0, result.first.second);
}

TEST_F(StringToIntAdaptiveRadixTreeTest, insert_ThreePairs_AddsNodeToRoot)
{
	tree_string_int_.insert("", 0);

	const char* secondKey = "a";
	tree_string_int_.insert(secondKey, 1);

	const char* thirdKey = "b";
	std::pair<tree_string_int::iterator, bool> result = tree_string_int_.insert(thirdKey, 3);
	EXPECT_EQ(3, tree_string_int_.size());
	EXPECT_EQ(true, result.second);
	EXPECT_EQ(3, result.first.second);
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
			tree_string_int_.insert(keys[i], i);
			EXPECT_EQ(i + 1, tree_string_int_.size());
		}
	}

	TIME_AUTO("find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			tree_string_int::iterator findIt = tree_string_int_.find(keys[i]);
			EXPECT_TRUE(findIt != tree_string_int_.end());
			EXPECT_EQ(i, findIt.second);
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
	TIME_AUTO("map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::map<std::string, int>::iterator findIt = map_tree.find(keys[i]);
			EXPECT_TRUE(findIt != map_tree.end());
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
	TIME_AUTO("unordered_map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::unordered_map<std::string, int>::iterator findIt = unordered_map_tree.find(keys[i]);
			EXPECT_TRUE(findIt != unordered_map_tree.end());
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
			tree_string_int_.insert(keys[i], i);
			EXPECT_EQ(i + 1, tree_string_int_.size());
		}
	}

	TIME_AUTO("find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			tree_string_int::iterator findIt = tree_string_int_.find(keys[i]);
			EXPECT_TRUE(findIt != tree_string_int_.end());
			EXPECT_EQ(i, findIt.second);
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
	TIME_AUTO("map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::map<std::string, int>::iterator findIt = map_tree.find(keys[i]);
			EXPECT_TRUE(findIt != map_tree.end());
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
	TIME_AUTO("unordered_map_find")
	{
		for (size_t i = 0; i < keys.size(); ++i)
		{
			std::unordered_map<std::string, int>::iterator findIt = unordered_map_tree.find(keys[i]);
			EXPECT_TRUE(findIt != unordered_map_tree.end());
			EXPECT_EQ(i, findIt->second);
		}
	}
}

TEST_F(StringToIntAdaptiveRadixTreeTest, find)
{
	tree_string_int_.insert("", -1);
	EXPECT_EQ(1, tree_string_int_.size());

	tree_string_int::iterator it = tree_string_int_.find("");
	EXPECT_TRUE(it != tree_string_int_.end());
	EXPECT_EQ(-1, it.second);
}
