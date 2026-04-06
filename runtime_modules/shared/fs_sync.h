#ifndef JSMX_RUNTIME_MODULES_SHARED_FS_SYNC_H
#define JSMX_RUNTIME_MODULES_SHARED_FS_SYNC_H

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum runtime_fs_kind_e {
	RUNTIME_FS_KIND_FILE = 0,
	RUNTIME_FS_KIND_DIRECTORY = 1,
	RUNTIME_FS_KIND_SYMLINK = 2,
	RUNTIME_FS_KIND_BLOCK_DEVICE = 3,
	RUNTIME_FS_KIND_CHARACTER_DEVICE = 4,
	RUNTIME_FS_KIND_FIFO = 5,
	RUNTIME_FS_KIND_SOCKET = 6,
	RUNTIME_FS_KIND_OTHER = 7
} runtime_fs_kind_t;

typedef struct runtime_fs_path_info_s {
	runtime_fs_kind_t kind;
	runtime_fs_kind_t target_kind;
	int is_symlink;
} runtime_fs_path_info_t;

typedef struct runtime_fs_dir_entry_s {
	char *name;
	char *path;
	runtime_fs_kind_t kind;
	runtime_fs_kind_t target_kind;
	int is_symlink;
} runtime_fs_dir_entry_t;

typedef struct runtime_fs_dir_list_s {
	runtime_fs_dir_entry_t *entries;
	size_t count;
} runtime_fs_dir_list_t;

static int
runtime_fs_dir_entry_name_compare(const void *lhs_ptr, const void *rhs_ptr)
{
	const runtime_fs_dir_entry_t *lhs = lhs_ptr;
	const runtime_fs_dir_entry_t *rhs = rhs_ptr;

	return strcmp(lhs->name, rhs->name);
}

