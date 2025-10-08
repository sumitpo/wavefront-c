// src/lib.c
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <io.h>
#else
#endif

char* wf_strdup(const char* str) {
  if (!str)
    return NULL;
#ifdef _WIN32
  size_t len  = strlen(str);
  char*  copy = (char*)malloc(len + 1);
  if (copy) {
    memcpy(copy, str, len + 1);
  }
  return copy;
#else
  return strdup(str);
#endif
}

char* wf_trim(char* s) {
  if (!s)
    return s;

  while (isspace((unsigned char)*s)) {
    s++;
  }

  if (*s == '\0') {
    return s;
  }

  char* end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) {
    end--;
  }
  *(end + 1) = '\0';

  return s;
}

float wf_parse_float(const char** s) {
  if (!s || !*s)
    return 0.0f;
  const char* sPtr = *s;

  while (*sPtr == ' ' || *sPtr == '\t') {
    sPtr++;
  }
  char* end;
  float val = strtof(sPtr, &end);
  if (end != sPtr)
    *s = end;
  else
    *s = sPtr;
  return val;
}

int wf_parse_int(const char** s) {
  if (!s || !*s)
    return 0;
  char* end;
  long  val = strtol(*s, &end, 10);
  *s        = end;
  return (int)val;
}

void* wf_realloc_array(void* ptr, size_t* capacity, size_t count,
                       size_t element_size) {
  if (!capacity)
    return NULL;
  if (count < *capacity)
    return ptr;

  size_t new_capacity = *capacity + 16;
  void*  new_ptr      = realloc(ptr, new_capacity * element_size);
  if (new_ptr) {
    *capacity = new_capacity;
  }
  return new_ptr;
}

char* wf_strndup(const char* str, size_t n) {
  if (!str)
    return NULL;

  size_t      len = 0;
  const char* p   = str;
  while (len < n && *p != '\0') {
    len++;
    p++;
  }

  char* copy = (char*)malloc(len + 1);
  if (copy) {
    memcpy(copy, str, len);
    copy[len] = '\0';
  }
  return copy;
}

int wf_strcasecmp(const char* s1, const char* s2) {
#ifdef _WIN32
  return _stricmp(s1, s2);
#else
  return strcasecmp(s1, s2);
#endif
}

size_t wf_strlen(const char* s) {
  return s ? strlen(s) : 0;
}
