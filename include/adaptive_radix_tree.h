#pragma once

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

#define ENABLE_ADAPTIVE_RADIX_TREE_STATS 1

template<typename TValue>
class base_adaptive_radix_tree
{
protected:
	// Node type is defined by node pointer low 4 bits.
	struct node_leaf_traits { enum { kPointerEmbeddedType = 0 }; enum { kStatsIndex = 0 }; };
	struct node_4_traits    { enum { kPointerEmbeddedType = 1 }; enum { kStatsIndex = 1 }; enum { kMaxChildrenCount = 4   }; };
	struct node_16_traits   { enum { kPointerEmbeddedType = 2 }; enum { kStatsIndex = 2 }; enum { kMaxChildrenCount = 16  }; };
	struct node_48_traits   { enum { kPointerEmbeddedType = 4 }; enum { kStatsIndex = 3 }; enum { kMaxChildrenCount = 48  }; };
	struct node_256_traits  { enum { kPointerEmbeddedType = 8 }; enum { kStatsIndex = 4 }; enum { kMaxChildrenCount = 256 }; };

	enum
	{
		kPointerEmbeddedTypeMask = 15
	};

	enum
	{
		kNodeAlignment = 16
	};

	
	struct node;
	typedef node* node_ptr;

	// Node type is stored as lower 4 bits of node pointer.
	// Thus node must be always aligned by 16 bytes!
	struct node_ptr_with_type
	{
		node_ptr_with_type()
			: ptr(NULL)
		{}

		node_ptr_with_type(const node_ptr_with_type& other)
			: ptr(other.ptr)
		{}

		template<typename TNode>
		node_ptr_with_type(TNode* n)
			: ptr(reinterpret_cast<node_ptr>((reinterpret_cast<size_t>(n) | TNode::traits::kPointerEmbeddedType)))
		{}

		node_ptr get_raw_node() const { return reinterpret_cast<node_ptr>((reinterpret_cast<size_t>(ptr) & ~kPointerEmbeddedTypeMask)); }
		size_t get_node_type() const { return (reinterpret_cast<size_t>(ptr) & kPointerEmbeddedTypeMask); }

		template<typename TNode>
		bool is_node_type() const
		{
			return (reinterpret_cast<size_t>(ptr) & TNode::traits::kPointerEmbeddedType) != 0;
		}

		bool is_node_leaf() const
		{
			return (reinterpret_cast<size_t>(ptr) & kPointerEmbeddedTypeMask) == node_leaf::traits::kPointerEmbeddedType;
		}

		template<typename TNode>
		TNode* get_node() const
		{
			size_t p = reinterpret_cast<size_t>(ptr) & ~kPointerEmbeddedTypeMask;
			return reinterpret_cast<TNode*>(p);
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
		node(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen)
			: prefixLength(keyLen)
			, childrenCount(0)
			, parent(p)
		{
			memcpy(prefix, key, keyLen);
		}

		enum { kMaxPrefixLength = 6 };

		uint8_t prefixLength;
		uint8_t prefix[kMaxPrefixLength];
		uint8_t childrenCount;
		node_ptr_with_type parent;
	};

	// Simple indexed node template.
	// Dictionary-like storage of child nodes.
	// L1 cache friendly with possible sorting for larger nodes.
	template<typename TNodeTraits>
	struct node_indexed : node
	{
		typedef TNodeTraits traits;

		node_indexed(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen)
			: node(p, key, keyLen)
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
	typedef node_indexed<node_4_traits>  node_4;
	typedef node_indexed<node_16_traits> node_16;

	// 48 children accessible through 256 indexer.
	struct node_48 : node
	{
		typedef node_48_traits traits;

		node_48(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen)
			: node(p, key, keyLen)
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

	// Full inner node.
	// 256 children accessible through indexing.
	struct node_256 : node
	{
		typedef node_256_traits traits;

		node_256(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen)
			: node(p, key, keyLen)
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

