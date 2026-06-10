#include "core/sema/symbol.h"
#include "core/arena/arena.h"
#include <string.h>
#include <stdio.h>

#define DPP_SYMTAB_INITIAL_SIZE 1024

static u32 s_hash(const u8 *name, size_t len)
{
        u32 hash = 2166136261u;
        for (size_t i = 0; i < len; i++) {
                hash ^= name[i];
                hash *= 16777619u;
        }
        return hash;
}

void dpp_symtab_init(struct dpp_symtab *tab, struct dpp_arena *arena)
{
        tab->tab_arena = arena;
        tab->tab_curr  = NULL;
        dpp_symtab_push(tab); /* Global scope */

        /* Injeta tipos básicos do D++ para facilitar self-hosting */
        const char *builtins[] = {"u8",       "u16",      "u32",    "u64",     "s8",      "s16",
                                  "s32",      "s64",      "size_t", "bool",    "uint8_t", "uint16_t",
                                  "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t"};
        for (int i = 0; i < 18; i++) {
                struct dpp_node *node         = dpp_node_new(arena, NOD_VAR_DECL, 0, 0);
                node->nod_data.nod_id.id_name = (const u8 *)builtins[i];
                node->nod_data.nod_id.id_len  = strlen(builtins[i]);
                node->nod_data.nod_id.id_type = TYPE_INT;
                node->nod_type_flags |= NOD_TYPE_TYPEDEF;
                dpp_symtab_insert(tab, node->nod_data.nod_id.id_name, node->nod_data.nod_id.id_len, node);
        }
}

void dpp_symtab_push(struct dpp_symtab *tab)
{
        struct dpp_scope *sco = dpp_arena_alloc(tab->tab_arena, sizeof(struct dpp_scope));
        sco->sco_parent       = tab->tab_curr;
        sco->sco_bucket_count = DPP_SYMTAB_INITIAL_SIZE;
        sco->sco_buckets      = dpp_arena_alloc(tab->tab_arena, sizeof(struct dpp_symbol *) * sco->sco_bucket_count);
        memset(sco->sco_buckets, 0, sizeof(struct dpp_symbol *) * sco->sco_bucket_count);

        tab->tab_curr = sco;
}

void dpp_symtab_pop(struct dpp_symtab *tab)
{
        if (tab->tab_curr) {
                tab->tab_curr = tab->tab_curr->sco_parent;
        }
}

struct dpp_symbol *dpp_symtab_insert(struct dpp_symtab *tab, const u8 *name, size_t len, struct dpp_node *node)
{
        u32 h = s_hash(name, len) % tab->tab_curr->sco_bucket_count;

        /* Verifica se já existe no escopo ATUAL */
        struct dpp_symbol *existing = tab->tab_curr->sco_buckets[h];
        while (existing) {
                if (existing->sym_len == len && memcmp(existing->sym_name, name, len) == 0) {
                        return existing; /* Retorna o existente para permitir múltiplas declarações idênticas */
                }
                existing = existing->sym_next;
        }

        /* Novo símbolo na Arena */
        struct dpp_symbol *sym = dpp_arena_alloc(tab->tab_arena, sizeof(struct dpp_symbol));

        u8 *name_copy = dpp_arena_alloc(tab->tab_arena, len + 1);
        memcpy(name_copy, name, len);
        name_copy[len] = '\0';

        sym->sym_name                 = name_copy;
        sym->sym_len                  = len;
        sym->sym_node                 = node;
        sym->sym_next                 = tab->tab_curr->sco_buckets[h];
        tab->tab_curr->sco_buckets[h] = sym;

        return sym;
}

struct dpp_symbol *dpp_symtab_lookup(struct dpp_symtab *tab, const u8 *name, size_t len)
{
        u32               h   = s_hash(name, len);
        struct dpp_scope *sco = tab->tab_curr;

        while (sco) {
                u32                bucket_idx = h % sco->sco_bucket_count;
                struct dpp_symbol *sym        = sco->sco_buckets[bucket_idx];
                while (sym) {
                        if (sym->sym_len == len && memcmp(sym->sym_name, name, len) == 0) {
                                return sym;
                        }
                        sym = sym->sym_next;
                }
                sco = sco->sco_parent; /* Procura no escopo pai */
        }
        return NULL;
}

struct dpp_symbol *dpp_symtab_lookup_local(struct dpp_symtab *tab, const u8 *name, size_t len)
{
	if (!tab->tab_curr) return NULL;
	u32                h          = s_hash(name, len);
	struct dpp_scope  *sco        = tab->tab_curr;
	u32                bucket_idx = h % sco->sco_bucket_count;
	struct dpp_symbol *sym        = sco->sco_buckets[bucket_idx];
	while (sym) {
		if (sym->sym_len == len && memcmp(sym->sym_name, name, len) == 0) {
			return sym;
		}
		sym = sym->sym_next;
	}
	return NULL;
}
