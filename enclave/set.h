#ifndef _SET_H__
#define _SET_H__

#include "types.h"


typedef uint64_t (*set_hash_function) (const char *key);

typedef struct  {
    char* _key;
    uint64_t _hash;
} set_node;

typedef struct  {
    set_node **nodes;
    uint64_t number_nodes;
    uint64_t used_nodes;
    set_hash_function hash_function;
} set_t;



/*  Initialize the set either with default parameters (hash function and space)
    or optionally set the set with specifed values

    Returns:
        SET_MALLOC_ERROR: If an error occured setting up the memory
        SET_TRUE: On success
*/
int set_init_alt(set_t *set, uint64_t num_els, set_hash_function hash);
static inline int set_init(set_t *set) {
    return set_init_alt(set, 1024, NULL);
}

/* Utility function to clear out the set */
int set_clear(set_t *set);

/* Free all memory that is part of the set */
int set_destroy(set_t *set);

/*  Add element to set

    Returns:
        SET_TRUE if added
        SET_ALREADY_PRESENT if already present
        SET_CIRCULAR_ERROR if set is completely full
        SET_MALLOC_ERROR if unable to grow the set
    NOTE: SET_CIRCULAR_ERROR should never happen, but is there for insurance!
*/
int set_add(set_t *set, const char *key);

/*  Remove element from the set

    Returns:
        SET_TRUE if removed
        SET_FALSE if not present
*/
int set_remove(set_t *set, const char *key);

/*  Check if key in set

    Returns:
        SET_TRUE if present,
        SET_FALSE if not found
        SET_CIRCULAR_ERROR if set is full and not found
    NOTE: SET_CIRCULAR_ERROR should never happen, but is there for insurance!
*/
int set_contains(set_t *set, const char *key);

/* Return the number of elements in the set */
uint64_t set_length(set_t *set);

/*  Set res to the union of s1 and s2
    res = s1 ∪ s2

    The union of a set A with a B is the set of elements that are in either
    set A or B. The union is denoted as A ∪ B
*/
int set_union(set_t *res, set_t *s1, set_t *s2);

/*  Set res to the intersection of s1 and s2
    res = s1 ∩ s2

    The intersection of a set A with a B is the set of elements that are in
    both set A and B. The intersection is denoted as A ∩ B
*/
int set_intersection(set_t *res, set_t *s1, set_t *s2);

/*  Set res to the difference between s1 and s2
    res = s1∖ s2

    The set difference between two sets A and B is written A ∖ B, and means
    the set that consists of the elements of A which are not elements
    of B: x ∈ A ∖ B ⟺ x ∈ A ∧ x ∉ B. Another frequently seen notation
    for S ∖ T is S − T.
*/
int set_difference(set_t *res, set_t *s1, set_t *s2);

/*  Set res to the symmetric difference between s1 and s2
    res = s1 △ s2

    The symmetric difference of two sets A and B is the set of elements either
    in A or in B but not in both. Symmetric difference is denoted
    A △ B or A * B
*/
int set_symmetric_difference(set_t *res, set_t *s1, set_t *s2);

/*  Return SET_TRUE if test is fully contained in s2; returns SET_FALSE
    otherwise
    test ⊆ against

    A set A is a subset of another set B if all elements of the set A are
    elements of the set B. In other words, the set A is contained inside
    the set B. The subset relationship is denoted as A ⊆ B
*/
int set_is_subset(set_t *test, set_t *against);

/*  Inverse of subset; return SET_TRUE if set test fully contains
    (including equal to) set against; return SET_FALSE otherwise
    test ⊇ against

    Superset Definition: A set A is a superset of another set B if all
    elements of the set B are elements of the set A. The superset
    relationship is denoted as A ⊇ B
*/
static inline int set_is_superset(set_t *test, set_t *against) {
    return set_is_subset(against, test);
}

/*  Strict subset ensures that the test is a subset of against, but that
    the two are also not equal.
    test ⊂ against

    Set A is a strict subset of another set B if all elements of the set A
    are elements of the set B. In other words, the set A is contained inside
    the set B. A ≠ B is required. The strict subset relationship is denoted
    as A ⊂ B
*/
int set_is_subset_strict(set_t *test, set_t *against);

/*  Strict superset ensures that the test is a superset of against, but that
    the two are also not equal.
    test ⊃ against

    Strict Superset Definition: A set A is a superset of another set B if
    all elements of the set B are elements of the set A. A ≠ B is required.
    The superset relationship is denoted as A ⊃ B
*/
static inline int set_is_superset_strict(set_t *test, set_t *against) {
    return set_is_subset_strict(against, test);
}

/*  Return an array of the elements in the set
    NOTE: Up to the caller to free the memory */
char** set_to_array(set_t *set, uint64_t *size);

/*  Compare two sets for equality (size, keys same, etc)

    Returns:
        SET_RIGHT_GREATER if left is less than right
        SET_LEFT_GREATER if right is less than left
        SET_EQUAL if left is the same size as right and keys match
        SET_UNEQUAL if size is the same but elements are different
*/
int set_cmp(set_t *left, set_t *right);

#define SET_TRUE 0
#define SET_FALSE -1
#define SET_MALLOC_ERROR -2
#define SET_CIRCULAR_ERROR -3
#define SET_OCCUPIED_ERROR -4
#define SET_ALREADY_PRESENT 1

#define SET_RIGHT_GREATER 3
#define SET_LEFT_GREATER 1
#define SET_EQUAL 0
#define SET_UNEQUAL 2

#endif
