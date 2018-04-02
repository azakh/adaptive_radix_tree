#pragma once

#include "adaptive_radix_tree_pool.h"
#include <memory.h>
#include <assert.h>
#include <cstdint>
#include <utility>
#include <algorithm>

// Only temporary to avoid size calculation for string.
#include <string>

// sizeof(base_adaptive_radix_tree::node_leaf)	24
// sizeof(base_adaptive_radix_tree::node_4)		56
// sizeof(base_adaptive_radix_tree::node_16)	160
// sizeof(base_adaptive_radix_tree::node_48)	656
// sizeof(base_adaptive_radix_tree::node_256)	2064

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

template<typename TValue, size_t kMaxPrefixLength = 6, typename TAllocator = proxy_allocator>
class base_adaptive_radix_tree
{
protected:
    // Node type is defined by node pointer low 4 bits.
    struct node_leaf_traits { enum { kPointerEmbeddedType = 0 }; };
    struct node_4_traits    { enum { kPointerEmbeddedType = 1 }; enum { kMaxChildrenCount = 4 }; };
    struct node_16_traits   { enum { kPointerEmbeddedType = 2 }; enum { kMaxChildrenCount = 16 }; };
    struct node_48_traits   { enum { kPointerEmbeddedType = 3 }; enum { kMaxChildrenCount = 48 }; };
    struct node_96_traits   { enum { kPointerEmbeddedType = 4 }; enum { kMaxChildrenCount = 96 }; };
    struct node_256_traits  { enum { kPointerEmbeddedType = 5 }; enum { kMaxChildrenCount = 256 }; };

    enum
    {
        kPointerEmbeddedTypeMask = 7
    };

    enum
    {
        kNodeAlignment = 8
    };


    typedef void* node_ptr;

    // Node type is stored as lower 3 bits of node pointer.
    // Thus node must be always aligned by 8 bytes!
    struct node_ptr_with_type
    {
        node_ptr_with_type()
            : ptr(NULL)
        {
        }

        node_ptr_with_type(const node_ptr_with_type& other)
            : ptr(other.ptr)
        {
        }

        template<typename TNode>
        node_ptr_with_type(TNode* n)
            : ptr(reinterpret_cast<node_ptr>((reinterpret_cast<size_t>(n) | TNode::traits::kPointerEmbeddedType)))
        {
        }

        node_ptr get_raw_node() const { return reinterpret_cast<node_ptr>((reinterpret_cast<size_t>(ptr) & ~kPointerEmbeddedTypeMask)); }
        size_t get_node_type() const { return (reinterpret_cast<size_t>(ptr) & kPointerEmbeddedTypeMask); }

        template<typename TNode>
        bool is_node_type() const
        {
            return (reinterpret_cast<size_t>(ptr) & kPointerEmbeddedTypeMask) == TNode::traits::kPointerEmbeddedType;
        }

        bool is_node_leaf() const
        {
            return (reinterpret_cast<size_t>(ptr) & kPointerEmbeddedTypeMask) == node_leaf::traits::kPointerEmbeddedType;
        }

        template<typename TNode>
        TNode* get_node() const
        {
            return static_cast<TNode*>(get_raw_node());
        }

        operator node_ptr() const
        {
            return get_raw_node();
        }

        bool is_valid() const
        {
            return ptr != NULL;
        }

        node_ptr ptr;
    };


    // Base node. Contains key prefix and children data.
    struct node
    {
        node(const uint8_t* key, uint8_t keyLen)
            : prefixLength(keyLen)
            , childrenCount(0)
        {
            memcpy(prefix, key, keyLen);
        }

        uint8_t prefixLength;
        uint8_t prefix[kMaxPrefixLength];
        uint8_t childrenCount;
    };

    // Simple indexed node template.
    // Dictionary-like storage of child nodes.
    // L1 cache friendly with possible sorting for larger nodes.
    template<typename TNodeTraits>
    struct node_array_indexed : node
    {
        typedef TNodeTraits traits;

        node_array_indexed(const uint8_t* key, uint8_t keyLen)
            : node(key, keyLen)
        {
        }

