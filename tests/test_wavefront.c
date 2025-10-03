// tests/test_wavefront.c
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>
#include "log.h"
#include "wavefront.h"

// Test data
static const char* test_cube_obj = "v -1 -1 -1\n"
                                   "v 1 -1 -1\n"
                                   "v 1 1 -1\n"
                                   "v -1 1 -1\n"
                                   "v -1 -1 1\n"
                                   "v 1 -1 1\n"
                                   "v 1 1 1\n"
                                   "v -1 1 1\n"
                                   "f 1 2 3 4\n"
                                   "f 2 6 7 3\n"
                                   "f 6 5 8 7\n"
                                   "f 5 1 4 8\n"
                                   "f 4 3 7 8\n"
                                   "f 5 6 2 1\n";

static const char* test_cube_mtl = "newmtl white\n"
                                   "Kd 1.0 1.0 1.0\n"
                                   "illum 2\n";

// Helper function to create test files
static void create_test_file(const char* filename, const char* content) {
  FILE* f = fopen(filename, "w");
  if (f) {
    fwrite(content, 1, strlen(content), f);
    fclose(f);
  }
}

// Test fixture setup
static int setup_test_scene(void** state) {
// Create test directory
#ifdef _WIN32
  system("mkdir test_data 2>nul");
#else
  system("mkdir -p test_data");
#endif

  // Create test files
  create_test_file("test_data/cube.obj", test_cube_obj);
  create_test_file("test_data/cube.mtl", test_cube_mtl);

  wf_scene_t* scene = malloc(sizeof(wf_scene_t));
  *state            = scene;
  return 0;
}

// Test fixture teardown
static int teardown_test_scene(void** state) {
  wf_scene_t* scene = *state;
  if (scene) {
    wf_free_scene(scene);
    free(scene);
  }

// Clean up test files
#ifdef _WIN32
  system("rmdir /s /q test_data 2>nul");
#else
  system("rm -rf test_data");
#endif

  return 0;
}

// Test: Basic OBJ loading
static void test_load_basic_obj(void** state) {
  wf_scene_t* scene = *state;

  wf_parse_options_t options;
  wf_parse_options_init(&options);

  wf_error_t err = wf_load_obj("test_data/cube.obj", scene, &options);
  assert_int_equal(err, WF_SUCCESS);

  // Verify geometry
  assert_int_equal(scene->vertex_count, 8);
  assert_int_equal(scene->texcoord_count, 0);
  assert_int_equal(scene->normal_count, 0);

  // Verify objects
  assert_non_null(scene->objects);
  assert_int_equal(scene->objects->face_count, 12);

  // Verify first vertex
  assert_float_equal(scene->vertices[0].x, -1.0f, 1e-5);
  assert_float_equal(scene->vertices[0].y, -1.0f, 1e-5);
  assert_float_equal(scene->vertices[0].z, -1.0f, 1e-5);
}

// Test: MTL loading
static void test_load_mtl(void** state) {
  wf_material_t* materials      = NULL;
  size_t         material_count = 0;
  size_t         material_cap   = 0;

  wf_error_t err = wf_load_mtl("test_data/cube.mtl", &materials,
                               &material_count, &material_cap);
  assert_int_equal(err, WF_SUCCESS);
  assert_int_equal(material_count, 1);

  assert_string_equal(materials[0].name, "white");
  assert_float_equal(materials[0].Kd.x, 1.0f, 1e-5);
  assert_float_equal(materials[0].Kd.y, 1.0f, 1e-5);
  assert_float_equal(materials[0].Kd.z, 1.0f, 1e-5);
  assert_int_equal(materials[0].illum, 2);

  // Cleanup
  free(materials[0].name);
  free(materials);
}

// Test: Scene validation
static void test_scene_validation(void** state) {
  wf_scene_t* scene = *state;

  wf_parse_options_t options;
  wf_parse_options_init(&options);

  wf_error_t err = wf_load_obj("test_data/cube.obj", scene, &options);
  assert_int_equal(err, WF_SUCCESS);

  // Should be valid
  assert_true(wf_validate_scene(scene));
}

// Test: Triangle conversion
static void test_triangle_conversion(void** state) {
  wf_scene_t* scene = *state;

  wf_parse_options_t options;
  wf_parse_options_init(&options);
  options.triangulate = 1;

  wf_error_t err = wf_load_obj("test_data/cube.obj", scene, &options);
  assert_int_equal(err, WF_SUCCESS);

  wf_face* triangles      = NULL;
  size_t   triangle_count = 0;
  err = wf_scene_to_triangles(scene, &triangles, &triangle_count);
  assert_int_equal(err, WF_SUCCESS);
  assert_int_equal(triangle_count, 12);

  // Verify first triangle
  assert_int_equal(triangles[0].vertices[0].v_idx, 0);
  assert_int_equal(triangles[0].vertices[1].v_idx, 1);
  assert_int_equal(triangles[0].vertices[2].v_idx, 2);

  free(triangles);
}

// Test: Error handling - file not found
static void test_file_not_found(void** state) {
  wf_scene_t scene;
  memset(&scene, 0, sizeof(scene));

  wf_parse_options_t options;
  wf_parse_options_init(&options);

  wf_error_t err = wf_load_obj("nonexistent.obj", &scene, &options);
  assert_int_equal(err, WF_ERROR_FILE_NOT_FOUND);

  // Error message should contain line info
  const char* error = wf_get_error(&scene);
  assert_null(error);
}

// Test: Invalid face
static void test_invalid_face(void** state) {
  const char* invalid_obj = "v 1 2 3\nf 999 999 999\n";
  create_test_file("test_data/invalid.obj", invalid_obj);

  wf_scene_t scene;
  memset(&scene, 0, sizeof(scene));

  wf_parse_options_t options;
  wf_parse_options_init(&options);

  wf_error_t err = wf_load_obj("test_data/invalid.obj", &scene, &options);
  // Should not fail, just skip invalid face
  assert_int_equal(err, WF_SUCCESS);

  wf_free_scene(&scene);
}

int main(void) {
  log_init(LOG_LEVEL_FATAL); // Quiet logging for tests
  // log_init(LOG_LEVEL_DEBUG); // Quiet logging for tests

  const struct CMUnitTest tests[] = {
    cmocka_unit_test_setup_teardown(test_load_basic_obj, setup_test_scene,
                                    teardown_test_scene),
    cmocka_unit_test_setup_teardown(test_load_mtl, setup_test_scene,
                                    teardown_test_scene),
    cmocka_unit_test_setup_teardown(test_scene_validation, setup_test_scene,
                                    teardown_test_scene),
    cmocka_unit_test_setup_teardown(test_triangle_conversion, setup_test_scene,
                                    teardown_test_scene),
    cmocka_unit_test(test_file_not_found),
    cmocka_unit_test_setup_teardown(test_invalid_face, setup_test_scene,
                                    teardown_test_scene),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
