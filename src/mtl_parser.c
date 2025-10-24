// src/mtl_parser.c
#include "mtl_parser.h"
#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "log4c.h"

// Helper: safely assign string (free old, strdup new)
#define SET_MATERIAL_STRING(mat, member, value)                                \
  do {                                                                         \
    free((mat)->member);                                                       \
    (mat)->member = (value) ? wf_strdup(value) : NULL;                         \
  } while (0)

// Parse a single material property (called after newmtl)
static void parse_material_property(wf_material_t* mat, const char* line) {
  char* s = wf_trim((char*)line);

  // Skip comments and empty lines
  if (*s == '#' || *s == '\0')
    return;

  // Handle vector properties (Ka, Kd, Ks, Tf)
  if (strncmp(s, "Ka", 2) == 0 && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Ka.x     = wf_parse_float(&p);
    mat->Ka.y     = wf_parse_float(&p);
    mat->Ka.z     = wf_parse_float(&p);
  } else if (strncmp(s, "Kd", 2) == 0
             && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Kd.x     = wf_parse_float(&p);
    mat->Kd.y     = wf_parse_float(&p);
    mat->Kd.z     = wf_parse_float(&p);
  } else if (strncmp(s, "Ke", 2) == 0
             && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Ke.x     = wf_parse_float(&p);
    mat->Ke.y     = wf_parse_float(&p);
    mat->Ke.z     = wf_parse_float(&p);
  } else if (strncmp(s, "Ks", 2) == 0
             && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Ks.x     = wf_parse_float(&p);
    mat->Ks.y     = wf_parse_float(&p);
    mat->Ks.z     = wf_parse_float(&p);
  } else if (strncmp(s, "Tf", 2) == 0
             && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Tf.x     = wf_parse_float(&p);
    mat->Tf.y     = wf_parse_float(&p);
    mat->Tf.z     = wf_parse_float(&p);
    mat->Tf.w     = 1.0f;
  }
  // Handle scalar properties
  else if (strncmp(s, "Ns", 2) == 0 && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Ns       = wf_parse_float(&p);
  } else if (strncmp(s, "Ni", 2) == 0
             && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Ni       = wf_parse_float(&p);
  } else if (*s == 'd' && (s[1] == ' ' || s[1] == '\t')) {
    const char* p = wf_trim(s + 1);
    mat->d        = wf_parse_float(&p);
  } else if (strncmp(s, "Tr", 2) == 0
             && (s[2] == ' ' || s[2] == '\t' || !s[2])) {
    const char* p = wf_trim(s + 2);
    mat->Tr       = wf_parse_float(&p);
    mat->d        = 1.0f - mat->Tr;
  } else if (strncmp(s, "illum", 5) == 0
             && (s[5] == ' ' || s[5] == '\t' || !s[5])) {
    const char* p = wf_trim(s + 5);
    mat->illum    = (int)wf_parse_float(&p);
  }
  // Handle string properties (textures)
  else if (strncmp(s, "map_Ka", 6) == 0
           && (s[6] == ' ' || s[6] == '\t' || !s[6])) {
    SET_MATERIAL_STRING(mat, map_Ka, wf_trim(s + 6));
  } else if (strncmp(s, "map_Kd", 6) == 0
             && (s[6] == ' ' || s[6] == '\t' || !s[6])) {
    SET_MATERIAL_STRING(mat, map_Kd, wf_trim(s + 6));
  } else if (strncmp(s, "map_Ks", 6) == 0
             && (s[6] == ' ' || s[6] == '\t' || !s[6])) {
    SET_MATERIAL_STRING(mat, map_Ks, wf_trim(s + 6));
  } else if (strncmp(s, "map_Ns", 6) == 0
             && (s[6] == ' ' || s[6] == '\t' || !s[6])) {
    SET_MATERIAL_STRING(mat, map_Ns, wf_trim(s + 6));
  } else if (strncmp(s, "map_d", 5) == 0
             && (s[5] == ' ' || s[5] == '\t' || !s[5])) {
    SET_MATERIAL_STRING(mat, map_d, wf_trim(s + 5));
  } else if (strncmp(s, "map_Tr", 6) == 0
             && (s[6] == ' ' || s[6] == '\t' || !s[6])) {
    SET_MATERIAL_STRING(mat, map_Tr, wf_trim(s + 6));
  } else if (strncmp(s, "bump", 4) == 0
             && (s[4] == ' ' || s[4] == '\t' || !s[4])) {
    SET_MATERIAL_STRING(mat, bump, wf_trim(s + 4));
  } else if (strncmp(s, "disp", 4) == 0
             && (s[4] == ' ' || s[4] == '\t' || !s[4])) {
    SET_MATERIAL_STRING(mat, disp, wf_trim(s + 4));
  } else if (strncmp(s, "decal", 5) == 0
             && (s[5] == ' ' || s[5] == '\t' || !s[5])) {
    SET_MATERIAL_STRING(mat, decal, wf_trim(s + 5));
  }
  // Note: Ke, sharpness, etc. are ignored (not in wf_material_t)
}

wf_error_t wf_mtl_parse_file(wf_mtl_parser_t* parser, const char* filename,
                             wf_material_t** materials, size_t* material_count,
                             size_t* material_cap) {
  LOG_INFO("Starting MTL file parsing: %s", filename);

  FILE* file = fopen(filename, "r");
  if (!file) {
    LOG_ERROR("Cannot open MTL file: [%s]", filename);
    return WF_ERROR_FILE_NOT_FOUND;
  }

  wf_material_t* mats  = *materials;
  size_t         count = *material_count;
  size_t         cap   = *material_cap;
  // wf_material_t current_mat = {0};
  // current_mat.Kd = (wf_vec3){0.6f, 0.6f, 0.6f};
  // current_mat.illum = 2;

  char   line[4096];
  size_t line_num = 0;

  while (fgets(line, sizeof(line), file)) {
    line_num++;
    char* s = wf_trim(line);
    if (*s == '#' || *s == '\0')
      continue;

    // Handle newmtl: finalize previous material and start new one
    if (strncmp(s, "newmtl", 6) == 0
        && (s[6] == ' ' || s[6] == '\t' || !s[6])) {
      mats = wf_realloc_array(mats, &cap, ++count, sizeof(wf_material_t));
      if (!mats)
        goto oom;
      // Initialize new material
      memset(&mats[count - 1], 0, sizeof(wf_material_t));
      SET_MATERIAL_STRING(&mats[count - 1], name, wf_trim(s + 6));
      mats[count - 1].Kd    = (wf_vec3){ 0.6f, 0.6f, 0.6f };
      mats[count - 1].illum = 2;
      continue;
    }

    // Parse property into current material
    parse_material_property(&mats[count - 1], s);
  }

  fclose(file);
  *materials      = mats;
  *material_count = count;
  LOG_INFO("Successfully parsed MTL file: %s (%zu materials)", filename, count);
  return WF_SUCCESS;

oom:
  fclose(file);
  LOG_ERROR("MTL parsing failed at line %zu: Out of memory", line_num);
  return WF_ERROR_OUT_OF_MEMORY;
}
