/* object_deps.h - parser-module declarations needed by object.c.
 *
 * parser.h is being extracted in parallel by another agent; per the
 * extraction plan, object.c declares ONLY what it needs here instead of
 * creating parser.h.  Every declaration below mirrors the C++ port's
 * src/parser.h with narrowed widths.
 *
 * TODO(merge into parser.h): reconcile all of this file against the real
 * parser module when it lands.  The extern data symbols are declared flat
 * (parser_TOKEN / parser_STRING); if the parser module keeps them inside a
 * parser_state struct, references in object.c must be renamed (a link
 * error will flag every site - nothing fails silently).
 */
#ifndef DOD_OBJECT_DEPS_H
#define DOD_OBJECT_DEPS_H

#include "dod_types.h"

/* TODO(merge into parser.h): internal-code string terminator */
enum { I_NULL = 0xFF };

/* TODO(merge into parser.h): token/expansion work buffers */
extern dodBYTE parser_TOKEN[33];
extern dodBYTE parser_STRING[35];

/* TODO(merge into parser.h): EXPAND.ASM - unpack one 5-bit packed string.
 * X points at the packed table entry; *Xup returns the byte advance to the
 * next entry; U is the output buffer, or NULL to expand into STRING with
 * the port's 2-byte header offset. */
void parser_EXPAND(const dodBYTE *X, dodBYTE *Xup, dodBYTE *U);

/* TODO(merge into parser.h): PARSER.ASM - match next token against packed
 * table; returns 1 = match (A=entry index, B=entry class), 0 = no token,
 * -1 = token that does not match the table. */
int8_t parser_PARSER(const dodBYTE *TABLE, dodBYTE *A, dodBYTE *B,
                     uint8_t norm);

/* TODO(merge into parser.h): print the '???' command error */
void parser_CMDERR(void);

#endif /* DOD_OBJECT_DEPS_H */