		void add_child(node_ptr_with_type n, uint8_t k)
		{
			children[k] = n;
			childrenCount++;
		}

		node_ptr_with_type children[traits::kMaxChildrenCount];
	};

	// Leaf node. Only value.
	struct node_leaf : node
	{
		typedef node_leaf_traits traits;

		node_leaf(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen, const TValue& v)
			: node(p, key, keyLen)
			, value(v)
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

	base_adaptive_radix_tree()
		: root_()
		, size_(0)
	{
#ifdef ENABLE_ADAPTIVE_RADIX_TREE_STATS
		memset(&node_stats_, 0, sizeof(node_stats_));
#endif // ENABLE_ADAPTIVE_RADIX_TREE_STATS
	}

	size_t size() const
	{
		return size_;
	}

	void clear()
	{
		size_ = 0;
#ifdef ENABLE_ADAPTIVE_RADIX_TREE_STATS
		memset(&node_stats_, 0, sizeof(node_stats_));
#endif // ENABLE_ADAPTIVE_RADIX_TREE_STATS
	}

	enum FindResult
	{
		kFoundLeaf,
		kFoundParent,
		kFoundNodeToSplit,
	};

	FindResult find_any(const uint8_t* key, size_t keyLen, node_ptr_with_type& result, size_t& usedKey) const
	{
		assert(keyLen != 0);

		// Starting from root
		node_ptr_with_type current_node_with_type = root_;
		while (current_node_with_type.is_valid())
		{
			node_ptr n = current_node_with_type;
			const size_t prefixLen = n->prefixLength;
			// Process prefix
			if (prefixLen != 0)
			{
				// If prefix is larger than key part, no match
				if (prefixLen > keyLen) // TODO: unlikely
					return current_node_with_type;

				// Check if prefix matches key part
				if (memcmp(n->prefix, key, prefixLen) != 0)
					return current_node_with_type;

				key += prefixLen;
				keyLen -= prefixLen;
			}

			// If no key left, current node is the match
			if (keyLen == 0)
				return current_node_with_type;

			// Continue descending
			current_node_with_type = get_child(current_node_with_type, *key);

			// Jumping to the child means we use a character from a key
			key++;
			keyLen--;
		}

		return current_node_with_type;
	}

	node_leaf* find_leaf(const uint8_t* key, size_t keyLen) const
	{
		assert(keyLen != 0);

		// Starting from root
		node_ptr_with_type current_node_with_type = root_;
		while (current_node_with_type.is_valid())
		{
			node_ptr n = current_node_with_type;
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

			// If no key left, current node is the match
			if (keyLen == 0)
				return current_node_with_type.is_node_leaf() ? current_node_with_type.get_node<node_leaf>() : NULL;;

			// Continue descending
			current_node_with_type = get_child(current_node_with_type, *key);

			// Jumping to the child means we use a character from a key
			key++;
			keyLen--;
		}

		return NULL;
	}

