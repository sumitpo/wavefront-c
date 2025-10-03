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

#ifdef __cplusplus
}
#endif

#endif // OBJ_PARSER_H
