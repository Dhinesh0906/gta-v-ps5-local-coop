/* Upper bit of the existing seed-version word; lower bits retain their ABI.
 * Zero-initialized menus default to split-screen ON. No NPC state is changed. */
#ifndef COOP_DISPLAY_PREFERENCE_H
#define COOP_DISPLAY_PREFERENCE_H
#define COOP_SPLIT_DISABLED 0x80000000u
#define COOP_DISPLAY_WORD_OFFSET 0x10624ull
static inline int coop_display_split_enabled(unsigned word){return !(word&COOP_SPLIT_DISABLED);}
#endif
