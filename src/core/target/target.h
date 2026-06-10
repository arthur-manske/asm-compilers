#ifndef DPP_TARGET_H
#define DPP_TARGET_H

#include "core/types.h"

struct dpp_type_info {
	u32 ti_size;
	u32 ti_align;
};

struct dpp_target {
	char *tar_name;
	bool  tar_is_big_endian;

	struct dpp_type_info tar_ptr;

	/* Basic types */
	struct dpp_type_info tar_void;
	struct dpp_type_info tar_bool;
	struct dpp_type_info tar_char;
	struct dpp_type_info tar_short;
	struct dpp_type_info tar_int;
	struct dpp_type_info tar_long;
	struct dpp_type_info tar_llong;
	struct dpp_type_info tar_float;
	struct dpp_type_info tar_double;
	struct dpp_type_info tar_ldouble;

	/* Calling Convention */
	struct {
		char *name;
		u32   stack_align;
		u32   red_zone_size;

		char *int_arg_regs[16];
		u32   int_arg_reg_count;
		char *float_arg_regs[16];
		u32   float_arg_reg_count;

		char *int_ret_reg;
		char *float_ret_reg;

		char *preserved_regs[16];
		u32   preserved_reg_count;
		char *scratch_regs[16];
		u32   scratch_reg_count;

		u32 max_reg_return_size;
	} tar_cc;

	/* Struct rules */
	u32  tar_struct_default_align;
	u32  tar_struct_max_field_align;
	bool tar_struct_bitfield_packing;

	/* Linker Config */
	char *tar_linker_cmd;
	char *tar_dynamic_linker;
	char *tar_crt_objs[16];
	u32   tar_crt_count;
	char *tar_post_crt_objs[16];
	u32   tar_post_crt_count;
	char *tar_lib_paths[16];
	u32   tar_lib_path_count;
	char *tar_libs[16];
	u32   tar_lib_count;
};

int  dpp_target_load_yaml(struct dpp_target *target, const char *yaml_path);
void dpp_target_free(struct dpp_target *target);

#endif
