// src/wavefront.c
#include "wavefront.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "log4c.h"
#include "mtl_parser.h"
#include "obj_parser.h"

static const wf_parse_options_t DEFAULT_OPTIONS       = { .triangulate      = 1,
                                                          .merge_objects    = 0,
                                                          .load_textures    = 1,
                                                          .strict_mode      = 0,
                                                          .preserve_indices = 0,
                                                          .max_line_length  = 4096 };
static const wf_print_options_t DEFAULT_PRINT_OPTIONS = {
  .vertex_limit     = -1,
  .texcoord_limit   = -1,
  .normals_limit    = -1,
  .parameters_limit = -1,
  .materials_limit  = -1,
  .faces_limit      = -1,
};

void wf_parse_options_init(wf_parse_options_t* options) {
  if (options) {
    *options = DEFAULT_OPTIONS;
  }
}

wf_error_t wf_load_obj(const char* filename, wf_scene_t* scene,
                       const wf_parse_options_t* options) {
  if (!filename || !scene) {
    return WF_ERROR_INVALID_FORMAT;
  }

  memset(scene, 0, sizeof(wf_scene_t));

  wf_parse_options_t opts   = options ? *options : DEFAULT_OPTIONS;
  wf_obj_parser_t    parser = { 0 };
  parser.options            = &opts;
  parser.scene              = scene;
  parser.line_capacity      = opts.max_line_length;

  return wf_obj_parse_file(&parser, filename);
}

wf_error_t wf_load_mtl(const char* filename, wf_material_t** materials,
                       size_t* material_count, size_t* material_cap) {
  wf_mtl_parser_t parser = { 0 };
  return wf_mtl_parse_file(&parser, filename, materials, material_count,
                           material_cap);
}

void wf_free_scene(wf_scene_t* scene) {
  if (!scene)
    return;

  free(scene->vertices);
  free(scene->texcoords);
  free(scene->normals);
  free(scene->parameters);

  for (size_t i = 0; i < scene->material_count; i++) {
    free(scene->materials[i].name);
    free(scene->materials[i].map_Ka);
    free(scene->materials[i].map_Kd);
    free(scene->materials[i].map_Ks);
    free(scene->materials[i].map_Ns);
    free(scene->materials[i].map_d);
    free(scene->materials[i].map_Tr);
    free(scene->materials[i].bump);
    free(scene->materials[i].disp);
    free(scene->materials[i].decal);
  }
  free(scene->materials);

  wf_object_t* obj = scene->objects;
  while (obj) {
    wf_object_t* next = obj->next;
    free(obj->name);
    free(obj->faces);
    free(obj);
    obj = next;
  }

  free(scene->error_message);
  memset(scene, 0, sizeof(wf_scene_t));
}

const char* wf_get_error(const wf_scene_t* scene) {
  return scene ? scene->error_message : "Invalid scene";
}

int wf_validate_scene(const wf_scene_t* scene) {
  if (!scene)
    return 0;

  wf_object_t* obj = scene->objects;
  while (obj) {
    for (size_t i = 0; i < obj->face_count; i++) {
      for (int j = 0; j < 3; j++) {
        wf_vertex_index idx = obj->faces[i].vertices[j];
        if (idx.v_idx >= (int)scene->vertex_count && idx.v_idx != -1) {
          return 0;
        }
        if (idx.vt_idx >= (int)scene->texcoord_count && idx.vt_idx != -1) {
          return 0;
        }
        if (idx.vn_idx >= (int)scene->normal_count && idx.vn_idx != -1) {
          return 0;
        }
      }
    }
    obj = obj->next;
  }

  return 1;
}

wf_error_t wf_scene_to_triangles(const wf_scene_t* scene, wf_face** triangles,
                                 size_t* triangle_count) {
  if (!scene || !triangles || !triangle_count) {
    return WF_ERROR_INVALID_FORMAT;
  }

  size_t       total_tris = 0;
  wf_object_t* obj        = scene->objects;
  while (obj) {
    total_tris += obj->face_count;
    obj = obj->next;
  }

  if (total_tris == 0) {
    *triangles      = NULL;
    *triangle_count = 0;
    return WF_SUCCESS;
  }

  wf_face* tris = malloc(total_tris * sizeof(wf_face));
  if (!tris) {
    return WF_ERROR_OUT_OF_MEMORY;
  }

  size_t idx = 0;
  obj        = scene->objects;
  while (obj) {
    memcpy(&tris[idx], obj->faces, obj->face_count * sizeof(wf_face));
    for (size_t i = 0; i < obj->face_count; i++) {
      tris[idx + i].material_idx = obj->material_idx;
    }
    idx += obj->face_count;
    obj = obj->next;
  }

  *triangles      = tris;
  *triangle_count = total_tris;
  return WF_SUCCESS;
}

