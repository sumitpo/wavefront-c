// src/obj_parser.c
#include "obj_parser.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"
#include "log.h"
#include "mtl_parser.h"

static wf_error_t wf_handle_vertex(void* parser, const char* line);
static wf_error_t wf_handle_texcoord(void* parser, const char* line);
static wf_error_t wf_handle_normal(void* parser, const char* line);
static wf_error_t wf_handle_parameter(void* parser, const char* line);
static wf_error_t wf_handle_face(void* parser, const char* line);
static wf_error_t wf_handle_object(void* parser, const char* line);
static wf_error_t wf_handle_group(void* parser, const char* line);
static wf_error_t wf_handle_mtllib(void* parser, const char* line);
static wf_error_t wf_handle_usemtl(void* parser, const char* line);
static wf_error_t wf_handle_smoothing(void* parser, const char* line);
static wf_error_t wf_handle_line_elem(void* parser, const char* line);
static wf_error_t wf_handle_freeform(void* parser, const char* line);

static const wf_command_t WF_COMMANDS[] = {
  { "v ",      2, wf_handle_vertex    },
  { "vt ",     3, wf_handle_texcoord  },
  { "vn ",     3, wf_handle_normal    },
  { "vp ",     3, wf_handle_parameter },
  { "f ",      2, wf_handle_face      },
  { "o ",      2, wf_handle_object    },
  { "g ",      2, wf_handle_group     },
  { "mtllib ", 7, wf_handle_mtllib    },
  { "usemtl ", 7, wf_handle_usemtl    },
  { "s ",      2, wf_handle_smoothing },
  { "l ",      2, wf_handle_line_elem },
  { "cstype ", 7, wf_handle_freeform  },
  { "deg ",    4, wf_handle_freeform  },
  { "bmat ",   5, wf_handle_freeform  },
  { "step ",   5, wf_handle_freeform  },
  { "curv ",   5, wf_handle_freeform  },
  { "surf ",   5, wf_handle_freeform  },
  { "parm ",   5, wf_handle_freeform  },
  { "trim ",   5, wf_handle_freeform  },
  { "hole ",   5, wf_handle_freeform  },
  { "scrv ",   5, wf_handle_freeform  },
  { "sp ",     3, wf_handle_freeform  },
  { "end ",    4, wf_handle_freeform  },
  { NULL,      0, NULL                }
};

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

// 辅助函数：解析索引（0-based 或 1-based）
static int resolve_index(int idx, size_t count, int preserve_1_based) {
  if (preserve_1_based) {
    return idx;
  }
  if (idx > count)
    return -1;
  if (idx + count < 0)
    return -1;
  if (idx > 0) {
    return idx - 1; // 转换为 0-based
  } else if (idx < 0) {
    return (int)(count + idx); // 负索引处理
  }
  return -1; // 0 是无效索引
}