        node_ptr_with_type get_child(uint8_t k) const
        {
            for (uint8_t i = 0; i < childrenCount; ++i)
            {
                if (keys[i] == k)
                    return children[i];
            }
            return node_ptr_with_type();
        }

        node_ptr_with_type* get_child_ptr(uint8_t k)
        {
            for (uint8_t i = 0; i < childrenCount; ++i)
            {
                if (keys[i] == k)
                    return &children[i];
            }
            return NULL;
        }

        template<typename TNode>
        bool add_child(TNode* n_ptr, uint8_t k)
        {
            if (childrenCount >= traits::kMaxChildrenCount)
                return false;

            keys[childrenCount] = k;
            children[childrenCount] = n_ptr;
            childrenCount++;

            return true;
        }

        bool add_child(node_ptr_with_type n, uint8_t k)
        {
            if (childrenCount >= traits::kMaxChildrenCount)
                return false;

            keys[childrenCount] = k;
            children[childrenCount] = n;
            childrenCount++;

            return true;
        }

        uint8_t keys[traits::kMaxChildrenCount];
        node_ptr_with_type children[traits::kMaxChildrenCount];
    };
    typedef node_array_indexed<node_4_traits>  node_4;
    typedef node_array_indexed<node_16_traits> node_16;

    // Bit-indexed node.
    template<typename TNodeTraits>
    struct node_bit_indexed : node
    {
        typedef TNodeTraits traits;

        node_bit_indexed(const uint8_t* key, uint8_t keyLen)
            : node(key, keyLen)
        {
            memset(keys, kInvalidIndex, sizeof(keys));
        }

        node_ptr_with_type get_child(uint8_t k) const
        {
            return keys[k] != kInvalidIndex ? children[keys[k]] : node_ptr_with_type();
        }

        node_ptr_with_type* get_child_ptr(uint8_t k)
        {
            return keys[k] != kInvalidIndex ? &children[keys[k]] : NULL;
        }

        template<typename TNode>
        bool add_child(TNode* n_ptr, uint8_t k)
        {
            if (childrenCount >= traits::kMaxChildrenCount)
                return false;

            keys[k] = childrenCount;
            children[childrenCount] = n_ptr;
            childrenCount++;

            return true;
        }

        bool add_child(node_ptr_with_type n, uint8_t k)
        {
            if (childrenCount >= traits::kMaxChildrenCount)
                return false;

            keys[k] = childrenCount;
            children[childrenCount] = n;
            childrenCount++;

            return true;
        }

        enum { kInvalidIndex = 0xFF };

        uint8_t keys[256];
        node_ptr_with_type children[traits::kMaxChildrenCount];
    };
    typedef node_bit_indexed<node_48_traits> node_48;
    typedef node_bit_indexed<node_96_traits> node_96;

    // Full inner node.
    // 256 children accessible through indexing.
    struct node_256 : node
    {
        typedef node_256_traits traits;

        node_256(const uint8_t* key, uint8_t keyLen)
            : node(key, keyLen)
        {
            memset(children, 0, sizeof(children));
        }

        node_ptr_with_type get_child(uint8_t k) const
        {
            return children[k];
        }

        node_ptr_with_type* get_child_ptr(uint8_t k)
        {
            return &children[k];
        }

        template<typename TNode>
        bool add_child(TNode* n_ptr, uint8_t k)
        {
            children[k] = n_ptr;
            childrenCount++;

            return true;
        }

        bool add_child(node_ptr_with_type n, uint8_t k)
        {
            children[k] = n;
            childrenCount++;

            return true;
        }

        node_ptr_with_type children[traits::kMaxChildrenCount];
    };

    // Leaf node. Only value.
    struct node_leaf
    {
        typedef node_leaf_traits traits;

        node_leaf(const TValue& v)
            : value(v)
        {
        }

        TValue value;
    };

public:
    struct iterator
    {
        iterator() : cursor(), second(*static_cast<TValue*>(NULL)) {}
        iterator(node_ptr_with_type c)
            : cursor(c)
            , second(cursor.get_node<node_leaf>()->value)
        {
            assert(cursor.is_node_leaf());
        }

