// src/mtl_parser.c
#include "mtl_parser.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "log4c.h"

wf_error_t wf_mtl_parse_file(wf_mtl_parser_t* parser, const char* filename,
                             wf_material_t** materials, size_t* material_count,
                             size_t* material_cap) {
  LOG_INFO("Starting MTL file parsing: %s", filename);

  FILE* file = fopen(filename, "r");
  if (!file) {
    LOG_ERROR("Cannot open MTL file: %s", filename);
    return WF_ERROR_FILE_NOT_FOUND;
  }

  wf_material_t* mats        = *materials;
  size_t         count       = *material_count;
  size_t         cap         = *material_cap;
  wf_material_t  current_mat = { 0 };
  current_mat.Kd             = (wf_vec3){ 0.6f, 0.6f, 0.6f };
  current_mat.illum          = 2;

  char   line[4096];
  size_t line_num = 0;

  while (fgets(line, sizeof(line), file)) {
    line_num++;
    char* s = wf_trim(line);
    if (*s == '#' || *s == '\0')
      continue;

    if (strncmp(s, "newmtl", 6) == 0) {
      count += 1;
      if (current_mat.name) {
        mats = wf_realloc_array(mats, &cap, count, sizeof(wf_material_t));
        if (!mats) {
          fclose(file);
          free(current_mat.name);
          LOG_ERROR("MTL parsing failed at line %zu: Out of memory", line_num);
          return WF_ERROR_OUT_OF_MEMORY;
        }
        mats[count - 1] = current_mat;
        memset(&current_mat, 0, sizeof(current_mat));
      }
      s                 = wf_trim(s + 6);
      current_mat.name  = wf_strdup(s);
      current_mat.Kd    = (wf_vec3){ 0.6f, 0.6f, 0.6f };
      current_mat.illum = 2;
      LOG_DEBUG("Parsed material: %s", current_mat.name);
      continue;
    }

    if (strncmp(s, "Ka", 2) == 0) {
      s                = wf_trim(s + 2);
      current_mat.Ka.x = wf_parse_float(&(const char*){ s });
      current_mat.Ka.y = wf_parse_float(&(const char*){ s });
      current_mat.Ka.z = wf_parse_float(&(const char*){ s });
    } else if (strncmp(s, "Kd", 2) == 0) {
      s                = wf_trim(s + 2);
      current_mat.Kd.x = wf_parse_float(&(const char*){ s });
      current_mat.Kd.y = wf_parse_float(&(const char*){ s });
      current_mat.Kd.z = wf_parse_float(&(const char*){ s });
    } else if (strncmp(s, "Ks", 2) == 0) {
      s                = wf_trim(s + 2);
      current_mat.Ks.x = wf_parse_float(&(const char*){ s });
      current_mat.Ks.y = wf_parse_float(&(const char*){ s });
      current_mat.Ks.z = wf_parse_float(&(const char*){ s });
    } else if (strncmp(s, "Tf", 2) == 0) {
      s                = wf_trim(s + 2);
      current_mat.Tf.x = wf_parse_float(&(const char*){ s });
      current_mat.Tf.y = wf_parse_float(&(const char*){ s });
      current_mat.Tf.z = wf_parse_float(&(const char*){ s });
      current_mat.Tf.w = 1.0f;
    } else if (strncmp(s, "Ns", 2) == 0) {
      s              = wf_trim(s + 2);
      current_mat.Ns = wf_parse_float(&(const char*){ s });
    } else if (strncmp(s, "Ni", 2) == 0) {
      s              = wf_trim(s + 2);
      current_mat.Ni = wf_parse_float(&(const char*){ s });
    } else if (*s == 'd' && (s[1] == ' ' || s[1] == '\t')) {
      s             = wf_trim(s + 1);
      current_mat.d = wf_parse_float(&(const char*){ s });
    } else if (strncmp(s, "Tr", 2) == 0) {
      s              = wf_trim(s + 2);
      current_mat.Tr = wf_parse_float(&(const char*){ s });
      current_mat.d  = 1.0f - current_mat.Tr;
    } else if (strncmp(s, "illum", 5) == 0) {
      s                 = wf_trim(s + 5);
      current_mat.illum = (int)wf_parse_float(&(const char*){ s });
    } else if (strncmp(s, "map_Ka", 6) == 0) {
      s                  = wf_trim(s + 6);
      current_mat.map_Ka = wf_strdup(s);
    } else if (strncmp(s, "map_Kd", 6) == 0) {
      s                  = wf_trim(s + 6);
      current_mat.map_Kd = wf_strdup(s);
    } else if (strncmp(s, "map_Ks", 6) == 0) {
      s                  = wf_trim(s + 6);
      current_mat.map_Ks = wf_strdup(s);
    } else if (strncmp(s, "map_Ns", 6) == 0) {
      s                  = wf_trim(s + 6);
      current_mat.map_Ns = wf_strdup(s);
    } else if (strncmp(s, "map_d", 5) == 0) {
      s                 = wf_trim(s + 5);
      current_mat.map_d = wf_strdup(s);
    } else if (strncmp(s, "map_Tr", 6) == 0) {
      s                  = wf_trim(s + 6);
      current_mat.map_Tr = wf_strdup(s);
    } else if (strncmp(s, "bump", 4) == 0) {
      s                = wf_trim(s + 4);
      current_mat.bump = wf_strdup(s);
    } else if (strncmp(s, "disp", 4) == 0) {
      s                = wf_trim(s + 4);
      current_mat.disp = wf_strdup(s);
    } else if (strncmp(s, "decal", 5) == 0) {
      s                 = wf_trim(s + 5);
      current_mat.decal = wf_strdup(s);
    }
  }

  if (current_mat.name) {
    mats = wf_realloc_array(mats, &cap, count, sizeof(wf_material_t));
    if (!mats) {
      fclose(file);
      free(current_mat.name);
      LOG_ERROR("MTL parsing failed: Out of memory saving last material");
      return WF_ERROR_OUT_OF_MEMORY;
    }
    mats[count - 1] = current_mat;
  }

  fclose(file);
  *materials      = mats;
  *material_count = count;

  LOG_INFO("Successfully parsed MTL file: %s (%zu materials)", filename, count);
  return WF_SUCCESS;
}
