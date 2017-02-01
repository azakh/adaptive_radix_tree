#include "gtest/gtest.h"
#include "adaptive_radix_tree.h"

template<typename TKey, typename TValue>
struct AdaptiveRadixTreeTest : public ::testing::Test
{
	void FlushCache()
	{
		static const int kMemSetSize = 64 * 1024 * 1024;
		void* ptr = malloc(kMemSetSize);
		memset(ptr, 0xcd, kMemSetSize);
		free(ptr);
	}

	typedef adaptive_radix_tree<TKey, TValue> tree;
	tree tree_;
};