        bool operator== (const iterator& rhs) const
        {
            return cursor == rhs.cursor;
        }
        bool operator!= (const iterator& rhs) const
        {
            return !(*this == rhs);
        }

        node_ptr_with_type cursor;
        TValue& second;
    };

    base_adaptive_radix_tree() :
        size_(0),
        alloc_(),
        pool_allocators_(alloc_)
    {
        root_ = construct_raw_node_4(pool_allocators_, NULL, 0);
    }

    ~base_adaptive_radix_tree()
    {
        clear();
    }

    size_t size() const
    {
        return size_;
    }

    void clear()
    {
        size_ = 0;
    }

    node_leaf* find(const uint8_t* key, size_t keyLen) const
    {
        assert(keyLen != 0);

        // Starting from root
        node_ptr_with_type current_node_with_type = root_;
        while (current_node_with_type.is_valid())
        {
            const node* n = current_node_with_type.get_node<node>();
            const size_t prefixLen = n->prefixLength;
            // Process prefix
            if (prefixLen != 0)
            {
                // If prefix is larger than key part, no match
                if (prefixLen > keyLen) // TODO: unlikely
                    return NULL;

                // Check if prefix matches key part
                if (memcmp(n->prefix, key, prefixLen) != 0)
                    return NULL;

                key += prefixLen;
                keyLen -= prefixLen;
            }

            // Continue descending
            current_node_with_type = get_child(current_node_with_type, *key);

            // Check if we found leaf
            if (current_node_with_type.is_node_leaf())
                return current_node_with_type.get_node<node_leaf>();

            // Jumping to the child means we use a character as an index key.
            key++;
            keyLen--;
        }

        return NULL;
    }

    std::pair<node_leaf*, bool> insert(const uint8_t* key, size_t keyLen, const TValue& value)
    {
        assert(root_.is_valid());
        assert(keyLen != 0);

        // Starting from root
        node_ptr_with_type* current_node_with_type = &root_;
        for (;;)
        {
            node* n = current_node_with_type->get_node<node>();

            // If prefix is larger than key part, no match
            const uint8_t prefixLen = n->prefixLength;
            // Process prefix
            if (prefixLen != 0)
            {
                // Find out if there is common prefix for old node and new one
                uint8_t common_key_lenght = (uint8_t) get_common_key_lenght(key, keyLen, n->prefix, n->prefixLength);
                if (common_key_lenght < prefixLen)
                {
                    // Key prefix has a partial match, meaning this node should be split
                    return std::make_pair<node_leaf*, bool>(add_leaf_with_split(current_node_with_type, key, keyLen, common_key_lenght, value), true);
                }

                // Overwise continue
                key += prefixLen;
                keyLen -= prefixLen;
            }

            // Continue descending
            node_ptr_with_type* childptr = get_child_ptr(*current_node_with_type, *key);
            if (childptr == NULL || !childptr->is_valid())
            {
                // No child, just add a new leaf
                node_leaf* nleaf = add_leaf(current_node_with_type, key, keyLen, value);
                return std::pair<node_leaf*, bool>(nleaf, true);
            }

            // Going deeper
            current_node_with_type = childptr;
            key++;
            keyLen--;

            // If no key left, we found an existing node.
            if (keyLen == 0)
            {
                if (current_node_with_type->is_node_leaf())
                {
                    // Return existing node
                    return std::pair<node_leaf*, bool>(current_node_with_type->get_node<node_leaf>(), false);
                }
                else
                {
                    // Intermediate node which should also contain data.
                    // This is not supported in a current model. The key should be unique and define leaf unambiguously.
                    // This could be a valid use case for not null-terminated strings or any variable length data.
                    // TODO: Consider adding data to any node.
                    assert(false);
                    return std::pair<node_leaf*, bool>(NULL, false);
                }
            }
        }
    }

    void erase(const uint8_t* key, size_t keyLen)
    {
        node* found_node = find(key, keyLen);
        erase(found_node);
    }