static int
runtime_fs_strdup(const char *src, char **dst_ptr)
{
	size_t len;
	char *copy;

	if (src == NULL || dst_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = strlen(src);
	copy = malloc(len + 1u);
	if (copy == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memcpy(copy, src, len + 1u);
	*dst_ptr = copy;
	return 0;
}

static runtime_fs_kind_t
runtime_fs_kind_from_stat(const struct stat *st)
{
	if (S_ISDIR(st->st_mode)) {
		return RUNTIME_FS_KIND_DIRECTORY;
	}
	if (S_ISREG(st->st_mode)) {
		return RUNTIME_FS_KIND_FILE;
	}
	if (S_ISLNK(st->st_mode)) {
		return RUNTIME_FS_KIND_SYMLINK;
	}
	if (S_ISBLK(st->st_mode)) {
		return RUNTIME_FS_KIND_BLOCK_DEVICE;
	}
	if (S_ISCHR(st->st_mode)) {
		return RUNTIME_FS_KIND_CHARACTER_DEVICE;
	}
	if (S_ISFIFO(st->st_mode)) {
		return RUNTIME_FS_KIND_FIFO;
	}
	if (S_ISSOCK(st->st_mode)) {
		return RUNTIME_FS_KIND_SOCKET;
	}
	return RUNTIME_FS_KIND_OTHER;
}

static int
runtime_fs_classify_path(const char *path, runtime_fs_path_info_t *info_ptr)
{
	struct stat st;

	if (path == NULL || info_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (lstat(path, &st) < 0) {
		return -1;
	}
	info_ptr->kind = runtime_fs_kind_from_stat(&st);
	info_ptr->target_kind = info_ptr->kind;
	info_ptr->is_symlink = S_ISLNK(st.st_mode) ? 1 : 0;
	if (info_ptr->is_symlink) {
		if (stat(path, &st) < 0) {
			return -1;
		}
		info_ptr->target_kind = runtime_fs_kind_from_stat(&st);
	}
	return 0;
}

static int
runtime_fs_realpath_copy(const char *path, char **buf_ptr)
{
	char *resolved;

	if (path == NULL || buf_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	resolved = realpath(path, NULL);
	if (resolved == NULL) {
		return -1;
	}
	*buf_ptr = resolved;
	return 0;
}

static int
runtime_fs_join_path(const char *base, const char *name, char **out_ptr)
{
	size_t base_len;
	size_t name_len;
	size_t need_slash;
	char *path;

	if (base == NULL || name == NULL || out_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	base_len = strlen(base);
	name_len = strlen(name);
	need_slash = (base_len > 0 && base[base_len - 1] != '/') ? 1u : 0u;
	path = malloc(base_len + need_slash + name_len + 1u);
	if (path == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memcpy(path, base, base_len);
	if (need_slash) {
		path[base_len] = '/';
	}
	memcpy(path + base_len + need_slash, name, name_len);
	path[base_len + need_slash + name_len] = '\0';
	*out_ptr = path;
	return 0;
}

static void
runtime_fs_dir_list_free(runtime_fs_dir_list_t *list)
{
	size_t i;

	if (list == NULL) {
		return;
	}
	for (i = 0; i < list->count; i++) {
		free(list->entries[i].name);
		free(list->entries[i].path);
	}
	free(list->entries);
	list->entries = NULL;
	list->count = 0;
}

static int
runtime_fs_dir_list_append(runtime_fs_dir_list_t *list, const char *name,
		const char *path, const runtime_fs_path_info_t *info)
{
	runtime_fs_dir_entry_t *entries;
	size_t next = list->count;

	entries = realloc(list->entries, sizeof(*entries) * (next + 1u));
	if (entries == NULL) {
		errno = ENOMEM;
		return -1;
	}
	list->entries = entries;
	memset(&list->entries[next], 0, sizeof(list->entries[next]));
	if (runtime_fs_strdup(name, &list->entries[next].name) < 0) {
		return -1;
	}
	if (runtime_fs_strdup(path, &list->entries[next].path) < 0) {
		free(list->entries[next].name);
		list->entries[next].name = NULL;
		return -1;
	}
	list->entries[next].kind = info->kind;
	list->entries[next].target_kind = info->target_kind;
	list->entries[next].is_symlink = info->is_symlink;
	list->count++;
	return 0;
}

static int
runtime_fs_list_dir(const char *path, runtime_fs_dir_list_t *list_ptr)
{
	DIR *dir;
	struct dirent *entry;
	runtime_fs_dir_list_t list = {0};

	if (path == NULL || list_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	dir = opendir(path);
	if (dir == NULL) {
		return -1;
	}
	for (;;) {
		char *full_path = NULL;
		runtime_fs_path_info_t info;

		errno = 0;
		entry = readdir(dir);
		if (entry == NULL) {
			if (errno != 0) {
				runtime_fs_dir_list_free(&list);
				closedir(dir);
				return -1;
			}
			break;
		}
		if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		if (runtime_fs_join_path(path, entry->d_name, &full_path) < 0) {
			runtime_fs_dir_list_free(&list);
			closedir(dir);
			return -1;
		}
		if (runtime_fs_classify_path(full_path, &info) < 0 ||
				runtime_fs_dir_list_append(&list, entry->d_name, full_path,
					&info) < 0) {
			free(full_path);
			runtime_fs_dir_list_free(&list);
			closedir(dir);
			return -1;
		}
		free(full_path);
	}
	closedir(dir);
	if (list.count > 1) {
		qsort(list.entries, list.count, sizeof(*list.entries),
				runtime_fs_dir_entry_name_compare);
	}
	*list_ptr = list;
	return 0;
}

static int
runtime_fs_read_text_file(const char *path, uint8_t **buf_ptr, size_t *len_ptr)
{
	FILE *stream;
	uint8_t *buf = NULL;
	size_t len = 0;
	size_t cap = 0;

	if (path == NULL || buf_ptr == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	stream = fopen(path, "rb");
	if (stream == NULL) {
		return -1;
	}
	for (;;) {
		size_t chunk;

		if (len == cap) {
			size_t new_cap = cap == 0 ? 4096u : cap * 2u;
			uint8_t *new_buf = realloc(buf, new_cap);

			if (new_buf == NULL) {
				free(buf);
				fclose(stream);
				errno = ENOMEM;
				return -1;
			}
			buf = new_buf;
			cap = new_cap;
		}
		chunk = fread(buf + len, 1, cap - len, stream);
		len += chunk;
		if (chunk == 0) {
			if (ferror(stream)) {
				free(buf);
				fclose(stream);
				return -1;
			}
			break;
		}
	}
	fclose(stream);
	*buf_ptr = buf;
	*len_ptr = len;
	return 0;
}

#endif
