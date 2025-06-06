/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * bpf_jit.h: BPF JIT compiler for PPC
 *
 * Copyright 2011 Matt Evans <matt@ozlabs.org>, IBM Corporation
 * 	     2016 Naveen N. Rao <naveen.n.rao@linux.vnet.ibm.com>
 */
#ifndef _BPF_JIT_H
#define _BPF_JIT_H

#ifndef __ASSEMBLY__

#include <asm/types.h>
#include <asm/ppc-opcode.h>
#include <linux/build_bug.h>

#ifdef CONFIG_PPC64_ELF_ABI_V1
#define FUNCTION_DESCR_SIZE	24
#else
#define FUNCTION_DESCR_SIZE	0
#endif

#define CTX_NIA(ctx) ((unsigned long)ctx->idx * 4)

#define SZL			sizeof(unsigned long)
#define BPF_INSN_SAFETY		64

#define PLANT_INSTR(d, idx, instr)					      \
	do { if (d) { (d)[idx] = instr; } idx++; } while (0)
#define EMIT(instr)		PLANT_INSTR(image, ctx->idx, instr)

/* Long jump; (unconditional 'branch') */
#define PPC_JMP(dest)							      \
	do {								      \
		long offset = (long)(dest) - CTX_NIA(ctx);		      \
		if ((dest) != 0 && !is_offset_in_branch_range(offset)) {		      \
			pr_err_ratelimited("Branch offset 0x%lx (@%u) out of range\n", offset, ctx->idx);			\
			return -ERANGE;					      \
		}							      \
		EMIT(PPC_RAW_BRANCH(offset));				      \
	} while (0)

/* "cond" here covers BO:BI fields. */
#define PPC_BCC_SHORT(cond, dest)					      \
	do {								      \
		long offset = (long)(dest) - CTX_NIA(ctx);		      \
		if ((dest) != 0 && !is_offset_in_cond_branch_range(offset)) {		      \
			pr_err_ratelimited("Conditional branch offset 0x%lx (@%u) out of range\n", offset, ctx->idx);		\
			return -ERANGE;					      \
		}							      \
		EMIT(PPC_INST_BRANCH_COND | (((cond) & 0x3ff) << 16) | (offset & 0xfffc));					\
	} while (0)

/*
 * Sign-extended 32-bit immediate load
 *
 * If this is a dummy pass (!image), account for
 * maximum possible instructions.
 */
#define PPC_LI32(d, i)		do {					      \
	if (!image)							      \
		ctx->idx += 2;						      \
	else {								      \
		if ((int)(uintptr_t)(i) >= -32768 &&			      \
				(int)(uintptr_t)(i) < 32768)		      \
			EMIT(PPC_RAW_LI(d, i));				      \
		else {							      \
			EMIT(PPC_RAW_LIS(d, IMM_H(i)));			      \
			if (IMM_L(i))					      \
				EMIT(PPC_RAW_ORI(d, d, IMM_L(i)));	      \
		}							      \
	} } while (0)

#ifdef CONFIG_PPC64
/* If dummy pass (!image), account for maximum possible instructions */
#define PPC_LI64(d, i)		do {					      \
	if (!image)							      \
		ctx->idx += 5;						      \
	else {								      \
		if ((long)(i) >= -2147483648 &&				      \
				(long)(i) < 2147483648)			      \
			PPC_LI32(d, i);					      \
		else {							      \
			if (!((uintptr_t)(i) & 0xffff800000000000ULL))	      \
				EMIT(PPC_RAW_LI(d, ((uintptr_t)(i) >> 32) &   \
						0xffff));		      \
			else {						      \
				EMIT(PPC_RAW_LIS(d, ((uintptr_t)(i) >> 48))); \
				if ((uintptr_t)(i) & 0x0000ffff00000000ULL)   \
					EMIT(PPC_RAW_ORI(d, d,		      \
					  ((uintptr_t)(i) >> 32) & 0xffff));  \
			}						      \
			EMIT(PPC_RAW_SLDI(d, d, 32));			      \
			if ((uintptr_t)(i) & 0x00000000ffff0000ULL)	      \
				EMIT(PPC_RAW_ORIS(d, d,			      \
					 ((uintptr_t)(i) >> 16) & 0xffff));   \
			if ((uintptr_t)(i) & 0x000000000000ffffULL)	      \
				EMIT(PPC_RAW_ORI(d, d, (uintptr_t)(i) &       \
							0xffff));             \
		}							      \
	} } while (0)
#define PPC_LI_ADDR	PPC_LI64