    node_leaf* erase(node* n)
    {


        return NULL;
    }

private:
    struct pool_allocators
    {
        pool_allocators(TAllocator& alloc) :
            alloc_leaf(alloc),
            alloc_node_4(alloc),
            alloc_node_16(alloc),
            alloc_node_48(alloc),
            alloc_node_96(alloc),
            alloc_node_256(alloc)
        {}

        pool<node_leaf, TAllocator, kNodeAlignment, 1024> alloc_leaf;
        pool<node_4,    TAllocator, kNodeAlignment, 1024> alloc_node_4;
        pool<node_16,   TAllocator, kNodeAlignment, 1024> alloc_node_16;
        pool<node_48,   TAllocator, kNodeAlignment,  512> alloc_node_48;
        pool<node_96,   TAllocator, kNodeAlignment,  512> alloc_node_96;
        pool<node_256,  TAllocator, kNodeAlignment,  256> alloc_node_256;
    };

    // Construct a leaf node with the specific parent and key (which should be <= node::kMaxPrefixLength)
    // Setting parent here is faster than in add_child as we don't need to convert node_ptr_with_type to node_ptr.
    static node_leaf* construct_raw_leaf(pool_allocators& alloc, const TValue& value)
    {
        // TODO: include valueAlignment into account
        // TODO: Block allocator
        // Allocate aligned node
        node_leaf* n = reinterpret_cast<node_leaf*>(alloc.alloc_leaf.allocate(sizeof(node_leaf)));
        assert((reinterpret_cast<size_t>(n) & kPointerEmbeddedTypeMask) == 0);
        // Construct
        new (n) node_leaf(value);
        return n;
    }

    template<typename TNode, typename TAllocator>
    static TNode* construct_raw_index(TAllocator& alloc, const uint8_t* key, uint8_t keyLen)
    {
        assert(keyLen <= kMaxPrefixLength);

        // TODO: Block allocator
        // Allocate aligned node
        TNode* n = reinterpret_cast<TNode*>(alloc.allocate(sizeof(TNode)));
        assert((reinterpret_cast<size_t>(n) & kPointerEmbeddedTypeMask) == 0);
        // Construct
        new (n) TNode(key, keyLen);
        return n;
    }

    template<typename TNode> struct construct_policy;

    template<> struct construct_policy<node_4>
    {
        static node_4* construct_node(pool_allocators& alloc, const uint8_t* key, uint8_t keyLen)
        {
            return construct_raw_index<node_4>(alloc.alloc_node_4, ncopy.prefix, ncopy.prefixLength);
        }
    };

    template<> struct construct_policy<node_16>
    {
        static node_16* construct_node(pool_allocators& alloc, const uint8_t* key, uint8_t keyLen)
        {
            return construct_raw_index<node_16>(alloc.alloc_node_16, ncopy.prefix, ncopy.prefixLength);
        }
    };

    template<> struct construct_policy<node_48>
    {
        static node_48* construct_node(pool_allocators& alloc, const uint8_t* key, uint8_t keyLen)
        {
            return construct_raw_index<node_48>(alloc.alloc_node_48, ncopy.prefix, ncopy.prefixLength);
        }
    };

    template<> struct construct_policy<node_96>
    {
        static node_96* construct_node(pool_allocators& alloc, const uint8_t* key, uint8_t keyLen)
        {
            return construct_raw_index<node_96>(alloc.alloc_node_96, ncopy.prefix, ncopy.prefixLength);
        }
    };

    template<> struct construct_policy<node_256>
    {
        static node_256* construct_node(pool_allocators& alloc, const uint8_t* key, uint8_t keyLen)
        {
            return construct_raw_index<node_48>(alloc.alloc_node_256, ncopy.prefix, ncopy.prefixLength);
        }
    };

    template<typename TNode, typename TAllocator>
    static void destruct_node_n(TAllocator& alloc, TNode* n)
    {
        alloc.deallocate(reinterpret_cast<uint8_t*>(n));
    }