// ANSI color code
#define WF_COLOR_RESET   "\x1b[0m"
#define WF_COLOR_RED     "\x1b[31m"
#define WF_COLOR_GREEN   "\x1b[32m"
#define WF_COLOR_YELLOW  "\x1b[33m"
#define WF_COLOR_BLUE    "\x1b[34m"
#define WF_COLOR_MAGENTA "\x1b[35m"
#define WF_COLOR_CYAN    "\x1b[36m"
#define WF_COLOR_WHITE   "\x1b[37m"

// check if color output is supported
static int wf_is_terminal() {
#ifdef _WIN32
  return _isatty(_fileno(stderr));
#else
  return isatty(STDERR_FILENO);
#endif
}

// print colored header
static void wf_print_header(const char* title, const char* color) {
  if (wf_is_terminal()) {
    fprintf(stderr, "\n%s=== %s ===%s\n", color, title, WF_COLOR_RESET);
  } else {
    fprintf(stderr, "\n=== %s ===\n", title);
  }
}

// print vector
static void wf_print_vec3(const char* label, wf_vec3 v, const char* color) {
  if (wf_is_terminal()) {
    fprintf(stderr, "%s%s:%s (%.3f, %.3f, %.3f)\n", color, label,
            WF_COLOR_RESET, v.x, v.y, v.z);
  } else {
    fprintf(stderr, "%s: (%.3f, %.3f, %.3f)\n", label, v.x, v.y, v.z);
  }
}

// print 4d vector
static void wf_print_vec4(const char* label, wf_vec4 v, const char* color) {
  if (wf_is_terminal()) {
    fprintf(stderr, "%s%s:%s (%.3f, %.3f, %.3f, %.3f)\n", color, label,
            WF_COLOR_RESET, v.x, v.y, v.z, v.w);
  } else {
    fprintf(stderr, "%s: (%.3f, %.3f, %.3f, %.3f)\n", label, v.x, v.y, v.z,
            v.w);
  }
}

/**
 * @brief Print detailed scene information with colored output
 * @param scene Scene to print
 */
