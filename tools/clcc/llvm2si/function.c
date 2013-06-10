/*
 *  Multi2Sim
 *  Copyright (C) 2013  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>

#include <arch/southern-islands/asm/arg.h>
#include <clcc/si2bin/arg.h>
#include <clcc/si2bin/inst.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/linked-list.h>
#include <lib/util/list.h>
#include <lib/util/string.h>

#include "basic-block.h"
#include "function.h"
#include "symbol-table.h"


/*
 * Function Argument Object
 */

struct llvm2si_function_arg_t *llvm2si_function_arg_create(struct si_arg_t *arg)
{
	struct llvm2si_function_arg_t *function_arg;

	/* Allocate */
	function_arg = xcalloc(1, sizeof(struct llvm2si_function_arg_t));
	function_arg->arg = arg;

	/* Return */
	return function_arg;
}


void llvm2si_function_arg_free(struct llvm2si_function_arg_t *arg)
{
	si_arg_free(arg->arg);
	free(arg);
}


void llvm2si_function_arg_dump(struct llvm2si_function_arg_t *function_arg, FILE *f)
{
	struct si_arg_t *arg = function_arg->arg;

	switch (arg->type)
	{
	case si_arg_pointer:

		/* Type, name, offset, UAV */
		fprintf(f, "\t%s* %s %d uav11\n",
				str_map_value(&si_arg_data_type_map, arg->pointer.data_type),
				arg->name, function_arg->index * 16);
		break;

	default:
		fatal("%s: argument type not recognized (%d)",
				__FUNCTION__, arg->type);
	}
}





/*
 * Function Object
 */

struct llvm2si_function_t *llvm2si_function_create(const char *name)
{
	struct llvm2si_function_t *function;

	/* Allocate */
	function = xcalloc(1, sizeof(struct llvm2si_function_t));
	function->name = xstrdup(name);
	function->basic_block_list = linked_list_create();
	function->arg_list = list_create();
	function->symbol_table = llvm2si_symbol_table_create();

	/* Return */
	return function;
}


void llvm2si_function_free(struct llvm2si_function_t *function)
{
	int index;

	/* Free list of basic blocks */
	LINKED_LIST_FOR_EACH(function->basic_block_list)
		llvm2si_basic_block_free(linked_list_get(function->basic_block_list));
	linked_list_free(function->basic_block_list);

	/* Free list of arguments */
	LIST_FOR_EACH(function->arg_list, index)
		llvm2si_function_arg_free(list_get(function->arg_list, index));
	list_free(function->arg_list);

	/* Rest */
	llvm2si_symbol_table_free(function->symbol_table);
	free(function->name);
	free(function);
}


static void llvm2si_function_dump_data(struct llvm2si_function_t *function,
		FILE *f)
{
	/* Section header */
	fprintf(f, ".data\n");

	/* User elements */
	fprintf(f, "\tuserElementCount = 3\n");
	fprintf(f, "\tuserElements[0] = PTR_INTERNAL_GLOBAL_TABLE, 0, s[%d:%d]\n",
			function->sreg_uav10, function->sreg_uav10 + 1);
	fprintf(f, "\tuserElements[1] = PTR_UAV_TABLE, 0, s[%d:%d]\n",
			function->sreg_uav11, function->sreg_uav11 + 1);
	fprintf(f, "\tuserElements[2] = IMM_CONST_BUFFER, 1, s[%d:%d]\n",
			function->sreg_cb1, function->sreg_cb1 + 3);
	fprintf(f, "\n");

	/* Registers */
	fprintf(f, "\tNumVgprs = %d\n", function->num_vregs);
	fprintf(f, "\tNumSgprs = %d\n", function->num_sregs);
	fprintf(f, "\n");

	/* Floating-point mode */
	fprintf(f, "\tFloatMode = 192\n");
	fprintf(f, "\tIeeeMode = 0\n");
	fprintf(f, "\n");

	/* Program resources */
	fprintf(f, "\tCOMPUTE_PGM_RSRC2 = 0x00000011\n");  /* What is this? */
	fprintf(f, "\tCOMPUTE_PGM_RSRC2:USER_SGPR = %d\n", function->sreg_wgid);  /* Work-group ID */
	fprintf(f, "\n");
}


void llvm2si_function_dump(struct llvm2si_function_t *function, FILE *f)
{
	struct llvm2si_basic_block_t *basic_block;
	struct llvm2si_function_arg_t *function_arg;

	int index;

	/* Function name */
	fprintf(f, ".global %s\n\n", function->name);

	/* Arguments */
	fprintf(f, ".args\n");
	LIST_FOR_EACH(function->arg_list, index)
	{
		function_arg = list_get(function->arg_list, index);
		llvm2si_function_arg_dump(function_arg, f);
	}
	fprintf(f, "\n");

	/* Dump basic blocks */
	fprintf(f, ".text\n");
	LINKED_LIST_FOR_EACH(function->basic_block_list)
	{
		basic_block = linked_list_get(function->basic_block_list);
		llvm2si_basic_block_dump(basic_block, f);
	}
	fprintf(f, "\n");

	/* Dump section '.data' */
	llvm2si_function_dump_data(function, f);
	fprintf(f, "\n");
}


