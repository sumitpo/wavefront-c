// src/obj_parser.h
#ifndef OBJ_PARSER_H
#define OBJ_PARSER_H

#include <stdio.h>
#include "wavefront.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef wf_error_t (*wf_line_handler_t)(void* parser, const char* line);

typedef struct {
  const char*       command;
  size_t            command_len;
  wf_line_handler_t handler;
} wf_command_t;

typedef struct {
  FILE*                     file;
  char*                     line_buffer;
  size_t                    line_capacity;
  size_t                    line_number;
  const wf_parse_options_t* options;
  wf_scene_t*               scene;
  char*                     current_mtl_dir;
  char*                     current_object_name;
  wf_object_t*              current_object;
} wf_obj_parser_t;

wf_error_t wf_obj_parse_file(wf_obj_parser_t* parser, const char* filename);

// Forward declarations
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

// Helper function declarations
static wf_error_t wf_ensure_current_object(wf_obj_parser_t* parser);
static wf_error_t wf_add_object_to_list(wf_obj_parser_t* parser,
                                        wf_object_t*     obj);
static wf_error_t wf_parse_vertex_data(wf_obj_parser_t* parser,
                                       const char* line, wf_vec3* vertex,
                                       size_t* count, wf_vec3** array,
                                       size_t* cap, size_t elem_size);
static wf_error_t wf_parse_face_indices(wf_obj_parser_t*  parser,
                                        const char*       line,
                                        wf_vertex_index** indices,
                                        size_t*           idx_count);
static wf_error_t wf_add_faces_to_object(wf_obj_parser_t* parser,
                                         wf_vertex_index* indices,
                                         size_t           idx_count);
static char* wf_build_full_path(const char* base_dir, const char* filename);
static void  wf_cleanup_parser_state(wf_obj_parser_t* parser);

#ifdef __cplusplus
}
#endif

#endif // OBJ_PARSER_H