static wf_error_t wf_parse_face_index_helper(const char*      token,
                                             wf_vertex_index* idx,
                                             size_t v_count, size_t vt_count,
                                             size_t vn_count,
                                             int    preserve_1_based) {
  // 初始化所有索引为 -1（无效）
  *idx = (wf_vertex_index){ -1, -1, -1 };

  if (!token || !token[0]) {
    return WF_SUCCESS;
  }

  // 创建可变副本用于解析（避免修改原始字符串）
  char* token_copy = wf_strdup(token);
  if (!token_copy) {
    return WF_ERROR_OUT_OF_MEMORY;
  }

  char* v_str  = token_copy;
  char* vt_str = NULL;
  char* vn_str = NULL;

  // 解析 v/vt/vn 格式
  char* slash1 = strchr(v_str, '/');
  if (slash1) {
    *slash1 = '\0';
    vt_str  = slash1 + 1;

    char* slash2 = strchr(vt_str, '/');
    if (slash2) {
      *slash2 = '\0';
      vn_str  = slash2 + 1;
    } else {
      // v//vn 格式（vt_str 实际是 vn_str）
      vn_str = vt_str;
      vt_str = NULL;
    }
  }

  // 解析顶点索引
  if (v_str[0]) {
    char* endptr;
    long  v_idx = strtol(v_str, &endptr, 10);
    if (endptr != v_str && *endptr == '\0') {
      idx->v_idx = resolve_index((int)v_idx, v_count, preserve_1_based);
    }
  }

  // 解析纹理坐标索引
  if (vt_str && vt_str[0]) {
    char* endptr;
    long  vt_idx = strtol(vt_str, &endptr, 10);
    if (endptr != vt_str && *endptr == '\0') {
      idx->vt_idx = resolve_index((int)vt_idx, vt_count, preserve_1_based);
    }
  }

  // 解析法线索引
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

static wf_error_t wf_handle_vertex(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  const char*      s      = line;
  wf_vec3          v      = { 0 };
  v.x                     = wf_parse_float(&s);
  if (*s)
    v.y = wf_parse_float(&s);
  if (*s)
    v.z = wf_parse_float(&s);
  parser->scene->vertex_count += 1;

  parser->scene->vertices =
      wf_realloc_array(parser->scene->vertices, &parser->scene->vertex_cap,
                       parser->scene->vertex_count, sizeof(wf_vec3));
  if (!parser->scene->vertices) {
    wf_set_error_with_line(parser, "Out of memory while parsing vertex");
    return WF_ERROR_OUT_OF_MEMORY;
  }
  parser->scene->vertices[parser->scene->vertex_count - 1] = v;
  LOG_DEBUG("Parsed vertex: (%.3f, %.3f, %.3f)", v.x, v.y, v.z);
  return WF_SUCCESS;
}

static wf_error_t wf_handle_texcoord(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  const char*      s      = line;
  wf_vec3          vt     = { 0, 0, 0 };
  vt.x                    = wf_parse_float(&s);
  if (*s)
    vt.y = wf_parse_float(&s);
  if (*s)
    vt.z = wf_parse_float(&s);

  parser->scene->texcoords =
      wf_realloc_array(parser->scene->texcoords, &parser->scene->texcoord_cap,
                       parser->scene->texcoord_count, sizeof(wf_vec3));
  if (!parser->scene->texcoords) {
    wf_set_error_with_line(parser,
                           "Out of memory while parsing texture coordinate");
    return WF_ERROR_OUT_OF_MEMORY;
  }
  parser->scene->texcoords[parser->scene->texcoord_count - 1] = vt;
  LOG_DEBUG("Parsed texture coordinate: (%.3f, %.3f, %.3f)", vt.x, vt.y, vt.z);
  return WF_SUCCESS;
}

static wf_error_t wf_handle_normal(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  const char*      s      = line;

  wf_vec3 vn = { 0 };
  vn.x       = wf_parse_float(&s);
  vn.y       = wf_parse_float(&s);
  vn.z       = wf_parse_float(&s);

  parser->scene->normals =
      wf_realloc_array(parser->scene->normals, &parser->scene->normal_cap,
                       parser->scene->normal_count, sizeof(wf_vec3));
  if (!parser->scene->normals) {
    wf_set_error_with_line(parser, "Out of memory while parsing normal");
    return WF_ERROR_OUT_OF_MEMORY;
  }
  parser->scene->normals[parser->scene->normal_count - 1] = vn;
  LOG_DEBUG("Parsed normal: (%.3f, %.3f, %.3f)", vn.x, vn.y, vn.z);
  return WF_SUCCESS;
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

  if (!parser->current_object) {
    parser->current_object = calloc(1, sizeof(wf_object_t));
    if (!parser->current_object) {
      wf_set_error_with_line(parser,
                             "Out of memory while creating object for face");
      return WF_ERROR_OUT_OF_MEMORY;
    }
    if (parser->current_object_name) {
      parser->current_object->name = wf_strdup(parser->current_object_name);
    }
    if (!parser->scene->objects) {
      parser->scene->objects = parser->current_object;
    } else {
      wf_object_t* last = parser->scene->objects;
      while (last->next)
        last = last->next;
      last->next = parser->current_object;
    }
  }

  char* buf = wf_strdup(line);
  if (!buf) {
    wf_set_error_with_line(parser, "Out of memory while parsing face");
    return WF_ERROR_OUT_OF_MEMORY;
  }

  wf_vertex_index* indices   = NULL;
  size_t           idx_count = 0, idx_cap = 0;
  char*            token = strtok(buf, " \t");
  while (token) {
    if (idx_count >= idx_cap) {
      idx_cap = idx_cap ? idx_cap * 2 : 8;
      wf_vertex_index* new_indices =
          realloc(indices, idx_cap * sizeof(wf_vertex_index));
      if (!new_indices) {
        free(indices);
        free(buf);
        wf_set_error_with_line(parser,
                               "Out of memory while parsing face indices");
        return WF_ERROR_OUT_OF_MEMORY;
      }
      indices = new_indices;
    }
    wf_parse_face_index_helper(token, &indices[idx_count],
                               parser->scene->vertex_count,
                               parser->scene->texcoord_count,
                               parser->scene->normal_count,
                               parser->options->preserve_indices);
    idx_count++;
    token = strtok(NULL, " \t");
  }
  free(buf);

  LOG_DEBUG("index count is %zu", idx_count);

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
    LOG_DEBUG("current face count is %zu", parser->current_object->face_count);
    free(tris);
    LOG_INFO("idx count is %zu, after triangulate get %zu faces", idx_count,
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

static wf_error_t wf_handle_object(void* parser_ptr, const char* line) {
  wf_obj_parser_t* parser = (wf_obj_parser_t*)parser_ptr;
  free(parser->current_object_name);
  parser->current_object_name = wf_strdup(line);
  parser->current_object      = calloc(1, sizeof(wf_object_t));
  if (!parser->current_object) {
    wf_set_error_with_line(parser, "Out of memory while creating object");
    return WF_ERROR_OUT_OF_MEMORY;
  }
  parser->current_object->name = wf_strdup(parser->current_object_name);
  if (!parser->scene->objects) {
    parser->scene->objects = parser->current_object;
  } else {
    wf_object_t* last = parser->scene->objects;
    while (last->next)
      last = last->next;
    last->next = parser->current_object;
  }
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

  char* full_path = NULL;
  if (parser->current_mtl_dir) {
    size_t len = wf_strlen(parser->current_mtl_dir) + wf_strlen(mtl_path) + 1;
    full_path  = malloc(len);
    if (full_path) {
      strcpy(full_path, parser->current_mtl_dir);
      strcat(full_path, mtl_path);
    }
  } else {
    full_path = mtl_path;
  }

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
  if (!parser->current_object) {
    parser->current_object = calloc(1, sizeof(wf_object_t));
    if (!parser->current_object) {
      wf_set_error_with_line(parser,
                             "Out of memory while creating object for usemtl");
      return WF_ERROR_OUT_OF_MEMORY;
    }
    if (!parser->scene->objects) {
      parser->scene->objects = parser->current_object;
    } else {
      wf_object_t* last = parser->scene->objects;
      while (last->next)
        last = last->next;
      last->next = parser->current_object;
    }
  }
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
  LOG_TRACE("line cap is %zu", parser->line_capacity);

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
      LOG_WARN("Ignoring unsupported command at line %zu: %.50s",
               parser->line_number, line);
    }

    if (result != WF_SUCCESS) {
      break;
    }
  }

  fclose(parser->file);
  free(parser->line_buffer);
  free(parser->current_mtl_dir);
  free(parser->current_object_name);

  if (result == WF_SUCCESS) {
    LOG_INFO("Successfully parsed OBJ file: %s", filename);
  }

  return result;
}
