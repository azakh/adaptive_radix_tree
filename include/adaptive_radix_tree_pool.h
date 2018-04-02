#pragma once

#include <memory.h>
#include <assert.h>

template<typename T, typename TAllocator, size_t kItemAlignment, size_t kPoolBlockSize>
class pool
{
public:
    struct block_header;
    struct used_item
    {
        uint8_t data[sizeof(T)];
        block_header* block;
    };

    struct free_item
    {
        free_item* next_free_item;
    };

    union item
    {
        used_item used;
        free_item free;
    };

    struct block_header
    {
        block_header() :
            used(0),
            first_free_item(NULL),
            prev(NULL),
            next(NULL)
        {}

        size_t used;
        free_item* first_free_item;
        block_header* prev;
        block_header* next;
    };

    enum
    {
        kBlockBytes = sizeof(block_header) + kPoolBlockSize * sizeof(item) + kItemAlignment - 1
    };

    pool(TAllocator& alloc) :
        first_free_block_(NULL),
        first_block_(NULL),
        used_(0),
        alloc_(alloc)
    {
    }

    ~pool()
    {
        deallocate_blocks(first_free_block_);
        deallocate_blocks(first_block_);
    }

    void* allocate(size_t size)
    {
        assert(size == sizeof(T));

        // Make sure we have a free block to grab item from.
        if (first_free_block_ == NULL)
            allocate_free_block();

        free_item* fi = first_free_block_->first_free_item;
        first_free_block_->first_free_item = fi->next_free_item;

        used_item* i = reinterpret_cast<used_item*>(fi);
        i->block = first_free_block_;
        first_free_block_->used++;
        used_++;

        // If the block is used, move it to used block list.
        if (first_free_block_->first_free_item == NULL)
        {
            block_header* next_free = first_free_block_->next;
            first_free_block_->next = first_block_;
            if (first_block_ != NULL)
                first_block_->prev = first_free_block_;
            first_block_ = first_free_block_;

            // And set the next free one.
            first_free_block_ = next_free;
        }

        return i;
    }

    void deallocate(void* ptr)
    {
        used_item* i = static_cast<used_item*>(ptr);
        block_header* block = i->block;
        const bool in_used_list = block->first_free_item == NULL;

        free_item* fi = reinterpret_cast<free_item*>(i);
        fi->next_free_item = block->first_free_item;
        block->first_free_item = fi;

        block->used--;
        used_--;

        // If completely empty and there is any free block, delete it
        if (block->used == 0 && first_free_block_ != NULL && first_free_block_ != block)
        {
            if (first_free_block_ == block)
            {
                first_free_block_ = block->next;
                if (first_free_block_ != NULL)
                    first_free_block_->prev = NULL;
            }
            else
            {
                block_header* prev = block->prev;
                block_header* next = block->next;
                if (prev != NULL)
                    prev->next = next;
                if (next != NULL)
                    next->prev = prev;
            }

            alloc_.deallocate(block, kBlockBytes);
            return;
        }
        if (!in_used_list)
            return;

        // Block has now free items, remove from used blocks list.
        if (first_block_ == block)
        {
            first_block_ = block->next;
            if (first_block_ != NULL)
                first_block_->prev = NULL;
        }
        else
        {
            block_header* prev = block->prev;
            block_header* next = block->next;
            if (prev != NULL)
                prev->next = next;
            if (next != NULL)
                next->prev = prev;
        }

        // Put to free blocks stack.
        block->next = first_free_block_;
        if (first_free_block_ != NULL)
            first_free_block_->prev = block;
        first_free_block_ = block;
    }

private:
    static void* align(void* ptr)
    {
        return reinterpret_cast<void*>((reinterpret_cast<size_t>(ptr) + kItemAlignment - 1) & (~(kItemAlignment - 1)));
    }

    void allocate_free_block()
    {
        // Allocate a new block.
        uint8_t* ptr = static_cast<uint8_t*>(alloc_.allocate(kBlockBytes));
        const uint8_t* end_ptr = ptr + kBlockBytes - sizeof(item);
        block_header* block = new (ptr) block_header();

        // Slice it by "item" size and push items to a free list.
        ptr = static_cast<uint8_t*>(align(block + 1));
        while (ptr <= end_ptr)
        {
            free_item* i = reinterpret_cast<free_item*>(ptr);
            i->next_free_item = block->first_free_item;
            block->first_free_item = i;

            // Next item
            ptr += sizeof(item);
            ptr = static_cast<uint8_t*>(ptr);
        }

        // Add block to free list.
        block->next = first_free_block_;
        if (first_free_block_ != NULL)
            first_free_block_->prev = block;
        first_free_block_ = block;
    }

    void deallocate_blocks(block_header* block)
    {
        while (block != NULL)
        {
            block_header* next = block->next;
            alloc_.deallocate(static_cast<void*>(block), kBlockBytes);
            block = next;
        }
    }

    block_header* first_free_block_;
    block_header* first_block_;
    size_t used_;
    TAllocator& alloc_;
};
