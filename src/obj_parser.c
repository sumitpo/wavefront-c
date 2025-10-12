// src/obj_parser.c
#include "obj_parser.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "log4c.h"
#include "mtl_parser.h"

static const wf_command_t WF_COMMANDS[] = {
  // Longest commands first (to avoid prefix conflicts like "v" vs "vp")
  { "usemtl", 6, wf_handle_usemtl    },
  { "mtllib", 6, wf_handle_mtllib    },
  { "cstype", 6, wf_handle_freeform  },
  { "parm",   4, wf_handle_freeform  },
  { "trim",   4, wf_handle_freeform  },
  { "hole",   4, wf_handle_freeform  },
  { "scrv",   4, wf_handle_freeform  },
  { "surf",   4, wf_handle_freeform  },
  { "curv",   4, wf_handle_freeform  },
  { "bmat",   4, wf_handle_freeform  },
  { "step",   4, wf_handle_freeform  },
  { "deg",    3, wf_handle_freeform  },
  { "end",    3, wf_handle_freeform  },
  { "tex",    3, wf_handle_freeform  },
  { "vp",     2, wf_handle_parameter },
  { "vt",     2, wf_handle_texcoord  },
  { "vn",     2, wf_handle_normal    },
  { "sp",     2, wf_handle_freeform  },
  { "f",      1, wf_handle_face      },
  { "o",      1, wf_handle_object    },
  { "g",      1, wf_handle_group     },
  { "s",      1, wf_handle_smoothing },
  { "l",      1, wf_handle_line_elem },
  { "v",      1, wf_handle_vertex    },
  { NULL,     0, NULL                }
};

// Error handling
static void wf_set_error_with_line(wf_obj_parser_t* parser, const char* format,
                                   ...) {
  va_list args;
  va_start(args, format);
  free(parser->scene->error_message);

  char* error_msg;
  vasprintf(&error_msg, format, args);
  va_end(args);

  char* full_error;
  asprintf(&full_error, "Line %zu: %s", parser->line_number, error_msg);

  parser->scene->error_message = full_error;
  free(error_msg);
  LOG_ERROR("OBJ parsing error: %s", full_error);
}

// Index resolution
static int resolve_index(int idx, size_t count, int preserve_1_based) {
  if (preserve_1_based) {
    return idx;
  }
  if (idx == 0) {
    return -1;
  }
  if (idx > 0) {
    int result = idx - 1;
    return (result < (int)count) ? result : -1;
  } else {
    int result = (int)count + idx;
    return (result >= 0 && result < (int)count) ? result : -1;
  }
}

// Face index parsing helper
static wf_error_t wf_parse_face_index_helper(const char*      token,
                                             wf_vertex_index* idx,
                                             size_t v_count, size_t vt_count,
                                             size_t vn_count,
                                             int    preserve_1_based) {
  *idx = (wf_vertex_index){ -1, -1, -1 };

  if (!token || !token[0]) {
    return WF_SUCCESS;
  }

  char* token_copy = wf_strdup(token);
  if (!token_copy) {
    return WF_ERROR_OUT_OF_MEMORY;
  }

  char* v_str  = token_copy;
  char* vt_str = NULL;
  char* vn_str = NULL;

  char* slash1 = strchr(v_str, '/');
  if (slash1) {
    *slash1      = '\0';
    vt_str       = slash1 + 1;
    char* slash2 = strchr(vt_str, '/');
    if (slash2) {
      *slash2 = '\0';
      vn_str  = slash2 + 1;
    } else {
      vn_str = vt_str;
      vt_str = NULL;
    }
  }

  if (v_str[0]) {
    char* endptr;
    long  v_idx = strtol(v_str, &endptr, 10);
    if (endptr != v_str && *endptr == '\0') {
      idx->v_idx = resolve_index((int)v_idx, v_count, preserve_1_based);
    }
  }

  if (vt_str && vt_str[0]) {
    char* endptr;
    long  vt_idx = strtol(vt_str, &endptr, 10);
    if (endptr != vt_str && *endptr == '\0') {
      idx->vt_idx = resolve_index((int)vt_idx, vt_count, preserve_1_based);
    }
  }

  if (vn_str && vn_str[0]) {
    char* endptr;
    long  vn_idx = strtol(vn_str, &endptr, 10);
    if (endptr != vn_str && *endptr == '\0') {
      idx->vn_idx = resolve_index((int)vn_idx, vn_count, preserve_1_based);
    }
  }

  free(token_copy);
  return WF_SUCCESS;
}

