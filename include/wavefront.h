/**
 * @file wavefront.h
 * @brief Complete Wavefront OBJ/MTL parser
 * @author Your Name
 * @version 1.0.0
 */

#ifndef WAVEFRONT_H
#define WAVEFRONT_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes
 */
typedef enum {
  WF_SUCCESS = 0,
  WF_ERROR_FILE_NOT_FOUND,
  WF_ERROR_INVALID_FORMAT,
  WF_ERROR_OUT_OF_MEMORY,
  WF_ERROR_UNSUPPORTED_FEATURE,
  WF_ERROR_INTERNAL
} wf_error_t;

/**
 * @brief Vector types
 */
typedef struct {
  float x, y, z;
} wf_vec3;

typedef struct {
  float x, y, z, w;
} wf_vec4;

/**
 * @brief Vertex index structure
 * Supports all OBJ face formats:
 * - v
 * - v/vt
 * - v//vn
 * - v/vt/vn
 */
typedef struct {
  int v_idx;  /**< Vertex index (1-based in OBJ, 0-based internally) */
  int vt_idx; /**< Texture coordinate index (-1 if not present) */
  int vn_idx; /**< Normal index (-1 if not present) */
} wf_vertex_index;

/**
 * @brief Face structure
 * Automatically triangulated during parsing
 */
typedef struct {
  wf_vertex_index vertices[3]; /**< Triangle vertices */
} wf_face;

/**
 * @brief Material structure
 * Supports all MTL properties
 */
typedef struct {
  char* name; /**< Material name */

  /* Color properties */
  wf_vec3 Ka; /**< Ambient color */
  wf_vec3 Kd; /**< Diffuse color */
  wf_vec3 Ks; /**< Specular color */
  wf_vec4 Tf; /**< Transmission filter */

  /* Scalar properties */
  float Ns;    /**< Specular exponent */
  float Ni;    /**< Optical density (index of refraction) */
  float d;     /**< Dissolve (alpha) */
  float Tr;    /**< Transparency (1 - d) */
  int   illum; /**< Illumination model */

  /* Texture maps */
  char* map_Ka; /**< Ambient texture */
  char* map_Kd; /**< Diffuse texture */
  char* map_Ks; /**< Specular texture */
  char* map_Ns; /**< Specular exponent texture */
  char* map_d;  /**< Dissolve texture */
  char* map_Tr; /**< Transparency texture */
  char* bump;   /**< Bump map */
  char* disp;   /**< Displacement map */
  char* decal;  /**< Decal texture */

  /* Texture options */
  struct {
    int   clamp;   /**< Clamp texture */
    float mm[2];   /**< Min/max values */
    int   imfchan; /**< Channel for bump maps */
    char* type;    /**< Texture type */
  } map_options;
} wf_material_t;

/**
 * @brief Object/Group structure
 * OBJ files can have multiple objects/groups
 */
typedef struct wf_object_s {
  char*               name;          /**< Object/group name */
  wf_face*            faces;         /**< Array of faces */
  size_t              face_count;    /**< Number of faces */
  size_t              face_cap;      /**< Cap of faces */
  char*               material_name; /**< Current material name */
  struct wf_object_s* next;          /**< Next object in list */
} wf_object_t;

/**
 * @brief Main scene structure
 */
typedef struct {
  /* Geometry data */
  wf_vec3* vertices;   /**< Vertex positions */
  wf_vec3* texcoords;  /**< Texture coordinates */
  wf_vec3* normals;    /**< Vertex normals */
  wf_vec4* parameters; /**< Free-form curve/surface parameters */

  size_t vertex_count;    /**< Number of vertices */
  size_t texcoord_count;  /**< Number of texture coordinates */
  size_t normal_count;    /**< Number of normals */
  size_t parameter_count; /**< Number of parameters */

  size_t vertex_cap;    /**< Capacity of vertices */
  size_t texcoord_cap;  /**< Capacity of texture coordinates */
  size_t normal_cap;    /**< Capacity of normals */
  size_t parameter_cap; /**< Capacity of parameters */

  /* Materials */
  wf_material_t* materials;      /**< Array of materials */
  size_t         material_count; /**< Number of materials */
  size_t         material_cap;   /**< Capacity of materials */

  /* Objects/Groups */
  wf_object_t* objects; /**< Linked list of objects */

  /* Free-form geometry (NURBS, curves, surfaces) */
  struct {
    /* Curve and surface elements */
    struct wf_curve_s*   curves;
    struct wf_surface_s* surfaces;
    size_t               curve_count;
    size_t               surface_count;
  } freeform;

  /* Error handling */
  char* error_message; /**< Last error message */
} wf_scene_t;

/**
 * @brief Parse options
 */
typedef struct {
  int    triangulate;      /**< Triangulate all polygons (default: 1) */
  int    merge_objects;    /**< Merge all objects into one (default: 0) */
  int    load_textures;    /**< Load texture files (default: 1) */
  int    strict_mode;      /**< Fail on unsupported features (default: 0) */
  int    preserve_indices; /**< Keep 1-based indices (default: 0) */
  size_t max_line_length;  /**< Maximum line length (default: 4096) */
} wf_parse_options_t;

/**
 * @brief Initialize parse options with defaults
 */
void wf_parse_options_init(wf_parse_options_t* options);

/**
 * @brief Load Wavefront OBJ file
 * @param filename Path to OBJ file
 * @param scene Output scene structure
 * @param options Parse options (can be NULL for defaults)
 * @return WF_SUCCESS on success, error code otherwise
 */
wf_error_t wf_load_obj(const char* filename, wf_scene_t* scene,
                       const wf_parse_options_t* options);

/**
 * @brief Load MTL file separately
 * @param filename Path to MTL file
 * @param materials Output materials array
 * @param material_count Output material count
 * @return WF_SUCCESS on success
 */
wf_error_t wf_load_mtl(const char* filename, wf_material_t** materials,
                       size_t* material_count, size_t* material_cap);

/**
 * @brief Free scene memory
 * @param scene Scene to free
 */
void wf_free_scene(wf_scene_t* scene);

/**
 * @brief Get error message
 * @param scene Scene that encountered error
 * @return Error message string
 */
const char* wf_get_error(const wf_scene_t* scene);

/**
 * @brief Validate scene integrity
 * @param scene Scene to validate
 * @return 1 if valid, 0 if invalid
 */
int wf_validate_scene(const wf_scene_t* scene);

/**
 * @brief Convert scene to triangles only (for ray tracing)
 * @param scene Input scene
 * @param triangles Output triangle array
 * @param triangle_count Output triangle count
 * @return WF_SUCCESS on success
 */
wf_error_t wf_scene_to_triangles(const wf_scene_t* scene, wf_face** triangles,
                                 size_t* triangle_count);

/**
 * @brief Print options
 */
typedef struct {
  int vertex_limit;
  int texcoord_limit;
  int normals_limit;
  int parameters_limit;
  int materials_limit;
  int faces_limit;
} wf_print_options_t;

/**
 * @brief Print detailed scene information with colored output
 * @param scene Scene to print
 */
void wf_print_scene(const wf_scene_t* scene, wf_print_options_t* opt);
#ifdef __cplusplus
}
#endif

#endif // WAVEFRONT_H
