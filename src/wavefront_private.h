// src/wavefront_private.h
#ifndef WAVEFRONT_PRIVATE_H
#define WAVEFRONT_PRIVATE_H

#include <stdlib.h>
#include "wavefront.h"

// Internal parser state
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
} wf_parser_t;

// Internal functions
static wf_error_t wf_parse_line(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_vertex(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_texcoord(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_normal(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_face(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_object(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_group(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_mtllib(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_usemtl(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_freeform(wf_parser_t* parser, const char* line);

// Memory management
static void* wf_realloc_array(void* ptr, size_t* capacity, size_t element_size);
static char* wf_strdup(const char* str);
static void  wf_set_error(wf_scene_t* scene, const char* format, ...);

// Free-form geometry parsing
static wf_error_t wf_parse_curve(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_surface(wf_parser_t* parser, const char* line);
static wf_error_t wf_parse_parameter(wf_parser_t* parser, const char* line);

// Utility functions
static char* wf_trim(char* s);
static int   wf_is_whitespace(char c);
static int   wf_is_digit(char c);
static float wf_parse_float(const char** s);
static int   wf_parse_int(const char** s);
static int   wf_resolve_index(int idx, size_t count, int preserve_1_based);

#endif // WAVEFRONT_PRIVATE_H