// Polygon triangulation
static wf_face* wf_triangulate_polygon(wf_vertex_index* poly, size_t poly_count,
                                       size_t* tri_count) {
  if (poly_count < 3) {
    *tri_count = 0;
    return NULL;
  }
  if (poly_count == 3) {
    wf_face* face = malloc(sizeof(wf_face));
    if (!face)
      return NULL;
    face->vertices[0] = poly[0];
    face->vertices[1] = poly[1];
    face->vertices[2] = poly[2];
    *tri_count        = 1;
    return face;
  }

  *tri_count    = poly_count - 2;
  wf_face* tris = malloc(*tri_count * sizeof(wf_face));
  if (!tris)
    return NULL;

  for (size_t i = 0; i < *tri_count; i++) {
    tris[i].vertices[0] = poly[0];
    tris[i].vertices[1] = poly[i + 1];
    tris[i].vertices[2] = poly[i + 2];
  }
  return tris;
}

// Ensure current object exists
static wf_error_t wf_ensure_current_object(wf_obj_parser_t* parser) {
  if (!parser->current_object) {
    parser->current_object = calloc(1, sizeof(wf_object_t));
    if (!parser->current_object) {
      wf_set_error_with_line(parser, "Out of memory while creating object");
      return WF_ERROR_OUT_OF_MEMORY;
    }

    if (parser->current_object_name) {
      parser->current_object->name = wf_strdup(parser->current_object_name);
    }

    wf_add_object_to_list(parser, parser->current_object);
  }
  return WF_SUCCESS;
}

// Add object to scene list
static wf_error_t wf_add_object_to_list(wf_obj_parser_t* parser,
                                        wf_object_t*     obj) {
  if (!parser->scene->objects) {
    parser->scene->objects = obj;
  } else {
    wf_object_t* last = parser->scene->objects;
    while (last->next)
      last = last->next;
    last->next = obj;
  }
  return WF_SUCCESS;
}

// Generic vertex data parser
static wf_error_t wf_parse_vertex_data(wf_obj_parser_t* parser,
                                       const char* line, wf_vec3* vertex,
                                       size_t* count, wf_vec3** array,
                                       size_t* cap, size_t elem_size) {
  const char* s = line;
  vertex->x     = wf_parse_float(&s);
  if (*s)
    vertex->y = wf_parse_float(&s);
  if (*s)
    vertex->z = wf_parse_float(&s);

  (*count)++;
  *array = wf_realloc_array(*array, cap, *count, elem_size);
  if (!*array) {
    wf_set_error_with_line(parser, "Out of memory while parsing vertex data");
    return WF_ERROR_OUT_OF_MEMORY;
  }
  (*array)[*count - 1] = *vertex;

  return WF_SUCCESS;
}

// Parse face indices from line
static wf_error_t wf_parse_face_indices(wf_obj_parser_t*  parser,
                                        const char*       line,
                                        wf_vertex_index** indices,
                                        size_t*           idx_count) {
  char* buf = wf_strdup(line);
  if (!buf) {
    wf_set_error_with_line(parser, "Out of memory while parsing face");
    return WF_ERROR_OUT_OF_MEMORY;
  }

  *indices       = NULL;
  size_t idx_cap = 0;
  *idx_count     = 0;

  char* token = strtok(buf, " \t");
  while (token) {
    if (*idx_count >= idx_cap) {
      idx_cap = idx_cap ? idx_cap * 2 : 8;
      wf_vertex_index* new_indices =
          realloc(*indices, idx_cap * sizeof(wf_vertex_index));
      if (!new_indices) {
        free(*indices);
        free(buf);
        wf_set_error_with_line(parser,
                               "Out of memory while parsing face indices");
        return WF_ERROR_OUT_OF_MEMORY;
      }
      *indices = new_indices;
    }

    wf_parse_face_index_helper(token, &(*indices)[*idx_count],
                               parser->scene->vertex_count,
                               parser->scene->texcoord_count,
                               parser->scene->normal_count,
                               parser->options->preserve_indices);
    (*idx_count)++;
    token = strtok(NULL, " \t");
  }
  free(buf);
  return WF_SUCCESS;
}

