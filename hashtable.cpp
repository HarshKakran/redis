#include <hashtable.h>

void h_init(HTab *t, size_t n)
{
    assert(n > 0 && (n & (n - 1)) == 0); // n needs to be a power of 2
    t->tab = (HNode **)calloc(n, sizeof(HNode));
    t->mask = n - 1;
    t->size = 0;
}

void h_insert(HTab *t, HNode *node)
{
    size_t pos = node->hcode & t->mask;
    HNode *next = t->tab[pos];
    node->next = next;
    t->tab[pos] = node;
    t->size++;
}

HNode **h_lookup(const HTab *t, HNode *key, bool (*eq)(HNode *, HNode *))
{
    if (!t->tab)
    {
        return NULL;
    }

    size_t pos = key->hcode & t->mask;
    HNode **from = &t->tab[pos]; // incoming pointer to the target
    for (HNode *curr; (curr = *from) != NULL; from = &curr->next)
    {
        if (curr->hcode == key->hcode && eq(curr, key))
        {
            return from; // Q: Why not return `cur`? This is for deletion.
        }
    }

    return NULL;
}

HNode *h_detach(HTab *t, HNode **from)
{
    HNode *node = *from;
    *from = node->next;
    t->size--;
    return node;
}

// Hash Map
void hm_trigger_rehashing(HMap *hmap)
{
    hmap->older = hmap->newer;
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    hm_help_rehashing(hmap); // migrate some keys

    HNode **from = h_lookup(&hmap->newer, key, eq);
    if (!from)
    {
        from = h_lookup(&hmap->older, key, eq);
    }

    return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    hm_help_rehashing(hmap); // migrate some keys

    if (HNode **from = h_lookup(&hmap->newer, key, eq))
    {
        return h_detach(&hmap->newer, from);
    }
    else if (from = h_lookup(&hmap->older, key, eq))
    {
        return h_detach(&hmap->older, from);
    }

    return NULL;
}

void hm_insert(HMap *hmap, HNode *node)
{
    if (!hmap->newer.tab)
    {
        h_init(&hmap->newer, 4);
    }

    h_insert(&hmap->newer, node); // insert in the new table
    if (hmap->older.tab)
    {
        // if older table exist then rehashing is required
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.size >= shreshold)
        {
            hm_trigger_rehashing(hmap);
        }
    }

    hm_help_rehashing(hmap); // migrate some keys
}

void hm_help_rehashing(HMap *hmap)
{
    size_t nwork = 0;

    while (nwork < k_rehashing_work && hmap->older.size > 0)
    {
        // find non-empty slot in the older table
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (!*from)
        { // empty slot
            hmap->migrate_pos++;
            continue;
        }

        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        nwork++;
    }

    if (hmap->older.size == 0 && !hmap->older.tab)
    {
        free(hmap->older.tab);
        hmap->older = HTab{};
    }
}

void hm_clear(HMap *hmap)
{
    free(hmap->newer.tab);
    free(hmap->older.tab);
    *hmap = HMap{};
}

size_t hm_size(HMap *hmap)
{
    return hmap->newer.size + hmap->older.size;
}

static bool h_foreach(HTab *htab, bool (*f)(HNode *, void *), void *arg)
{
    for (size_t i = 0; htab->mask != 0 && i <= htab->mask; i++)
    {
        for (HNode *node = htab->tab[i]; node != NULL; node = node->next)
        {
            if (!f(node, arg))
            {
                return false;
            }
        }
    }
    return true;
}

void hm_foreach(HMap *hmap, bool (*f)(HNode *, void *), void *args)
{
    h_foreach(&hmap->newer, f, args) && h_foreach(&hmap->older, f, args);
}