void llvm2si_function_add_basic_block(struct llvm2si_function_t *function,
		struct llvm2si_basic_block_t *basic_block)
{
	/* Check that basic block does not belong to any other function. */
	if (basic_block->function)
		panic("%s: basic block '%s' already added to a function",
				__FUNCTION__, basic_block->name);

	/* Add basic block */
	linked_list_add(function->basic_block_list, basic_block);
	basic_block->function = function;
}


void llvm2si_function_add_arg(struct llvm2si_function_t *function,
		struct llvm2si_function_arg_t *arg,
		struct llvm2si_basic_block_t *basic_block)
{
	struct list_t *arg_list;
	struct si2bin_inst_t *inst;

	/* Check that argument does not belong to a function yet */
	if (arg->function)
		panic("%s: argument already added", __FUNCTION__);

	/* If this is the first argument added to the function, update the
	 * scalar register that points to the first argument (sreg_arg). */
	if (!function->arg_list->count)
		function->sreg_arg = function->num_sregs;

	/* Make sure that all function arguments appear in contiguous scalar
	 * registers after the first recorded argument. */
	if (function->sreg_arg + function->arg_list->count != function->num_sregs)
		panic("%s: arguments in non-contiguous scalar registers", __FUNCTION__);

	/* Add argument */
	list_add(function->arg_list, arg);
	arg->function = function;
	arg->index = function->arg_list->count - 1;

	/* Allocate 1 scalar register for the argument */
	arg->sreg = function->num_sregs;
	function->num_sregs++;

	/* Generate code to load argument
	 * s_buffer_load_dword s[arg], s[cb1:cb1+3], idx*4
	 */
	arg_list = list_create();
	list_add(arg_list, si2bin_arg_create_scalar_register(arg->sreg));
	list_add(arg_list, si2bin_arg_create_scalar_register_series(function->sreg_cb1,
			function->sreg_cb1 + 3));
	list_add(arg_list, si2bin_arg_create_literal(arg->index * 4));
	inst = si2bin_inst_create(SI_INST_S_BUFFER_LOAD_DWORD, arg_list);
	llvm2si_basic_block_add_inst(basic_block, inst);
}


void llvm2si_function_gen_header(struct llvm2si_function_t *function,
		struct llvm2si_basic_block_t *basic_block)
{
	struct si2bin_inst_t *inst;
	struct list_t *arg_list;
	int index;

	/* Function must be empty at this point */
	assert(!function->num_sregs);
	assert(!function->num_vregs);

	/* Allocate 2 scalar registers for UAV table */
	function->sreg_uav_table = function->num_sregs;
	function->num_sregs += 2;

	/* Allocate 4 scalar registers for CB0 */
	function->sreg_cb0 = function->num_sregs;
	function->num_sregs += 4;

	/* Allocate 4 scalar registers for CB1 */
	function->sreg_cb1 = function->num_sregs;
	function->num_sregs += 4;

	/* Allocate 3 scalar registers for the work-group ID */
	function->sreg_wgid = function->num_sregs;
	function->num_sregs += 3;

	/* Allocate 3 scalar registers for the local size */
	function->sreg_lsize = function->num_sregs;
	function->num_sregs += 3;

	/* Allocate 3 scalar registers for the global offset */
	function->sreg_offs = function->num_sregs;
	function->num_sregs += 3;

	/* Allocate 4 scalar registers for uav10 */
	function->sreg_uav10 = function->num_sregs;
	function->num_sregs += 4;

	/* Allocate 4 scalar registers for uav11 */
	function->sreg_uav11 = function->num_sregs;
	function->num_sregs += 4;

	/* Allocate 3 vector registers (v[0:2]) for local ID */
	assert(!function->num_vregs);
	function->vreg_lid = function->num_vregs;
	function->num_vregs += 3;

	/* Allocate 3 vector register for global ID */
	function->vreg_gid = function->num_vregs;
	function->num_vregs += 3;

	/* Allocate 1 vector register for stack pointer */
	/* FIXME - initialize stack pointer */
	function->vreg_sp = function->num_vregs;
	function->num_vregs++;


	/* Obtain local size in s[lsize:lsize+2].
	 *
	 * s_buffer_load_dword s[lsize], s[cb0:cb0+3], 0x04
	 * s_buffer_load_dword s[lsize+1], s[cb0:cb0+3], 0x05
	 * s_buffer_load_dword s[lsize+2], s[cb0:cb0+3], 0x06
	 */
	for (index = 0; index < 3; index++)
	{
		arg_list = list_create();
		list_add(arg_list, si2bin_arg_create_scalar_register(
				function->sreg_lsize + index));
		list_add(arg_list, si2bin_arg_create_scalar_register_series(
				function->sreg_cb0, function->sreg_cb0 + 3));
		list_add(arg_list, si2bin_arg_create_literal(4 + index));
		inst = si2bin_inst_create(SI_INST_S_BUFFER_LOAD_DWORD, arg_list);
		llvm2si_basic_block_add_inst(basic_block, inst);
	}