// Add faces to current object
static wf_error_t wf_add_faces_to_object(wf_obj_parser_t* parser,
                                         wf_vertex_index* indices,
                                         size_t           idx_count) {
  if (idx_count < 3) {
    free(indices);
    LOG_WARN("Ignoring invalid face with %zu vertices at line %zu", idx_count,
             parser->line_number);
    return WF_SUCCESS;
  }

  if (parser->options->triangulate || idx_count > 4) {
    size_t   tri_count = 0;
    wf_face* tris      = wf_triangulate_polygon(indices, idx_count, &tri_count);
    free(indices);
    if (!tris) {
      wf_set_error_with_line(parser, "Out of memory while triangulating face");
      return WF_ERROR_OUT_OF_MEMORY;
    }

    size_t old_count = parser->current_object->face_count;
    parser->current_object->face_count += tri_count;
    parser->current_object->faces =
        wf_realloc_array(parser->current_object->faces,
                         &parser->current_object->face_cap,
                         parser->current_object->face_count, sizeof(wf_face));

    if (!parser->current_object->faces) {
      free(tris);
      wf_set_error_with_line(parser,
                             "Out of memory while storing triangulated faces");
      return WF_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < tri_count; i++) {
      parser->current_object->faces[old_count + i] = tris[i];
    }
    free(tris);
    log_debug("idx count is %zu, after triangulate get %zu faces", idx_count,
              tri_count);
  } else {
    wf_face face;
    for (size_t i = 0; i < 3 && i < idx_count; i++) {
      face.vertices[i] = indices[i];
    }
    for (size_t i = 3; i < idx_count; i++) {
      face.vertices[i % 3] = indices[i];
    }
    free(indices);

    parser->current_object->faces =
        wf_realloc_array(parser->current_object->faces,
                         &parser->current_object->face_cap,
                         parser->current_object->face_count, sizeof(wf_face));

    if (!parser->current_object->faces) {
      wf_set_error_with_line(parser, "Out of memory while storing face");
      return WF_ERROR_OUT_OF_MEMORY;
    }
    parser->current_object->faces[parser->current_object->face_count - 1] =
        face;
  }

  LOG_DEBUG("Parsed face with %zu vertices", idx_count);
  return WF_SUCCESS;
}

// Build full path helper
static char* wf_build_full_path(const char* base_dir, const char* filename) {
  if (!base_dir)
    return wf_strdup(filename);

  size_t len       = strlen(base_dir) + strlen(filename) + 1;
  char*  full_path = malloc(len);
  if (full_path) {
    strcpy(full_path, base_dir);
    strcat(full_path, filename);
  }
  return full_path;
}

// Cleanup helper
static void wf_cleanup_parser_state(wf_obj_parser_t* parser) {
  free(parser->line_buffer);
  free(parser->current_mtl_dir);
  free(parser->current_object_name);
}

// Handler implementations
static wf_error_t wf_handle_vertex(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  wf_vec3          v      = { 0 };
  wf_error_t       result =
      wf_parse_vertex_data(parser, line, &v, &parser->scene->vertex_count,
                           &parser->scene->vertices, &parser->scene->vertex_cap,
                           sizeof(wf_vec3));
  if (result == WF_SUCCESS) {
    LOG_DEBUG("Parsed vertex: (%.3f, %.3f, %.3f)", v.x, v.y, v.z);
  }
  return result;
}