#ifndef CONFIG_PPC_KERNEL_PCREL
#define PPC64_LOAD_PACA()						      \
	EMIT(PPC_RAW_LD(_R2, _R13, offsetof(struct paca_struct, kernel_toc)))
#else
#define PPC64_LOAD_PACA()	do {} while (0)
#endif
#else
#define PPC_LI64(d, i)	BUILD_BUG()
#define PPC_LI_ADDR	PPC_LI32
#define PPC64_LOAD_PACA() BUILD_BUG()
#endif

/*
 * The fly in the ointment of code size changing from pass to pass is
 * avoided by padding the short branch case with a NOP.	 If code size differs
 * with different branch reaches we will have the issue of code moving from
 * one pass to the next and will need a few passes to converge on a stable
 * state.
 */
#define PPC_BCC(cond, dest)	do {					      \
		if (is_offset_in_cond_branch_range((long)(dest) - CTX_NIA(ctx))) {	\
			PPC_BCC_SHORT(cond, dest);			      \
			EMIT(PPC_RAW_NOP());				      \
		} else {						      \
			/* Flip the 'T or F' bit to invert comparison */      \
			PPC_BCC_SHORT(cond ^ COND_CMP_TRUE, CTX_NIA(ctx) + 2*4);  \
			PPC_JMP(dest);					      \
		} } while(0)

/* To create a branch condition, select a bit of cr0... */
#define CR0_LT		0
#define CR0_GT		1
#define CR0_EQ		2
/* ...and modify BO[3] */
#define COND_CMP_TRUE	0x100
#define COND_CMP_FALSE	0x000
/* Together, they make all required comparisons: */
#define COND_GT		(CR0_GT | COND_CMP_TRUE)
#define COND_GE		(CR0_LT | COND_CMP_FALSE)
#define COND_EQ		(CR0_EQ | COND_CMP_TRUE)
#define COND_NE		(CR0_EQ | COND_CMP_FALSE)
#define COND_LT		(CR0_LT | COND_CMP_TRUE)
#define COND_LE		(CR0_GT | COND_CMP_FALSE)

#define SEEN_FUNC	0x20000000 /* might call external helpers */
#define SEEN_TAILCALL	0x40000000 /* uses tail calls */

struct codegen_context {
	/*
	 * This is used to track register usage as well
	 * as calls to external helpers.
	 * - register usage is tracked with corresponding
	 *   bits (r3-r31)
	 * - rest of the bits can be used to track other
	 *   things -- for now, we use bits 0 to 2
	 *   encoded in SEEN_* macros above
	 */
	unsigned int seen;
	unsigned int idx;
	unsigned int stack_size;
	int b2p[MAX_BPF_JIT_REG + 2];
	unsigned int exentry_idx;
	unsigned int alt_exit_addr;
};

#define bpf_to_ppc(r)	(ctx->b2p[r])

#ifdef CONFIG_PPC32
#define BPF_FIXUP_LEN	3 /* Three instructions => 12 bytes */
#else
#define BPF_FIXUP_LEN	2 /* Two instructions => 8 bytes */
#endif

static inline bool bpf_is_seen_register(struct codegen_context *ctx, int i)
{
	return ctx->seen & (1 << (31 - i));
}

static inline void bpf_set_seen_register(struct codegen_context *ctx, int i)
{
	ctx->seen |= 1 << (31 - i);
}

static inline void bpf_clear_seen_register(struct codegen_context *ctx, int i)
{
	ctx->seen &= ~(1 << (31 - i));
}

void bpf_jit_init_reg_mapping(struct codegen_context *ctx);
int bpf_jit_emit_func_call_rel(u32 *image, u32 *fimage, struct codegen_context *ctx, u64 func);
int bpf_jit_build_body(struct bpf_prog *fp, u32 *image, u32 *fimage, struct codegen_context *ctx,
		       u32 *addrs, int pass, bool extra_pass);
void bpf_jit_build_prologue(u32 *image, struct codegen_context *ctx);
void bpf_jit_build_epilogue(u32 *image, struct codegen_context *ctx);
void bpf_jit_build_fentry_stubs(u32 *image, struct codegen_context *ctx);
void bpf_jit_realloc_regs(struct codegen_context *ctx);
int bpf_jit_emit_exit_insn(u32 *image, struct codegen_context *ctx, int tmp_reg, long exit_addr);

int bpf_add_extable_entry(struct bpf_prog *fp, u32 *image, u32 *fimage, int pass,
			  struct codegen_context *ctx, int insn_idx,
			  int jmp_off, int dst_reg);

#endif

#endif