	/* Obtain global offset in s[offs:offs+2].
	 *
	 * s_buffer_load_dword s[offs], s[cb0:cb0+3], 0x18
	 * s_buffer_load_dword s[offs], s[cb0:cb0+3], 0x19
	 * s_buffer_load_dword s[offs], s[cb0:cb0+3], 0x1a
	 */
	for (index = 0; index < 3; index++)
	{
		arg_list = list_create();
		list_add(arg_list, si2bin_arg_create_scalar_register(
				function->sreg_offs + index));
		list_add(arg_list, si2bin_arg_create_scalar_register_series(
				function->sreg_cb0, function->sreg_cb0 + 3));
		list_add(arg_list, si2bin_arg_create_literal(0x18 + index));
		inst = si2bin_inst_create(SI_INST_S_BUFFER_LOAD_DWORD, arg_list);
		llvm2si_basic_block_add_inst(basic_block, inst);
	}

	/* Calculate global ID in dimensions [0:2] and store it in v[3:5].
	 *
	 * v_mov_b32 v[gid+dim], s[lsize+dim]
	 * v_mul_i32_i24 v[gid+dim], s[wgid+dim], v[gid+dim]
	 * v_add_i32 v[gid+dim], vcc, v[gid+dim], v[lid+dim]
	 * v_add_i32 v[gid+dim], vcc, v[gid+dim], s[offs+dim]
	 */
	for (index = 0; index < 3; index++)
	{
		/* v_mov_b32 */
		arg_list = list_create();
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_gid + index));
		list_add(arg_list, si2bin_arg_create_scalar_register(
				function->sreg_lsize + index));
		inst = si2bin_inst_create(SI_INST_V_MOV_B32, arg_list);
		llvm2si_basic_block_add_inst(basic_block, inst);

		/* v_mul_i32_i24 */
		arg_list = list_create();
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_gid + index));
		list_add(arg_list, si2bin_arg_create_scalar_register(
				function->sreg_wgid + index));
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_gid + index));
		inst = si2bin_inst_create(SI_INST_V_MUL_I32_I24, arg_list);
		llvm2si_basic_block_add_inst(basic_block, inst);

		/* v_add_i32 */
		arg_list = list_create();
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_gid + index));
		list_add(arg_list, si2bin_arg_create_special_register(si_inst_special_reg_vcc));
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_gid + index));
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_lid + index));
		inst = si2bin_inst_create(SI_INST_V_ADD_I32, arg_list);
		llvm2si_basic_block_add_inst(basic_block, inst);

		/* v_add_i32 */
		arg_list = list_create();
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_gid + index));
		list_add(arg_list, si2bin_arg_create_special_register(si_inst_special_reg_vcc));
		list_add(arg_list, si2bin_arg_create_scalar_register(
				function->sreg_offs + index));
		list_add(arg_list, si2bin_arg_create_vector_register(
				function->vreg_gid + index));
		inst = si2bin_inst_create(SI_INST_V_ADD_I32, arg_list);
		llvm2si_basic_block_add_inst(basic_block, inst);
	}

	/* Load UAVs. UAV10 is used for private memory an stored in s[uav10:uav10+3].
	 * UAV11 is used for global memory and stored in s[uav11:uav11+3].
	 *
	 * s_load_dwordx4 s[uav10:uav10+3], s[uav:uav+1], 0x50
	 */
	arg_list = list_create();
	list_add(arg_list, si2bin_arg_create_scalar_register_series(
			function->sreg_uav10, function->sreg_uav10 + 3));
	list_add(arg_list, si2bin_arg_create_scalar_register_series(
			function->sreg_uav_table, function->sreg_uav_table + 1));
	list_add(arg_list, si2bin_arg_create_literal(0x50));
	inst = si2bin_inst_create(SI_INST_S_LOAD_DWORDX4, arg_list);
	llvm2si_basic_block_add_inst(basic_block, inst);

	/* s_load_dwordx4 s[uav11:uav11+3], s[uav:uav+1], 0x58 */
	arg_list = list_create();
	list_add(arg_list, si2bin_arg_create_scalar_register_series(
			function->sreg_uav11, function->sreg_uav11 + 3));
	list_add(arg_list, si2bin_arg_create_scalar_register_series(
			function->sreg_uav_table, function->sreg_uav_table + 1));
	list_add(arg_list, si2bin_arg_create_literal(0x58));
	inst = si2bin_inst_create(SI_INST_S_LOAD_DWORDX4, arg_list);
	llvm2si_basic_block_add_inst(basic_block, inst);
}

