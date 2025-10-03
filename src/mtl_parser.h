// src/mtl_parser.h
#ifndef MTL_PARSER_H
#define MTL_PARSER_H

#include <stdio.h>
#include "wavefront.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  FILE*  file;
  size_t line_number;
  char*  mtl_dir;
} wf_mtl_parser_t;
wf_error_t wf_mtl_parse_file(wf_mtl_parser_t* parser, const char* filename,
                             wf_material_t** materials, size_t* material_count,
                             size_t* material_cap);

#ifdef __cplusplus
}
#endif

#endif // MTL_PARSER_H