    template<typename TNode> struct destruct_policy;

    template<> struct destruct_policy<node_4>
    {
        static void destruct_node(pool_allocators& alloc, node_4* n)
        {
            destruct_node_n<node_4>(alloc.alloc_node_4, n);
        }
    };

    template<> struct destruct_policy<node_16>
    {
        static void destruct_node(pool_allocators& alloc, node_16* n)
        {
            destruct_node_n<node_16>(alloc.alloc_node_16, n);
        }
    };

    template<> struct destruct_policy<node_48>
    {
        static void destruct_node(pool_allocators& alloc, node_48* n)
        {
            destruct_node_n<node_48>(alloc.alloc_node_48, n);
        }
    };

    template<> struct destruct_policy<node_96>
    {
        static void destruct_node(pool_allocators& alloc, node_96* n)
        {
            destruct_node_n<node_96>(alloc.alloc_node_96, n);
        }
    };

    template<> struct destruct_policy<node_256>
    {
        static void destruct_node(pool_allocators& alloc, node_256* n)
        {
            destruct_node_n<node_256>(alloc.alloc_node_256, n);
        }
    };

    template<typename TNode> struct grow_policy;
    template<typename TNode> struct shrink_policy;

    template<> struct grow_policy<node_4>
    {
        typedef node_16 grow_type; // We don't really need it with auto, but doing this for pre c++11 support
        static node_16* copy_construct_node(pool_allocators& alloc, const node_4& ncopy)
        {
            node_16* n = construct_raw_index<node_16>(alloc.alloc_node_16, ncopy.prefix, ncopy.prefixLength);
            n->childrenCount = ncopy.childrenCount;
            memcpy(n->keys, ncopy.keys, ncopy.childrenCount);
            memcpy(n->children, ncopy.children, ncopy.childrenCount * sizeof(ncopy.children[0]));

            return n;
        }
    };

    template<> struct shrink_policy<node_16>
    {
        typedef node_4 shrink_type;
        static shrink_type* copy_construct_node(pool_allocators& alloc, const node_16& ncopy)
        {
            node_4* n = construct_raw_index<node_4>(alloc.alloc_node_4, ncopy.prefix, ncopy.prefixLength);
            n->childrenCount = ncopy.childrenCount;
            memcpy(n->keys, ncopy.keys, ncopy.childrenCount);
            memcpy(n->children, ncopy.children, ncopy.childrenCount * sizeof(ncopy.children[0]));

            return n;
        }
    };
    template<> struct grow_policy<node_16>
    {
        typedef node_48 grow_type;
        static node_48* copy_construct_node(pool_allocators& alloc, const node_16& ncopy)
        {
            node_48* n = construct_raw_index<node_48>(alloc.alloc_node_48, ncopy.prefix, ncopy.prefixLength);
            n->childrenCount = ncopy.childrenCount;
            for (uint8_t i = 0; i < ncopy.childrenCount; ++i)
            {
                n->keys[ncopy.keys[i]] = i;
                n->children[i] = ncopy.children[i];
            }

            return n;
        }
    };
    //template<> struct grow_policy<node_16>
    //{
    //    typedef node_96 grow_type;
    //    static node_96* copy_construct_node(pool_allocators& alloc, const node_16& ncopy)
    //    {
    //        node_96* n = construct_raw_index<node_96>(alloc.alloc_node_96, ncopy.prefix, ncopy.prefixLength);
    //        n->childrenCount = ncopy.childrenCount;
    //        for (uint8_t i = 0; i < ncopy.childrenCount; ++i)
    //        {
    //            n->keys[ncopy.keys[i]] = i;
    //            n->children[i] = ncopy.children[i];
    //        }

    //        return n;
    //    }
    //};

