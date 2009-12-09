/*
 * Copyright (c) 2006, 2007 Christoph Schwering <schwering@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Implementation of B+-Tree for indexing.
 *
 * Some words about the internal implementation: Each node is stored in a 
 * block of memory whose size is a multiple of BLK_SIZE.
 * A block in RAM is seen as a char pointer of exactly this size. The first 
 * bytes of this buffer are used for general information, i.e. the node's 
 * type (LEAF or INNER node) and the count of sons of the node, which is 
 * floor(ORDER/2) <= cnt <= ORDER. Note, by the way, that ORDER is always 
 * an odd number. The remaining bytes are mainly used for storing keys and
 * addresses, except some trailing bytes for the alignment. A node can save
 * up to ORDER pairs of keys and addresses, where addresses in INNER nodes
 * point to a node in the next level of the tree (blkaddr_t) and addresses
 * in LEAF nodes are a tuple address in the tuple file (blkaddr_t). 
 * In this B+-Tree's terminology, the addresses are called pointers 
 * and accessed with the PTR macro; keys are accessed with the KEY macro. 
 * Each node with count cnt sons, floor(ORDER/2) <= cnt <= ORDER, has cnt 
 * PTR/KEY pairs whose indexes are 0, ..., cnt-1. All entries in the subtree 
 * of the i-th entry are less or equal than the i-th key; and the subtree's 
 * node is the node at i-th pointer address, of course.
 * This only applies to nodes that are active (i.e. not deleted). Deleted
 * nodes store no information except a pointer to its predecessor in the list 
 * of deleted nodes which is accessed with PREV_DEL (stands for previously 
 * deleted). The address of the latest deleted node is stored in the index 
 * structure's ix_avail field, by default INVALID_ADDR.
 *
 * Note that this B+-Tree does not allow inserting one key twice. Thus, 
 * each key is unique. This makes this B+-Tree useful for primary indexes.
 * To use it as secondary index, just store the tuple's address to which 
 * the key refers in the key itself, too. (Do not forget to tell your cmpf()
 * function to compare not just keys, but also the tuple addresses.) This is
 * ensures that each key is unique.
 *
 * The B+-Tree algorithms are influenced by
 * Cormen et al.. Algorithmen - Eine Einfuehrung. Section 18: B-Baeume,
 * pp. 439 - 459
 */

#ifndef __BTREE_H__
#define __BTREE_H__

#include "block.h"
#include "cache.h"
#include "constants.h"

struct index {
	char		ix_name[PATH_MAX+1]; /* file name */
	int		ix_fd;		/* file descriptor */
	int		(*ix_cmpf)(const char *, const char *, size_t);
	size_t		ix_size;	/* needed size for data */
	size_t		ix_blksize;	/* real, aligned size (block size) */
	short		ix_order;	/* the order of the B+-tree */
	char		*ix_buf;	/* node buffer */
	blkaddr_t	ix_root;	/* address of root node */
	blkaddr_t	ix_max;		/* maximum addressed block in file */
	blkaddr_t	ix_avail;	/* last deleted node address */
	struct cache	*ix_cache;	/* IO cache structure */
};

struct ix_iter {
	struct index	*it_ix;		/* parent index structure */
	short		it_curindex;	/* current index in node */
	int		it_curcmpval;	/* result of cmpf(key, KEY(curindex) */
	char		*it_key;	/* searched key */
	char		*it_buf;	/* current read node */
	blkaddr_t	it_origaddr;	/* needed for ix_iterator_reset() */
	short		it_origindex;	/* needed for ix_iterator_reset() */
	int		it_origcmpval;	/* needed for ix_iterator_reset() */
};

/* Creates a new B+-Tree index.
 * The ix_name argument must be the filename of the B+-Tree. The ix_size 
 * argument must be the size of a key in the B+-Tree. The cmpf argument
 * must point to a function that  compares to keys and returns -1, 0, +1 if
 * the first is smaller, equal, greater than the second key. If cmpf is NULL,
 * memcmp is used as default.  */
struct index *ix_create(const char *ix_name, size_t ix_size,
		int (*cmpf)(const char *, const char *, size_t));

/* Opens an existing B+-Tree index.
 * The ix_name argument must be the filename of the B+-Tree. The cmpf argument
 * must point to a function that  compares to keys and returns -1, 0, +1 if
 * the first is smaller, equal, greater than the second key. If cmpf is NULL,
 * memcmp is used as default.  */
