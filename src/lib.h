// src/lib.h
#ifndef LIB_H
#define LIB_H

#include <stddef.h>

char*  wf_strdup(const char* str);
char*  wf_trim(char* s);
float  wf_parse_float(const char** s);
int    wf_parse_int(const char** s);
void*  wf_realloc_array(void* ptr, size_t* capacity, size_t count,
                        size_t element_size);
char*  wf_strndup(const char* str, size_t n);
int    wf_strcasecmp(const char* s1, const char* s2);
size_t wf_strlen(const char* s);

#endif // LIB_H