    template<> struct shrink_policy<node_48>
    {
        typedef node_16 shrink_type;
        static node_16* copy_construct_node(pool_allocators& alloc, const node_48& ncopy)
        {
            node_16* n = construct_raw_index<node_16>(alloc.alloc_node_16, ncopy.prefix, ncopy.prefixLength);
            for (size_t i = 0; i < sizeof(node_48::keys); ++i)
            {
                if (ncopy.keys[i] == node_48::kInvalidIndex)
                    continue;

                n->keys[n->childrenCount] = i;
                n->children[n->childrenCount] = ncopy.children[ncopy.keys[i]];
                n->childrenCount++;
            }

            return n;
        }
    };
    template<> struct grow_policy<node_48>
    {
        typedef node_96 grow_type;
        static node_96* copy_construct_node(pool_allocators& alloc, const node_48& ncopy)
        {
            node_96* n = construct_raw_index<node_96>(alloc.alloc_node_96, ncopy.prefix, ncopy.prefixLength);
            n->childrenCount = ncopy.childrenCount;
            memcpy(n->keys, ncopy.keys, sizeof(ncopy.keys));
            memcpy(n->children, ncopy.children, ncopy.childrenCount * sizeof(ncopy.children[0]));

            return n;
        }
    };

    template<> struct shrink_policy<node_96>
    {
        typedef node_48 shrink_type;
        static node_48* copy_construct_node(pool_allocators& alloc, const node_96& ncopy)
        {
            node_48* n = construct_raw_index<node_48>(alloc.alloc_node_48, ncopy.prefix, ncopy.prefixLength);
            n->childrenCount = ncopy.childrenCount;
            memcpy(n->keys, ncopy.keys, sizeof(ncopy.keys));
            memcpy(n->children, ncopy.children, ncopy.childrenCount * sizeof(ncopy.children[0]));

            return n;
        }
    };
    template<> struct grow_policy<node_96>
    {
        typedef node_256 grow_type;
        static node_256* copy_construct_node(pool_allocators& alloc, const node_96& ncopy)
        {
            node_256* n = construct_raw_index<node_256>(alloc.alloc_node_256, ncopy.prefix, ncopy.prefixLength);
            n->childrenCount = ncopy.childrenCount;
            for (size_t i = 0; i < sizeof(node_96::keys); ++i)
            {
                if (ncopy.keys[i] == node_96::kInvalidIndex)
                    continue;

                n->children[i] = ncopy.children[ncopy.keys[i]];
            }

            return n;
        }
    };

    template<> struct shrink_policy<node_256>
    {
        typedef node_96 shrink_type;
        static node_96* copy_construct_node(pool_allocators& alloc, const node_256& ncopy)
        {
            node_96* n = construct_raw_index<node_96>(alloc.alloc_node_96, ncopy.prefix, ncopy.prefixLength);
            for (size_t i = 0; i < ncopy.childrenCount; ++i)
            {
                if (!ncopy.children[i].is_valid())
                    continue;

                n->keys[i] = n->childrenCount;
                n->children[n->childrenCount] = ncopy.children[i];
                n->childrenCount++;
            }

            return n;
        }
    };


    static node_4* construct_raw_node_4(pool_allocators& alloc, const uint8_t* key, uint8_t keyLen) { return construct_raw_index<node_4>(alloc.alloc_node_4, key, keyLen); }

    // Return k child of the node n.
    static node_ptr_with_type get_child(node_ptr_with_type n, uint8_t k)
    {
        size_t node_type = n.get_node_type();
        assert(node_type != node_leaf_traits::kPointerEmbeddedType);

        return node_type == node_4_traits::kPointerEmbeddedType ? n.get_node<node_4>()->get_child(k) :
            node_type == node_16_traits::kPointerEmbeddedType ? n.get_node<node_16>()->get_child(k) :
            node_type < node_256_traits::kPointerEmbeddedType ? n.get_node<node_96>()->get_child(k) :
            n.get_node<node_256>()->get_child(k);
    }

    static node_ptr_with_type* get_child_ptr(node_ptr_with_type n, uint8_t k)
    {
        size_t node_type = n.get_node_type();
        assert(node_type != node_leaf_traits::kPointerEmbeddedType);

        return node_type == node_4_traits::kPointerEmbeddedType ? n.get_node<node_4>()->get_child_ptr(k) :
            node_type == node_16_traits::kPointerEmbeddedType ? n.get_node<node_16>()->get_child_ptr(k) :
            node_type < node_256_traits::kPointerEmbeddedType ? n.get_node<node_96>()->get_child_ptr(k) :
            n.get_node<node_256>()->get_child_ptr(k);
    }