	std::pair<node_leaf*, bool> insert(const uint8_t* key, size_t keyLen, const TValue& value)
	{
		assert(keyLen != 0);

		if (!root_.is_valid()) // TODO: unlikely
		{
			node_leaf* nleaf = add_leaf(&root_, key, keyLen, value);
			return std::make_pair(nleaf, true);
		}

		// Starting from root
		node_ptr_with_type* current_node_with_type = &root_;
		for (;;)
		{
			node* n = *current_node_with_type;

			// If prefix is larger than key part, no match
			const uint8_t prefixLen = n->prefixLength;
			// Process prefix
			if (prefixLen != 0)
			{
				// Find out if there is common prefix for old node and new one
				uint8_t different_key_pos = (uint8_t)get_different_key_position(key, keyLen, n->prefix, n->prefixLength);
				if (different_key_pos < prefixLen)
				{
					// Key prefix has a partial match, meaning this node should be split
					return std::make_pair<node_leaf*, bool>(add_leaf_with_split(current_node_with_type, key, keyLen, different_key_pos, value), true);
				}

				// Overwise continue
				key += prefixLen;
				keyLen -= prefixLen;
			}

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

			// And continue descending
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
	// Construct a leaf node with the specific parent and key (which should be <= node::kMaxPrefixLength)
	// Setting parent here is faster than in add_child as we don't need to convert node_ptr_with_type to node_ptr.
	static node_leaf* construct_raw_leaf(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen, const TValue& value)
	{
		assert(keyLen <= node::kMaxPrefixLength);

		// TODO: include valueAlignment into account
		// TODO: Block allocator
		// Allocate aligned node
		node_leaf* n = reinterpret_cast<node_leaf*>(_aligned_malloc(sizeof(node_leaf), kNodeAlignment));
		assert((reinterpret_cast<size_t>(n) & kPointerEmbeddedTypeMask) == 0);
		// Construct
		new (n) node_leaf(p, key, keyLen, value);
		return n;
	}

	template<typename TNode>
	static TNode* construct_raw_node_n(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen)
	{
		assert(keyLen <= node::kMaxPrefixLength);

		// TODO: Block allocator
		// Allocate aligned node
		TNode* n = reinterpret_cast<TNode*>(_aligned_malloc(sizeof(TNode), kNodeAlignment));
		assert((reinterpret_cast<size_t>(n) & kPointerEmbeddedTypeMask) == 0);
		// Construct
		new (n) TNode(p, key, keyLen);
		return n;
	}

	template<typename TNode> struct grow_policy {};
	template<typename TNode> struct shrink_policy {};

	template<> struct grow_policy<node_4>
	{
		typedef node_16 grow_type; // We don't really need it with auto, but doing this for pre c++11 support
		static node_16* copy_construct_node(const node_4& ncopy)
		{
			node_16* n = construct_raw_node_n<node_16>(ncopy.parent, ncopy.prefix, ncopy.prefixLength);
			n->childrenCount = ncopy.childrenCount;
			memcpy(n->keys, ncopy.keys, ncopy.childrenCount);
			memcpy(n->children, ncopy.children, ncopy.childrenCount * sizeof(ncopy.children[0]));

			return n;
		}
	};

	template<> struct shrink_policy<node_16>
	{
		typedef node_4 shrink_type;
		static shrink_type* copy_construct_node(const node_16& ncopy)
		{
			node_4* n = construct_raw_node_n<node_4>(ncopy.parent, ncopy.prefix, ncopy.prefixLength);
			n->childrenCount = ncopy.childrenCount;
			memcpy(n->keys, ncopy.keys, ncopy.childrenCount);
			memcpy(n->children, ncopy.children, ncopy.childrenCount * sizeof(ncopy.children[0]));

			return n;
		}
	};
	template<> struct grow_policy<node_16>
	{
		typedef node_48 grow_type;
		static node_48* copy_construct_node(const node_16& ncopy)
		{
			node_48* n = construct_raw_node_n<node_48>(ncopy.parent, ncopy.prefix, ncopy.prefixLength);
			n->childrenCount = ncopy.childrenCount;
			for (uint8_t i = 0; i < ncopy.childrenCount; ++i)
			{
				n->keys[ncopy.keys[i]] = i;
				n->children[i] = ncopy.children[i];
			}

			return n;
		}
	};

	template<> struct shrink_policy<node_48>
	{
		typedef node_16 shrink_type;
		static node_16* copy_construct_node(const node_48& ncopy)
		{
			node_16* n = construct_raw_node_n<node_16>(ncopy.parent, ncopy.prefix, ncopy.prefixLength);
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
		typedef node_256 grow_type;
		static node_256* copy_construct_node(const node_48& ncopy)
		{
			node_256* n = construct_raw_node_n<node_256>(ncopy.parent, ncopy.prefix, ncopy.prefixLength);
			n->childrenCount = ncopy.childrenCount;
			for (size_t i = 0; i < sizeof(node_48::keys); ++i)
			{
				if (ncopy.keys[i] == node_48::kInvalidIndex)
					continue;

				n->children[i] = ncopy.children[ncopy.keys[i]];
			}

			return n;
		}
	};

	template<> struct shrink_policy<node_256>
	{
		typedef node_48 shrink_type;
		static node_48* copy_construct_node(const node_256& ncopy)
		{
			node_48* n = construct_raw_node_n<node_48>(ncopy.parent, ncopy.prefix, ncopy.prefixLength);
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

	template<typename TNode>
	static void destruct_node_n(TNode* n)
	{
		_aligned_free(n);
	}

	static node_4* construct_raw_node_4(node_ptr_with_type p, const uint8_t* key, uint8_t keyLen) { return construct_raw_node_n<node_4>(p, key, keyLen); }

	// Return k child of the node n.
	// Return k child of the node n.
	static node_ptr_with_type get_child(node_ptr_with_type n, uint8_t k)
	{
		assert(!n.is_node_leaf());

		return n.is_node_type<node_4>() ? n.get_node<node_4>()->get_child(k) :
			n.is_node_type<node_16>() ? n.get_node<node_16>()->get_child(k) :
			n.is_node_type<node_48>() ? n.get_node<node_48>()->get_child(k) :
			n.get_node<node_256>()->get_child(k);
	}
	static node_ptr_with_type* get_child_ptr(node_ptr_with_type n, uint8_t k)
	{
		assert(!n.is_node_leaf());

		return n.is_node_type<node_4>() ? n.get_node<node_4>()->get_child_ptr(k) :
			n.is_node_type<node_16>() ? n.get_node<node_16>()->get_child_ptr(k) :
			n.is_node_type<node_48>() ? n.get_node<node_48>()->get_child_ptr(k) :
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
		TNodeGrow* ngrown = grow_policy<TNode>::copy_construct_node(*ntyped);
		ngrown->add_child(childptr, key);
		// Delete old node
		destruct_node_n(ntyped);
#ifdef ENABLE_ADAPTIVE_RADIX_TREE_STATS
		node_stats_[TNodeGrow::traits::kStatsIndex]++;
		node_stats_[TNode::traits::kStatsIndex]--;
#endif // ENABLE_ADAPTIVE_RADIX_TREE_STATS

		// Overwrite old reference
		*nptr = ngrown;

		return true;
	}

	// Add child to the nptr node with conversion if necessary.
	void add_child(node_ptr_with_type* nbox, node_ptr_with_type childptr, uint8_t key)
	{
		assert(!nbox->is_node_leaf());

		if (!try_add_child<node_4>(nbox, childptr, key))
		{
			if (!try_add_child<node_16>(nbox, childptr, key))
			{
				if (!try_add_child<node_48>(nbox, childptr, key))
				{
					nbox->get_node<node_256>()->add_child(childptr, key);
				}
			}
		}
	}

	// Add leaf node.
	node_leaf* add_leaf(node_ptr_with_type* parent_node, const uint8_t* key, size_t keyLen, const TValue& value)
	{
		node_ptr parent = parent_node != NULL ? parent_node->get_raw_node() : NULL;

		// In case the key is large than prefix length, we create intermediate nodes which can contain the full key.
		// The nodes can hold all the key, but leave one character for the leaf
		node_4* lastSplitNode4 = NULL;
		while (keyLen > node::kMaxPrefixLength + 1)
		{
			node_4* newSplitNode;
			if (lastSplitNode4 != NULL)
			{
				newSplitNode = construct_raw_node_4(lastSplitNode4, key + 1, node::kMaxPrefixLength);
				lastSplitNode4->add_child(newSplitNode, *key);

				key += node::kMaxPrefixLength + 1;
				keyLen -= node::kMaxPrefixLength + 1;
			}
			else if (parent != NULL)
			{
				newSplitNode = construct_raw_node_4(*parent_node, key + 1, node::kMaxPrefixLength);
				add_child(parent_node, newSplitNode, *key);

				key += node::kMaxPrefixLength + 1;
				keyLen -= node::kMaxPrefixLength + 1;
			}
			else
			{
				newSplitNode = construct_raw_node_4(node_ptr_with_type(), key, node::kMaxPrefixLength);
				*parent_node = newSplitNode;

				key += node::kMaxPrefixLength;
				keyLen -= node::kMaxPrefixLength;
			}

#ifdef ENABLE_ADAPTIVE_RADIX_TREE_STATS
			node_stats_[node_4_traits::kStatsIndex]++;
#endif // ENABLE_ADAPTIVE_RADIX_TREE_STATS

			lastSplitNode4 = newSplitNode;
		}

		node_leaf* leafNode;
		if (lastSplitNode4 != NULL)
		{
			leafNode = construct_raw_leaf(lastSplitNode4, key + 1, (uint8_t)(keyLen - 1), value);
			lastSplitNode4->add_child(leafNode, *key);
		}
		else if (parent != NULL)
		{
			leafNode = construct_raw_leaf(*parent_node, key + 1, (uint8_t)(keyLen - 1), value);
			add_child(parent_node, leafNode, *key);
		}
		else
		{
			leafNode = construct_raw_leaf(node_ptr_with_type(), key, (uint8_t)keyLen, value);
			*parent_node = leafNode;
		}
#ifdef ENABLE_ADAPTIVE_RADIX_TREE_STATS
		node_stats_[node_leaf_traits::kStatsIndex]++;
#endif // ENABLE_ADAPTIVE_RADIX_TREE_STATS

		size_++;

		return leafNode;
	}

	// Split node_to_split node.
	// Create node_4 which replaces node_to_split. New node points to node_to_split and new leaf node with the specified key.
	node_leaf* add_leaf_with_split(node_ptr_with_type* node_to_split, const uint8_t* key, size_t keyLen, uint8_t different_key_pos, const TValue& value)
	{
		node_ptr n = *node_to_split;

		// Create a split new node with common prefix
		node_4* newSplitNode = construct_raw_node_4(n->parent, key, different_key_pos);
#ifdef ENABLE_ADAPTIVE_RADIX_TREE_STATS
		node_stats_[node_4_traits::kStatsIndex]++;
#endif // ENABLE_ADAPTIVE_RADIX_TREE_STATS

		// Move the old node to the new node as a child
		n->parent = newSplitNode;
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
	static size_t get_different_key_position(const uint8_t* __restrict key1, size_t key1Len, const uint8_t* __restrict key2, size_t key2Len)
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
	// Tree depth, maximum key length
	size_t depth_;
	//allocator alloc;

#ifdef ENABLE_ADAPTIVE_RADIX_TREE_STATS
	size_t node_stats_[5];
#endif // ENABLE_ADAPTIVE_RADIX_TREE_STATS
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
		node_leaf* n = base_tree::find_leaf(reinterpret_cast<const uint8_t*>(&key), sizeof(key));
		return iterator(n);
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

	std::pair<iterator, bool> insert(const char* key, const TValue& value)
	{
		std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(key), strlen(key) + 1, value);
		return std::pair<iterator, bool>(iterator(result.first), result.second);
	}

	iterator find(const char* key) const
	{
		node_leaf* n = base_tree::find_leaf(reinterpret_cast<const uint8_t*>(key), strlen(key) + 1);
		return iterator(n);
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

	std::pair<iterator, bool> insert(const std::string& key, const TValue& value)
	{
		std::pair<node_leaf*, bool> result = base_tree::insert(reinterpret_cast<const uint8_t*>(key.c_str()), key.size() + 1, value);
		return std::pair<iterator, bool>(iterator(result.first), result.second);
	}

	iterator find(const std::string& key) const
	{
		node_leaf* n = base_tree::find_leaf(reinterpret_cast<const uint8_t*>(key.c_str()), key.size() + 1);
		return iterator(n);
	}
};
