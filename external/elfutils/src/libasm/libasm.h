/* Interface for libasm.
   Copyright (C) 2002, 2005, 2008 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef _LIBASM_H
#define _LIBASM_H 1

#include <stdbool.h>
#include <stdint.h>

#include <libebl.h>


typedef struct AsmCtx AsmCtx_t;

typedef struct AsmScn AsmScn_t;

typedef struct AsmScnGrp AsmScnGrp_t;

typedef struct AsmSym AsmSym_t;


typedef struct DisasmCtx DisasmCtx_t;

typedef int (*DisasmGetSymCB_t) (GElf_Addr, Elf32_Word, GElf_Addr, char **,
				 size_t *, void *);

typedef int (*DisasmOutputCB_t) (char *, size_t, void *);


#ifdef __cplusplus
extern "C" {
#endif

extern AsmCtx_t *asm_begin (const char *fname, Ebl *ebl, bool textp);

extern int asm_abort (AsmCtx_t *ctx);

extern int asm_end (AsmCtx_t *ctx);


extern AsmScn_t *asm_newscn (AsmCtx_t *ctx, const char *scnname,
			     GElf_Word type, GElf_Xword flags);


extern AsmScn_t *asm_newscn_ingrp (AsmCtx_t *ctx, const char *scnname,
				   GElf_Word type, GElf_Xword flags,
				   AsmScnGrp_t *grp);

extern AsmScn_t *asm_newsubscn (AsmScn_t *asmscn, unsigned int nr);


extern AsmScnGrp_t *asm_newscngrp (AsmCtx_t *ctx, const char *grpname,
				   AsmSym_t *signature, Elf32_Word flags);

extern int asm_scngrp_newsignature (AsmScnGrp_t *grp, AsmSym_t *signature);


extern int asm_addstrz (AsmScn_t *asmscn, const char *str, size_t len);

extern int asm_addint8 (AsmScn_t *asmscn, int8_t num);

extern int asm_adduint8 (AsmScn_t *asmscn, uint8_t num);

extern int asm_addint16 (AsmScn_t *asmscn, int16_t num);

extern int asm_adduint16 (AsmScn_t *asmscn, uint16_t num);

extern int asm_addint32 (AsmScn_t *asmscn, int32_t num);

extern int asm_adduint32 (AsmScn_t *asmscn, uint32_t num);

extern int asm_addint64 (AsmScn_t *asmscn, int64_t num);

extern int asm_adduint64 (AsmScn_t *asmscn, uint64_t num);


extern int asm_addsleb128 (AsmScn_t *asmscn, int32_t num);

extern int asm_adduleb128 (AsmScn_t *asmscn, uint32_t num);


extern AsmSym_t *asm_newsym (AsmScn_t *asmscn, const char *name,
			     GElf_Xword size, int type, int binding);


extern AsmSym_t *asm_newcomsym (AsmCtx_t *ctx, const char *name,
				GElf_Xword size, GElf_Addr align);

extern AsmSym_t *asm_newabssym (AsmCtx_t *ctx, const char *name,
				GElf_Xword size, GElf_Addr value,
				int type, int binding);


extern int asm_align (AsmScn_t *asmscn, GElf_Word value);

extern int asm_fill (AsmScn_t *asmscn, void *bytes, size_t len);


extern Elf *asm_getelf (AsmCtx_t *ctx);


extern int asm_errno (void);

extern const char *asm_errmsg (int __error);


extern DisasmCtx_t *disasm_begin (Ebl *ebl, Elf *elf, DisasmGetSymCB_t symcb);

extern int disasm_end (DisasmCtx_t *ctx);

extern int disasm_str (DisasmCtx_t *ctx, const uint8_t **startp,
		       const uint8_t *end, GElf_Addr addr, const char *fmt,
		       char **bufp, size_t len, void *symcbarg);

extern int disasm_cb (DisasmCtx_t *ctx, const uint8_t **startp,
		      const uint8_t *end, GElf_Addr addr, const char *fmt,
		      DisasmOutputCB_t outcb, void *outcbarg, void *symcbarg);

#ifdef __cplusplus
}
#endif

#endif	