    template<typename TNode>
    bool try_add_child(node_ptr_with_type* nptr, node_ptr_with_type childptr, uint8_t key)
    {
        if (!nptr->is_node_type<TNode>())
            return false;

        // Add if we have space
        TNode* ntyped = nptr->get_node<TNode>();
        if (ntyped->add_child(childptr, key))
            return true;

        // Grow to larger node and add new child
        typedef typename grow_policy<TNode>::grow_type TNodeGrow;
        TNodeGrow* ngrown = grow_policy<TNode>::copy_construct_node(pool_allocators_, *ntyped);
        ngrown->add_child(childptr, key);
        // Delete old node
        destruct_policy<TNode>::destruct_node(pool_allocators_, ntyped);

        // Overwrite old reference
        *nptr = ngrown;

        return true;
    }

    // Add child to the nptr node with conversion if necessary.
    void add_child(node_ptr_with_type* nbox, node_ptr_with_type childptr, uint8_t key)
    {
        assert(!nbox->is_node_leaf());

        if (try_add_child<node_4>(nbox, childptr, key))
            return;

        if (try_add_child<node_16>(nbox, childptr, key))
            return;

        if (try_add_child<node_48>(nbox, childptr, key))
            return;

        if (try_add_child<node_96>(nbox, childptr, key))
            return;

        nbox->get_node<node_256>()->add_child(childptr, key);
    }

    // Add leaf node.
    node_leaf* add_leaf(node_ptr_with_type* parent_node, const uint8_t* key, size_t keyLen, const TValue& value)
    {
        assert(parent_node != NULL);

        node_ptr parent = parent_node->get_raw_node();

        // In case the key is large than prefix length, we create intermediate nodes which can contain the full key.
        // The nodes can hold all the key, but leave one character for the leaf
        node_4* lastSplitNode4 = NULL;
        while (keyLen > 1)
        {
            const uint8_t prefixLen = std::min<uint8_t>((uint8_t) (keyLen - 2), (uint8_t) kMaxPrefixLength);
            node_4* newSplitNode = construct_raw_node_4(pool_allocators_, key + 1, prefixLen);;
            if (lastSplitNode4 != NULL)
                lastSplitNode4->add_child(newSplitNode, *key);
            else
                add_child(parent_node, newSplitNode, *key);
            key += prefixLen + 1;
            keyLen -= prefixLen + 1;

            lastSplitNode4 = newSplitNode;
        }

        node_leaf* leafNode = construct_raw_leaf(pool_allocators_, value);
        if (lastSplitNode4 != NULL)
            lastSplitNode4->add_child(leafNode, *key);
        else
            add_child(parent_node, leafNode, *key);

        size_++;

        return leafNode;
    }

    // Split node_to_split node.
    // Create node_4 which replaces node_to_split. New node points to node_to_split and new leaf node with the specified key.
    node_leaf* add_leaf_with_split(node_ptr_with_type* node_to_split, const uint8_t* key, size_t keyLen, uint8_t different_key_pos, const TValue& value)
    {
        node* n = node_to_split->get_node<node>();

        // Create a split new node with common prefix
        node_4* newSplitNode = construct_raw_node_4(pool_allocators_, key, different_key_pos);

        // Move the old node to the new node as a child
        newSplitNode->add_child(*node_to_split, n->prefix[different_key_pos]);

        // Advance to the different key position
        key += different_key_pos;
        keyLen -= different_key_pos;
        // There should be at least one character left in a key which is different from original node
        assert(keyLen > 0);

        // Patch a prefix of the old node
        // Char used in split node
        different_key_pos++;
        // Move a prefix of the old node down by different_key_pos elements (which were used in a split node)
        assert(n->prefixLength >= different_key_pos);
        for (size_t i = 0, c = n->prefixLength - different_key_pos; i < c; ++i)
            n->prefix[i] = n->prefix[different_key_pos + i];
        n->prefixLength -= different_key_pos;

        // Replace the original node
        *node_to_split = newSplitNode;

        // Place leaf as a child.
        node_leaf* leafNode = add_leaf(node_to_split, key, keyLen, value);

        return leafNode;
    }

