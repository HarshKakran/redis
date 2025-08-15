#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

const size_t k_max_load_factor = 8;
const size_t k_rehashing_work = 128;

struct HNode
{
    HNode *next = NULL;
    uint64_t hcode = 0;
};

struct HTab
{
    HNode **tab = NULL;
    size_t size = 0;
    size_t mask = 0;
};

void h_init(HTab *t, size_t n);
HNode **h_lookup(const HTab *t, HNode *key, bool (*eq)(HNode *, HNode *));
void h_insert(HTab *t, HNode *node);
HNode *h_detach(HTab *t, HNode **from);

// HMap using progressive re-hashing
struct HMap
{
    HTab newer;
    HTab older;

    size_t migrate_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_trigger_rehashing(HMap *hmap);
void hm_help_rehashing(HMap *hmap);
void hm_clear(HMap *hmap);
size_t hm_size(HMap *hmap);

void hm_foreach(HMap *hmap, bool (*f)(HNode *, void *), void *args);

#endif