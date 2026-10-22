/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Script bytecode (engine: Execute in src/moas.c) and the GAME.PCK text
 * dictionary its strings are packed with. */
#ifndef MU_SCRIPT_H
#define MU_SCRIPT_H

#include <stdint.h>
#include <stdio.h>

/* ---- GAME.PCK -------------------------------------------------------- */

typedef struct {
  uint8_t header;      /* first byte, not part of the dictionary */
  int n;               /* pieces; string byte N stands for piece N */
  char *piece[256];    /* CP850, NUL-terminated */
} Dict;

/* NULL on success, else an error message. */
const char *dict_from_pck(Dict *d, const uint8_t *data, long len);
uint8_t *dict_to_pck(const Dict *d, long *len);
void dict_write_text(FILE *fp, const Dict *d);
const char *dict_parse_text(Dict *d, const char *text, int *line);
void dict_free(Dict *d);

/* Packed string (piece numbers, NUL-terminated) -> CP850 text. */
void dict_unpack(const Dict *d, const uint8_t *packed, char *out, size_t outsz);
/* CP850 text -> packed string, longest piece first (what the original
 * packer did). Returns the packed length, or -1 and *bad = offset of the
 * first character no piece covers. */
int dict_pack(const Dict *d, const uint8_t *text, uint8_t *out, size_t outsz, int *bad);

/* ---- bytecode -------------------------------------------------------- */

typedef struct {
  uint8_t *code;
  int len;
} Prog;

/* Size of the instruction at pc, or -1 if it's not a valid instruction
 * (unknown opcode, or it runs past the end). */
int script_insn_size(const uint8_t *code, int len, int pc);
/* Offset of the packed string operand from the opcode, -1 if none. */
int script_string_offset(uint8_t op);
const char *script_op_name(uint8_t op);

/* Disassemble program idx; progs/nprog give the other programs for
 * cross-program labels. The output assembles back to the same bytes. */
void script_disasm(FILE *fp, int idx, const Prog *progs, int nprog, const Dict *d);

/* Assemble nprog sources (texts[i] for program i, names[i] for messages).
 * Fills progs[i]. Returns the number of errors (printed to stderr). */
int script_assemble(char **texts, const char **names, int nprog, const Dict *d, Prog *progs);

#endif