struct index *ix_open(const char *ix_name, 
		int (*cmpf)(const char *, const char *, size_t));

/* Closes a B+-Tree index and returns to indicate success. Invoking this 
 * function is imported as it saves the current root node address. */
bool ix_close(struct index *ix);

/* Finds the tuple address (blkaddr_t) that is equivalent to the specified key.
 * Returns the found address or INVALID_ADDR if no matching entry was found. 
 * Note that your B+-Tree's key-comparison-function cmpf must accept the
 * specified key. */
blkaddr_t ix_search(struct index *ix, const char *key);

/* Returns an iterator that points to the first found element that is (a) equal
 * to `key', or (b) greater than `key'. First preference is (a), of course,
 * second is (b).
 * If no such tuple is found, the iterator will point behind the greatest 
 * element in the index.
 * NULL is only returned if any IO-errors occured which might be the reason
 * of either a corrupt B+-tree or of implementation errors. */
struct ix_iter *ix_iterator(struct index *ix, const char *key);

/* The ix_min() and ix_max() functions return an iterator that points to the
 * smallest respectively behind greatest value in the index, i.e. exactly at
 * outermost left respectively behind the outermost right value. 
 * Note that this makes ix_lnext() and ix_rnext() work intuitively right with
 * ix_min() and ix_max(), i.e. a combination of ix_min() and ix_rnext() 
 * respectively ix_max() and ix_lnext() iterate over the complete index.
 * If the tree is empty, the ix_lnext() and ix_rnext() functions will inform
 * you because they'll immediately return INVALID_ADDR; The ix_min() and
 * ix_max() functions don't tell you. */
struct ix_iter *ix_min(struct index *ix);
struct ix_iter *ix_max(struct index *ix);

/* Frees an iterator structure and its buffer and key. */
void ix_iter_free(struct ix_iter *iter);

/* Resets an iterator to the position where it started from. */
void ix_reset(struct ix_iter *iter);

/* Calls to ix_next() return the block addresses of those tuples that are 
 * equivalent to the `key' specified in ix_iterator()-call. */
blkaddr_t ix_next(struct ix_iter *iter);

/* The ix_lnext() and ix_rnext() move the iterator to the left respectively
 * right.
 * The order of steps is contrary in ix_lnext() and ix_rnext(): Say i is 
 * the current position of the iterator. ix_lnext() firstly sets i := i - 1
 * and then returns the i-th element. ix_rnext() firstly determines the 
 * i-th element, then sets i := i + 1 and then returns the element that 
 * was determined before incrementing i.
 * This means that subsequent ix_lnext() and ix_rnext() neutralize another.
 * Nevertheless it is no good idea to mix ix_lnext() and ix_rnext() calls,
 * because this might run into errors if ix_lnext() or ix_rnext() reached
 * the left respectively right end of the leafs. */
blkaddr_t ix_lnext(struct ix_iter *iter);
const void *ix_lval(const struct ix_iter *iter);
blkaddr_t ix_rnext(struct ix_iter *iter);
const void *ix_rval(const struct ix_iter *iter);

/* Inserts a new entry key/tuple_addr in the B+Tree. The argument key must 
 * point to a memory block which has at least the size of the keys in the 
 * B+-Tree, because exactly this amount of bytes is copied as key into the 
 * B+-Tree. Returns true to indicate success. If the key already exists in 
 * the B+-Tree, the function returns true, too! If false is returned, the
 * B+-Tree has probably become inconsistent. The reason for a failure is most
 * probably an disk IO error. */
bool ix_insert(struct index *ix, blkaddr_t tuple_addr, const char *key);

/* Deletes an entry that matches key in the B+Tree. The argument key must 
 * point to a memory block which has at least the size of the keys in the 
 * B+-Tree, because exactly this amount of bytes is copied as key into the 
 * B+-Tree. Returns true to indicate success. Success means that an entry 
 * matching the key was either found and deleted or simply not found in the 
 * tree. If false is returned, the B+Tree has probably become inconsistent.
 * The reason for a failure is most probably an disk IO error. */
blkaddr_t ix_delete(struct index *ix, const char *key);

#ifndef NDEBUG
/* Debugging functions that print respectively draw the B+-Tree. */
void ix_print(struct index *ix, const char *fn);
void ix_draw(struct index *ix, const char *fn);
#endif

#endif