static wf_error_t wf_handle_texcoord(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  wf_vec3          vt     = { 0, 0, 0 };
  wf_error_t       result =
      wf_parse_vertex_data(parser, line, &vt, &parser->scene->texcoord_count,
                           &parser->scene->texcoords,
                           &parser->scene->texcoord_cap, sizeof(wf_vec3));
  if (result == WF_SUCCESS) {
    LOG_DEBUG("Parsed texture coordinate: (%.3f, %.3f, %.3f)", vt.x, vt.y,
              vt.z);
  }
  return result;
}

static wf_error_t wf_handle_normal(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  wf_vec3          vn     = { 0 };
  wf_error_t       result =
      wf_parse_vertex_data(parser, line, &vn, &parser->scene->normal_count,
                           &parser->scene->normals, &parser->scene->normal_cap,
                           sizeof(wf_vec3));
  if (result == WF_SUCCESS) {
    LOG_DEBUG("Parsed normal: (%.3f, %.3f, %.3f)", vn.x, vn.y, vn.z);
  }
  return result;
}

static wf_error_t wf_handle_parameter(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  const char*      s      = line;
  wf_vec4          vp     = { 0 };
  vp.x                    = wf_parse_float(&s);
  if (*s)
    vp.y = wf_parse_float(&s);
  if (*s)
    vp.z = wf_parse_float(&s);
  if (*s)
    vp.w = wf_parse_float(&s);

  parser->scene->parameters =
      wf_realloc_array(parser->scene->parameters, &parser->scene->parameter_cap,
                       parser->scene->parameter_count, sizeof(wf_vec4));

  if (!parser->scene->parameters) {
    wf_set_error_with_line(parser, "Out of memory while parsing parameter");
    return WF_ERROR_OUT_OF_MEMORY;
  }
  parser->scene->parameters[parser->scene->parameter_count - 1] = vp;
  LOG_DEBUG("Parsed parameter: (%.3f, %.3f, %.3f, %.3f)", vp.x, vp.y, vp.z,
            vp.w);
  return WF_SUCCESS;
}

static wf_error_t wf_handle_face(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;

  wf_error_t result = wf_ensure_current_object(parser);
  if (result != WF_SUCCESS)
    return result;

  wf_vertex_index* indices   = NULL;
  size_t           idx_count = 0;
  result = wf_parse_face_indices(parser, line, &indices, &idx_count);
  if (result != WF_SUCCESS)
    return result;

  return wf_add_faces_to_object(parser, indices, idx_count);
}

static wf_error_t wf_handle_object(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  if (!parser->current_object_name)
    free(parser->current_object_name);
  parser->current_object_name = wf_strdup(line);

  parser->current_object = calloc(1, sizeof(wf_object_t));
  if (!parser->current_object) {
    wf_set_error_with_line(parser, "Out of memory while creating object");
    return WF_ERROR_OUT_OF_MEMORY;
  }

  parser->current_object->name = wf_strdup(parser->current_object_name);
  wf_add_object_to_list(parser, parser->current_object);

  LOG_DEBUG("Parsed object: %s", parser->current_object_name);
  return WF_SUCCESS;
}

static wf_error_t wf_handle_group(void* parser_ptr, const char* line) {
  return wf_handle_object(parser_ptr, line);
}

static wf_error_t wf_handle_mtllib(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser   = (wf_obj_parser_t*)parser_ptr;
  char*            mtl_path = wf_strdup(line);
  if (!mtl_path) {
    wf_set_error_with_line(parser, "Out of memory while parsing mtllib");
    return WF_ERROR_OUT_OF_MEMORY;
  }

  char* full_path = wf_build_full_path(parser->current_mtl_dir, mtl_path);
  if (!full_path)
    full_path = mtl_path;

  wf_mtl_parser_t mtl_parser = { 0 };
  mtl_parser.mtl_dir         = parser->current_mtl_dir;

  wf_error_t result =
      wf_mtl_parse_file(&mtl_parser, full_path, &parser->scene->materials,
                        &parser->scene->material_count,
                        &parser->scene->material_cap);

  if (full_path != mtl_path)
    free(full_path);
  free(mtl_path);

  if (result != WF_SUCCESS) {
    wf_set_error_with_line(parser, "Failed to load MTL file: %s", line);
  } else {
    LOG_DEBUG("Loaded MTL file: %s", line);
  }

  return result;
}

