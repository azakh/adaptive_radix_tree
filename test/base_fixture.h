#include "gtest/gtest.h"
#include "adaptive_radix_tree.h"

template<typename TKey, typename TValue>
struct AdaptiveRadixTreeTest : public ::testing::Test
{
    class proxy_allocator
    {
    public:
        proxy_allocator()
            : allocated_(0)
        {
        }

        void* allocate(size_t n)
        {
            void* ptr = malloc(n);
            allocated_ += n;
            return ptr;
        }

        void deallocate(void* ptr, size_t n)
        {
            allocated_ -= n;
            free(ptr);
        }

    private:
        size_t allocated_;
    };

	void FlushCache()
	{
		static const int kMemSetSize = 64 * 1024 * 1024;
		void* ptr = malloc(kMemSetSize);
		memset(ptr, 0xcd, kMemSetSize);
        uint64_t dummy = 0;
        for (size_t i = 0; i < kMemSetSize / sizeof(uint64_t); ++i)
        {
            uint64_t* v = static_cast<uint64_t*>(ptr);
            dummy += *(v + i);
        }
		free(ptr);
	}

	typedef adaptive_radix_tree<TKey, TValue, 6, proxy_allocator> tree;
	tree tree_;
};