void wf_print_scene(const wf_scene_t* scene, wf_print_options_t* opt) {
  LOG_DEBUG("start print scene");
  if (!scene) {
    fprintf(stderr, "Error: scene is NULL\n");
    return;
  }
  if (!opt) {
    *opt = DEFAULT_PRINT_OPTIONS;
  }
  LOG_DEBUG("start print scene");

  wf_print_header("SCENE SUMMARY", WF_COLOR_WHITE);
  fprintf(stderr, "Vertices: %zu / %zu\n", scene->vertex_count,
          scene->vertex_cap);
  fprintf(stderr, "TexCoords: %zu / %zu\n", scene->texcoord_count,
          scene->texcoord_cap);
  fprintf(stderr, "Normals: %zu / %zu\n", scene->normal_count,
          scene->normal_cap);
  fprintf(stderr, "Parameters: %zu / %zu\n", scene->parameter_count,
          scene->parameter_cap);
  fprintf(stderr, "Materials: %zu / %zu\n", scene->material_count,
          scene->material_cap);

  // Count objects
  size_t       object_count = 0;
  wf_object_t* obj          = scene->objects;
  while (obj) {
    object_count++;
    obj = obj->next;
  }
  fprintf(stderr, "Objects: %zu\n", object_count);

  // Print vertices (first 10)
  if (scene->vertex_count > 0) {
    wf_print_header("VERTICES", WF_COLOR_GREEN);
    size_t max_vertices = (scene->vertex_count > 10) ? 10 : scene->vertex_count;
    for (size_t i = 0; i < max_vertices; i++) {
      wf_print_vec3("v", scene->vertices[i], WF_COLOR_GREEN);
    }
    if (scene->vertex_count > 10) {
      fprintf(stderr, "... and %zu more vertices\n", scene->vertex_count - 10);
    }
  }

  // Print texture coordinates (first 10)
  if (scene->texcoord_count > 0) {
    wf_print_header("TEXTURE COORDINATES", WF_COLOR_CYAN);
    size_t max_texcoords =
        (scene->texcoord_count > 10) ? 10 : scene->texcoord_count;
    for (size_t i = 0; i < max_texcoords; i++) {
      wf_print_vec3("vt", scene->texcoords[i], WF_COLOR_CYAN);
    }
    if (scene->texcoord_count > 10) {
      fprintf(stderr, "... and %zu more texture coordinates\n",
              scene->texcoord_count - 10);
    }
  }

  // Print normals (first 10)
  if (scene->normal_count > 0) {
    wf_print_header("NORMALS", WF_COLOR_BLUE);
    size_t max_normals = (scene->normal_count > 10) ? 10 : scene->normal_count;
    for (size_t i = 0; i < max_normals; i++) {
      wf_print_vec3("vn", scene->normals[i], WF_COLOR_BLUE);
    }
    if (scene->normal_count > 10) {
      fprintf(stderr, "... and %zu more normals\n", scene->normal_count - 10);
    }
  }

  // Print parameters (first 10)
  if (scene->parameter_count > 0) {
    wf_print_header("PARAMETERS", WF_COLOR_MAGENTA);
    size_t max_params =
        (scene->parameter_count > 10) ? 10 : scene->parameter_count;
    for (size_t i = 0; i < max_params; i++) {
      wf_print_vec4("vp", scene->parameters[i], WF_COLOR_MAGENTA);
    }
    if (scene->parameter_count > 20) {
      fprintf(stderr, "... and %zu more parameters\n",
              scene->parameter_count - 10);
    }
  }

  // Print materials
  if (scene->material_count > 0) {
    wf_print_header("MATERIALS", WF_COLOR_YELLOW);
    for (size_t i = 0; i < scene->material_count; i++) {
      wf_material_t* mat = &scene->materials[i];
      if (wf_is_terminal()) {
        fprintf(stderr, "%sMaterial %zu:%s %s\n", WF_COLOR_YELLOW, i,
                WF_COLOR_RESET, mat->name ? mat->name : "(unnamed)");
      } else {
        fprintf(stderr, "Material %zu: %s\n", i,
                mat->name ? mat->name : "(unnamed)");
      }

      if (mat->name) {
        wf_print_vec3("  Ka", mat->Ka, WF_COLOR_CYAN);
        wf_print_vec3("  Kd", mat->Kd, WF_COLOR_CYAN);
        wf_print_vec3("  Ks", mat->Ks, WF_COLOR_CYAN);
        wf_print_vec3("  Ke", mat->Ke, WF_COLOR_CYAN);
        wf_print_vec4("  Tf", mat->Tf, WF_COLOR_CYAN);
        fprintf(stderr, "  Ns: %.3f, Ni: %.3f, d: %.3f, Tr: %.3f, illum: %d\n",
                mat->Ns, mat->Ni, mat->d, mat->Tr, mat->illum);

        if (mat->map_Ka)
          fprintf(stderr, "  map_Ka: %s\n", mat->map_Ka);
        if (mat->map_Kd)
          fprintf(stderr, "  map_Kd: %s\n", mat->map_Kd);
        if (mat->map_Ks)
          fprintf(stderr, "  map_Ks: %s\n", mat->map_Ks);
        if (mat->map_Ns)
          fprintf(stderr, "  map_Ns: %s\n", mat->map_Ns);
        if (mat->map_d)
          fprintf(stderr, "  map_d: %s\n", mat->map_d);
        if (mat->map_Tr)
          fprintf(stderr, "  map_Tr: %s\n", mat->map_Tr);
        if (mat->bump)
          fprintf(stderr, "  bump: %s\n", mat->bump);
        if (mat->disp)
          fprintf(stderr, "  disp: %s\n", mat->disp);
        if (mat->decal)
          fprintf(stderr, "  decal: %s\n", mat->decal);
      }
    }
  }

  // Print objects and faces
  if (object_count > 0) {
    wf_print_header("OBJECTS & FACES", WF_COLOR_MAGENTA);
    obj                      = scene->objects;
    size_t obj_index         = 0;
    size_t global_face_index = 0;
    while (obj) {
      if (wf_is_terminal()) {
        fprintf(stderr, "%sObject %zu:%s %s (faces: %zu / %zu)\n",
                WF_COLOR_MAGENTA, obj_index, WF_COLOR_RESET,
                obj->name ? obj->name : "(unnamed)", obj->face_count,
                obj->face_cap);
      } else {
        fprintf(stderr, "Object %zu: %s (faces: %zu / %zu)\n", obj_index,
                obj->name ? obj->name : "(unnamed)", obj->face_count,
                obj->face_cap);
      }

      if (-1 != obj->material_idx) {
        fprintf(stderr, "  Material: %s, index: %zu\n",
                scene->materials[obj->material_idx].name, obj->material_idx);
      } else {
        fprintf(stderr, "  Missing Material\n");
      }

      // Print first 5 faces
      size_t max_faces = (obj->face_count > 5) ? 5 : obj->face_count;
      for (size_t face_i = 0; face_i < max_faces; face_i++) {
        wf_face* face = &obj->faces[face_i];
        fprintf(stderr, "  Face %zu|%zu : [", face_i,
                global_face_index + face_i);
        for (int v = 0; v < 3; v++) {
          wf_vertex_index idx = face->vertices[v];
          fprintf(stderr, "%d/%d/%d", idx.v_idx, idx.vt_idx, idx.vn_idx);
          if (v < 2)
            fprintf(stderr, ", ");
        }
        fprintf(stderr, "]\n");
      }
      if (obj->face_count > 5) {
        fprintf(stderr, "  ... and %zu more faces\n", obj->face_count - 5);
      }
      global_face_index += obj->face_count;

      obj = obj->next;
      obj_index++;
    }
  }

  // Print error message if exists
  if (scene->error_message) {
    wf_print_header("ERROR MESSAGE", WF_COLOR_RED);
    fprintf(stderr, "%s\n", scene->error_message);
  }

  wf_print_header("END OF SCENE", WF_COLOR_WHITE);
}