    // Return the position of the first different character of two keys.
    static size_t get_common_key_lenght(const uint8_t* __restrict key1, size_t key1Len, const uint8_t* __restrict key2, size_t key2Len)
    {
        size_t i = 0;
        const size_t len = std::min<size_t>(key1Len, key2Len);
        for (; i < len; ++i)
        {
            if (key1[i] != key2[i])
                break;
        }
        return i;
    }

    // Root node
    node_ptr_with_type root_;
    // Total number of nodes
    size_t size_;
    // Allocators
    TAllocator alloc_;
    pool_allocators pool_allocators_;
};

// TKey can be:
// - const char*
// - primitive type: int, float...
// - POD struct
template<typename TKey, typename TValue>
class adaptive_radix_tree : base_adaptive_radix_tree<TValue>
{
public:
    typedef base_adaptive_radix_tree<TValue> base_tree;
    typedef base_tree::iterator iterator;

    adaptive_radix_tree()
        : base_tree()
    {
    }

    size_t size() const
    {
        return base_tree::size();
    }

    iterator end() const
    {
        return iterator();
    }

    std::pair<iterator, bool> insert(const TKey& key, const TValue& value)
    {
        std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(&key), sizeof(key), value);
        return std::pair<iterator, bool>(iterator(result.first), result.second);
    }

    iterator find(const TKey& key) const
    {
        node_leaf* n = base_tree::find(reinterpret_cast<const uint8_t*>(&key), sizeof(key));
        return iterator(n);
    }

    TValue& operator[](const TKey& key)
    {
        std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(&key), sizeof(key), TValue());
        return result.first.second;
    }
};

// const char* specialization
template<typename TValue>
class adaptive_radix_tree<const char*, TValue> : base_adaptive_radix_tree<TValue>
{
public:
    typedef base_adaptive_radix_tree<TValue> base_tree;
    typedef base_adaptive_radix_tree<TValue>::iterator iterator;

    adaptive_radix_tree()
        : base_tree()
    {
    }

    size_t size() const
    {
        return base_tree::size();
    }

    iterator end() const
    {
        return iterator();
    }

    iterator find(const char* key) const
    {
        node_leaf* n = base_tree::find(reinterpret_cast<const uint8_t*>(key), strlen(key) + 1);
        return iterator(n);
    }

    std::pair<iterator, bool> insert(const char* key, const TValue& value)
    {
        std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(key), strlen(key) + 1, value);
        return std::pair<iterator, bool>(iterator(result.first), result.second);
    }

    TValue& operator[](const char* key)
    {
        std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(key), strlen(key) + 1, TValue());
        return result.first.second;
    }
};

// const std::string& specialization
template<typename TValue>
class adaptive_radix_tree<std::string, TValue> : base_adaptive_radix_tree<TValue>
{
public:
    typedef base_adaptive_radix_tree<TValue> base_tree;
    typedef base_adaptive_radix_tree<TValue>::iterator iterator;

    adaptive_radix_tree()
        : base_tree()
    {
    }

    size_t size() const
    {
        return base_tree::size();
    }

    iterator end() const
    {
        return iterator();
    }

    iterator find(const std::string& key) const
    {
        node_leaf* n = base_tree::find(reinterpret_cast<const uint8_t*>(key.c_str()), key.size() + 1);
        return iterator(n);
    }

    std::pair<iterator, bool> insert(const std::string& key, const TValue& value)
    {
        std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(key.c_str()), key.size() + 1, value);
        return std::pair<iterator, bool>(iterator(result.first), result.second);
    }

    TValue& operator[](const std::string& key)
    {
        std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(key.c_str()), key.size() + 1, std::string());
        return result.first.second;
    }
};