static wf_error_t wf_handle_usemtl(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;

  wf_error_t result = wf_ensure_current_object(parser);
  if (result != WF_SUCCESS)
    return result;

  free(parser->current_object->material_name);
  parser->current_object->material_name = wf_strdup(line);
  LOG_DEBUG("Set material: %s", line);
  return WF_SUCCESS;
}

static wf_error_t wf_handle_smoothing(void* parser_ptr, const char* line) {
  LOG_DEBUG("Ignoring smoothing group: %s", line);
  return WF_SUCCESS;
}

static wf_error_t wf_handle_line_elem(void* parser_ptr, const char* line) {
  LOG_DEBUG("Ignoring line element: %s", line);
  return WF_SUCCESS;
}

static wf_error_t wf_handle_freeform(void* parser_ptr, const char* line) {
  LOG_DEBUG("Ignoring free-form geometry command");
  return WF_SUCCESS;
}

// Main parsing function
wf_error_t wf_obj_parse_file(wf_obj_parser_t* parser, const char* filename) {
  LOG_INFO("Starting OBJ file parsing: %s", filename);

  parser->file = fopen(filename, "r");
  if (!parser->file) {
    LOG_ERROR("Cannot open OBJ file: %s", filename);
    return WF_ERROR_FILE_NOT_FOUND;
  }

  const char* last_slash = strrchr(filename, '/');
  if (!last_slash)
    last_slash = strrchr(filename, '\\');
  if (last_slash) {
    size_t dir_len          = last_slash - filename + 1;
    parser->current_mtl_dir = malloc(dir_len + 1);
    if (parser->current_mtl_dir) {
      strncpy(parser->current_mtl_dir, filename, dir_len);
      parser->current_mtl_dir[dir_len] = '\0';
    }
  }

  parser->line_buffer = malloc(parser->line_capacity);
  if (!parser->line_buffer) {
    fclose(parser->file);
    LOG_ERROR("Out of memory allocating line buffer");
    return WF_ERROR_OUT_OF_MEMORY;
  }

  wf_error_t result = WF_SUCCESS;
  while (fgets(parser->line_buffer, (int)parser->line_capacity, parser->file)) {
    parser->line_number++;
    char* line = wf_trim(parser->line_buffer);
    if (*line == '\0' || *line == '#')
      continue;

    const wf_command_t* cmd          = WF_COMMANDS;
    wf_line_handler_t   handler      = NULL;
    const char*         handler_line = line;

    while (cmd->command != NULL) {
      if (strncmp(line, cmd->command, cmd->command_len) == 0) {
        handler      = cmd->handler;
        handler_line = line + cmd->command_len;
        while (*handler_line == ' ' || *handler_line == '\t') {
          handler_line++;
        }
        break;
      }
      cmd++;
    }

    if (handler) {
      LOG_DEBUG("handle command %s @%zu: [%s]", cmd->command,
                parser->line_number, parser->line_buffer);
      result = handler(parser, handler_line);
    } else if (parser->options->strict_mode) {
      wf_set_error_with_line(parser, "Unsupported command: %.50s", line);
      result = WF_ERROR_UNSUPPORTED_FEATURE;
    } else {
      LOG_ERROR("Ignoring unsupported command at line %zu: %.50s",
                parser->line_number, line);
    }

    if (result != WF_SUCCESS)
      break;
  }

  fclose(parser->file);
  wf_cleanup_parser_state(parser);

  if (result == WF_SUCCESS) {
    LOG_INFO("Successfully parsed OBJ file: %s", filename);
  }

  return result;
}
