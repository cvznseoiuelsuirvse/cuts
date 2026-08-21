#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "util/log.h"
#include "util/helpers.h"

struct font_dir {
  uint32_t scaler_type;
  uint16_t num_tables;
  uint16_t search_range;
  uint16_t entry_selector;
  uint16_t range_shift;
};

struct font_table_dir {
  char tag[4];
  uint32_t checksum;
  uint32_t offset;
  uint32_t length;
};

struct font_name_table_header {
  uint16_t format;
  uint16_t count;
  uint16_t string_offset;
};

enum FONT_NAME_RECORD_PLATOFORM_ID {
  FONT_PLATFORM_UNICODE,
  FONT_PLATFORM_MACINTOSH,
  FONT_PLATFORM_RESERVED,
  FONT_PLATFORM_MICROSOFT,
};

enum FONT_NAME_RECORD_NAME_ID {
  FONT_NAME_COPYRIGHT,
  FONT_NAME_FONT_FAMILY,
  FONT_NAME_FONT_SUBFAMILY,
  FONT_NAME_SUBFAMILY_ID,
  FONT_NAME_FULLNAME,
  FONT_NAME_RECORD_NAME_ID_LENGTH,
};

struct font_name_record {
  uint16_t platform_id;          // enum FONT_NAME_RECORD_PLATFORM_ID
  uint16_t platform_specific_id;
  uint16_t language_id;
  uint16_t name_id;              // enum FONT_NAME_RECORD_NAME_ID
  uint16_t length;
  uint16_t offset;
};

#define to_var16(s, var) uint16_t var = swap_16(s.var)
#define to_var32(s, var) uint32_t var = swap_32(s.var)

static void b_to_string(char *in, char *out, size_t length) {
  memcpy(out, in, length);
  out[length] = 0;
}

static void u16_to_string(char *in, char *out, size_t length) {
  for (size_t i = *in ? 0 : 1; i < length; i+=2) {
    out[i/2] = in[i];
  }
  out[length/2] = 0;
}

static const char *name_id_to_string(enum FONT_NAME_RECORD_NAME_ID id) {
  switch (id) {
    case FONT_NAME_COPYRIGHT:      return "copyright";
    case FONT_NAME_FONT_FAMILY:    return "font family";
    case FONT_NAME_FONT_SUBFAMILY: return "font subfamily";
    case FONT_NAME_SUBFAMILY_ID:   return "subfamily ID";
    case FONT_NAME_FULLNAME:       return "full name";
    default:                       return "other";
  }
}

static int read_name_table(FILE *f, size_t table_offset, size_t length, char *name,
                       size_t name_size,
                       enum FONT_NAME_RECORD_NAME_ID name_to_return) {
  struct font_name_table_header header;
  assert(length > sizeof(header));

  if (!read_at(&header, sizeof(header), 1, table_offset, f)) {
    c_log_errno(C_LOG_ERROR, "failed to read name table header");
    goto error;
  }

  to_var16(header, count);
  to_var16(header, string_offset);


  for (size_t i = 0; i < count; i++) {
    struct font_name_record record;
    if (!read_at(&record, sizeof(record), 1, table_offset + sizeof(header) + i * sizeof(record), f)) {
      c_log_errno(C_LOG_ERROR, "failed to read name record %zu", i);
      goto error;
    }

    to_var16(record, platform_id);
    to_var16(record, name_id);
    to_var16(record, length);
    to_var16(record, offset);

    char buf[512];
    if (!read_at(buf, 1, length, table_offset + string_offset + offset, f)) {
      c_log_errno(C_LOG_ERROR, "failed to read '%s' name string", name_id_to_string(name_id));
      goto error;
    }

    if (name_id == name_to_return) {
      if (platform_id == FONT_PLATFORM_MICROSOFT)
        u16_to_string(buf, name, length);
      else
        b_to_string(buf, name, length);

      assert(name_size > length);
      return 0;
    }

  }

error:
  return 1;
}

static int read_font_name(const char *path, char *fontname, size_t fontname_size) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    c_log(C_LOG_ERROR, "failed to open %s\n", path);
    return 1;
  }

  int ret = 0;
  struct font_dir font_dir;
  size_t read_items = fread(&font_dir, sizeof(font_dir), 1, f);

  if (read_items < 1) {
    c_log(C_LOG_ERROR, "failed to read font directory\n");
    ret = 1;
    goto out;
  }

  to_var32(font_dir, num_tables);
  struct font_table_dir table;
  for (size_t i = 0; i < num_tables; i++) {
    if (!read_at(&table, sizeof(table), 1, sizeof(struct font_dir) + i * sizeof(table), f)) {
      c_log_errno(C_LOG_ERROR, "failed to read table %zu", i);
      ret = 1;
      goto out;
    }

    char tag[5];
    b_to_string(table.tag, tag, 4);

    to_var32(table, offset);
    to_var32(table, length);

    if (STREQ(tag, "name")) {
      ret = read_name_table(f, offset, length, fontname, fontname_size,
                            FONT_NAME_FULLNAME);
      goto out;
    }
  }

out:
  fclose(f);
  return ret;
}

static int search_dir(const char *fonts_dir, const char *fontname, char *fontpath, size_t size) {
  DIR *dirp = opendir(fonts_dir);
  if (!dirp) {
    c_log_errno(C_LOG_ERROR, "failed to open dir %s", fonts_dir);
    return 1;
  }

  int ret = 1;
  char next_path[size];

  errno = 0;
  struct dirent *dir;
  while ((dir = readdir(dirp))) {
    if (STREQ(dir->d_name, ".") || STREQ(dir->d_name, "..")) continue;

    if (dir->d_type == DT_DIR) {
      snprintf(next_path, sizeof(next_path), "%s/%s", fonts_dir, dir->d_name);
      if (!(ret = search_dir(next_path, fontname, fontpath, size))) goto out;

    } else if (dir->d_type == DT_REG) {
      snprintf(next_path, sizeof(next_path), "%s/%s", fonts_dir, dir->d_name);

      size_t next_path_len = strlen(next_path);
      next_path[next_path_len - 4] = 0;
      char *file_ext = next_path + next_path_len - 3;

      if (STREQ(file_ext, "ttf") || STREQ(file_ext, "otf")) {
        next_path[next_path_len - 4] = '.';
        char buf[512];

        if (read_font_name(next_path, buf, sizeof(buf)) == 0) {
          if (STREQ(fontname, buf)) {
            snprintf(fontpath, size, "%s", next_path);
            ret = 0;
            goto out;
          }
        }
      }
    }
  }

  if (errno) c_log_errno(C_LOG_ERROR, "failed to read %s", fonts_dir);

out:
  closedir(dirp);
  return ret;
}

int get_fontpath(const char *font, char *fontpath, size_t size) {
  const char *sys_fonts_dir = "/usr/share/fonts";
  char local_fonts_dir[512];

  const char *homepath = getenv("HOME");
  if (!homepath) {
    c_log(C_LOG_ERROR, "couldn't get 'HOME' env var");
    return 1;
  }
  snprintf(local_fonts_dir, sizeof(local_fonts_dir), "%s/.local/share/fonts", homepath);

  if (search_dir(local_fonts_dir, font, fontpath, size) == 0) return 0;
  if (search_dir(sys_fonts_dir, font, fontpath, size) == 0) return 0;

  return 1;
}
