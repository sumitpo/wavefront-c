// examples/loader.c
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "wavefront.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <obj_file>\n", argv[0]);
    return 1;
  }

  // Initialize logging
  log_init(LOG_LEVEL_DEBUG);

  const char*        filename = argv[1];
  wf_scene_t         scene;
  wf_parse_options_t options;
  wf_parse_options_init(&options);

  LOG_INFO("Loading Wavefront file: %s", filename);

  wf_error_t err = wf_load_obj(filename, &scene, &options);
  if (err != WF_SUCCESS) {
    LOG_ERROR("Failed to load OBJ file: %s", wf_get_error(&scene));
    wf_free_scene(&scene);
    return 1;
  }

  // Print scene summary
  LOG_INFO("Scene loaded successfully!");
  LOG_INFO("Vertices: %zu", scene.vertex_count);
  LOG_INFO("Texture coordinates: %zu", scene.texcoord_count);
  LOG_INFO("Normals: %zu", scene.normal_count);
  LOG_INFO("Materials: %zu", scene.material_count);

  // Count total faces
  size_t       total_faces = 0;
  wf_object_t* obj         = scene.objects;
  while (obj) {
    total_faces += obj->face_count;
    obj = obj->next;
  }
  LOG_INFO("Total faces: %zu", total_faces);

  // Validate scene
  if (wf_validate_scene(&scene)) {
    LOG_INFO("Scene validation passed!");
  } else {
    LOG_WARN("Scene validation failed!");
  }

  wf_print_options_t opt;
  wf_print_scene(&scene, &opt);

  // Convert to triangles for ray tracing
  wf_face* triangles      = NULL;
  size_t   triangle_count = 0;
  err = wf_scene_to_triangles(&scene, &triangles, &triangle_count);
  if (err == WF_SUCCESS) {
    LOG_INFO("Converted to %zu triangles", triangle_count);
    free(triangles);
  }

  wf_free_scene(&scene);
  LOG_INFO("Done!");
  return 0;
}
