#define _POSIX_C_SOURCE 200809L
#include "core/target/target.h"
#include <libfyaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u32 s_get_u32(struct fy_node *root, const char *path)
{
	struct fy_node *n = fy_node_by_path(root, path, (size_t)-1, FYNWF_DONT_FOLLOW);
	if (!n) return 0;
	size_t      len;
	const char *s = fy_node_get_scalar(n, &len);
	if (!s) return 0;
	char buf[64];
	snprintf(buf, sizeof(buf), "%.*s", (int)len, s);
	return (u32)atoi(buf);
}

static char *s_get_str(struct fy_node *root, const char *path)
{
	struct fy_node *n = fy_node_by_path(root, path, (size_t)-1, FYNWF_DONT_FOLLOW);
	if (!n) return NULL;
	size_t      len;
	const char *s = fy_node_get_scalar(n, &len);
	if (!s) return NULL;
	char *res = malloc(len + 1);
	memcpy(res, s, len);
	res[len] = 0;
	return res;
}

int dpp_target_load_yaml(struct dpp_target *tar, const char *yaml_path)
{
	memset(tar, 0, sizeof(*tar));
	struct fy_document *fyp = fy_document_build_from_file(NULL, yaml_path);
	if (!fyp) return -1;

	struct fy_node *root = fy_document_root(fyp);
	tar->tar_name        = s_get_str(root, "/name");

	/* Types */
	tar->tar_ptr.ti_size     = s_get_u32(root, "/types/ptr/size");
	tar->tar_ptr.ti_align    = s_get_u32(root, "/types/ptr/align");
	tar->tar_int.ti_size     = s_get_u32(root, "/types/int/size");
	tar->tar_int.ti_align    = s_get_u32(root, "/types/int/align");
	tar->tar_char.ti_size    = s_get_u32(root, "/types/char/size");
	tar->tar_char.ti_align   = s_get_u32(root, "/types/char/align");
	tar->tar_float.ti_size   = s_get_u32(root, "/types/float/size");
	tar->tar_float.ti_align  = s_get_u32(root, "/types/float/align");
	tar->tar_double.ti_size  = s_get_u32(root, "/types/double/size");
	tar->tar_double.ti_align = s_get_u32(root, "/types/double/align");

	/* Linker Config */
	tar->tar_linker_cmd     = s_get_str(root, "/linker");
	tar->tar_dynamic_linker = s_get_str(root, "/dynamic_linker");

	struct fy_node *crt = fy_node_by_path(root, "/crt", (size_t)-1, FYNWF_DONT_FOLLOW);
	if (crt && fy_node_is_sequence(crt)) {
		struct fy_node *n;
		void           *iter = NULL;
		while ((n = fy_node_sequence_iterate(crt, &iter))) {
			size_t      len;
			const char *s = fy_node_get_scalar(n, &len);
			if (s && tar->tar_crt_count < 16) {
				char *val = malloc(len + 1);
				memcpy(val, s, len);
				val[len]                                = 0;
				tar->tar_crt_objs[tar->tar_crt_count++] = val;
			}
		}
	}

	struct fy_node *pcrt = fy_node_by_path(root, "/post_crt", (size_t)-1, FYNWF_DONT_FOLLOW);
	if (pcrt && fy_node_is_sequence(pcrt)) {
		struct fy_node *n;
		void           *iter = NULL;
		while ((n = fy_node_sequence_iterate(pcrt, &iter))) {
			size_t      len;
			const char *s = fy_node_get_scalar(n, &len);
			if (s && tar->tar_post_crt_count < 16) {
				char *val = malloc(len + 1);
				memcpy(val, s, len);
				val[len]                                          = 0;
				tar->tar_post_crt_objs[tar->tar_post_crt_count++] = val;
			}
		}
	}

	struct fy_node *lp = fy_node_by_path(root, "/default_lib_paths", (size_t)-1, FYNWF_DONT_FOLLOW);
	if (lp && fy_node_is_sequence(lp)) {
		struct fy_node *n;
		void           *iter = NULL;
		while ((n = fy_node_sequence_iterate(lp, &iter))) {
			size_t      len;
			const char *s = fy_node_get_scalar(n, &len);
			if (s && tar->tar_lib_path_count < 16) {
				char *val = malloc(len + 1);
				memcpy(val, s, len);
				val[len]                                      = 0;
				tar->tar_lib_paths[tar->tar_lib_path_count++] = val;
			}
		}
	}

	struct fy_node *libs = fy_node_by_path(root, "/default_libs", (size_t)-1, FYNWF_DONT_FOLLOW);
	if (libs && fy_node_is_sequence(libs)) {
		struct fy_node *n;
		void           *iter = NULL;
		while ((n = fy_node_sequence_iterate(libs, &iter))) {
			size_t      len;
			const char *s = fy_node_get_scalar(n, &len);
			if (s && tar->tar_lib_count < 16) {
				char *val = malloc(len + 1);
				memcpy(val, s, len);
				val[len]                            = 0;
				tar->tar_libs[tar->tar_lib_count++] = val;
			}
		}
	}

	fy_document_destroy(fyp);
	return 0;
}

void dpp_target_free(struct dpp_target *tar)
{
	free(tar->tar_name);
	free(tar->tar_linker_cmd);
	free(tar->tar_dynamic_linker);
	for (u32 i = 0; i < tar->tar_crt_count; i++) free(tar->tar_crt_objs[i]);
	for (u32 i = 0; i < tar->tar_post_crt_count; i++) free(tar->tar_post_crt_objs[i]);
	for (u32 i = 0; i < tar->tar_lib_path_count; i++) free(tar->tar_lib_paths[i]);
	for (u32 i = 0; i < tar->tar_lib_count; i++) free(tar->tar_libs[i]);
}
