// ========================================================================== //
// Copyright (c) 2017-2018 The University of Texas at Austin.                 //
// All rights reserved.                                                       //
//                                                                            //
// Licensed under the Apache License, Version 2.0 (the "License");            //
// you may not use this file except in compliance with the License.           //
// A copy of the License is included with this software in the file LICENSE.  //
// If your copy does not contain the License, you may obtain a copy of the    //
// License at:                                                                //
//                                                                            //
//     https://www.apache.org/licenses/LICENSE-2.0                            //
//                                                                            //
// Unless required by applicable law or agreed to in writing, software        //
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT  //
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.           //
// See the License for the specific language governing permissions and        //
// limitations under the License.                                             //
//                                                                            //
// ========================================================================== //

#pragma once

#include <cstdint>

#include <embree2/rtcore_ray.h>
#include <embree2/rtcore_scene.h>

#include "glm/glm.hpp"
#include "pbrt/memory.h"

#include "io/ply_loader.h"
#include "render/domain.h"
#include "render/rays.h"
#include "render/shape.h"
#include "render/spray.h"

#define NUM_VERTICES_PER_FACE 3  // triangle

namespace spray {

struct RTCRayIntersection;
struct Domain;
class Material;

class HybridGeometryBuffer {
 public:
  HybridGeometryBuffer();
  ~HybridGeometryBuffer();

 public:
  void init(bool use_spray_color, std::size_t cache_size,
            std::size_t max_nvertices, std::size_t max_nfaces);

  RTCScene load(int cache_block, Domain& domain);

 private:
  void loadTriangles(int cache_block, const Domain& domain);

  void loadShapes(const std::vector<Shape*>& shapes, int cache_block,
                  unsigned int shape_geom_id);

 public:
  RTCScene get(int cache_block) { return scenes_[cache_block]; }

  void updateIntersection(int cache_block, RTCRayIntersection* isect) const {
    // #ifdef SPRAY_GLOG_CHECK
    //     CHECK(isect->geomID == 0 || isect->geomID == 1) << isect->geomID;
    // #endif
    if (isect->geomID == shape_geom_ids_[cache_block]) {
      updateShapeIntersection(cache_block, isect);
    } else if (!colors_) {
      updateTriangleIntersectionNoColor(cache_block, isect);
    } else {
      updateTriangleIntersection(cache_block, isect);
    }
  }

 private:
  void getColorTuple(const Domain& domain, int cache_block, uint32_t geomID,
                     uint32_t primID, uint32_t colors[3]) const;

  void getNormalTuple(const Domain& domain, int cache_block, uint32_t geomID,
                      uint32_t primID, float normals_out[9]) const;

  void computeNormals(int cache_block, const float* vertices,
                      std::size_t num_vertices, const uint32_t* faces,
                      std::size_t num_faces, float* normals);

  std::size_t vertexBaseIndex(int cache_block) const {
    return (cache_block * max_nvertices_ * 3);
  }

  std::size_t normalBaseIndex(int cache_block) const {
    return (cache_block * max_nvertices_ * 3);
  }

  std::size_t normalBaseIndex(int cache_block, std::size_t offset) const {
    return 3 * (cache_block * max_nvertices_ + offset);
  }

  std::size_t normalBaseIndex(const Domain& domain, int cache_block,
                              uint32_t geom_id) const {
    return 3 * (cache_block * max_nvertices_ +
                domain.getNumVerticesPrefixSum(geom_id));
  }

  std::size_t faceBaseIndex(int cache_block) const {
    return (cache_block * max_nfaces_ * NUM_VERTICES_PER_FACE);
  }

  std::size_t faceBaseIndex(const Domain& domain, int cache_block,
                            uint32_t geom_id) const {
    return NUM_VERTICES_PER_FACE *
           (cache_block * max_nfaces_ + domain.getNumFacesPrefixSum(geom_id));
  }

  std::size_t colorBaseIndex(int cache_block) const {
    return (cache_block * max_nvertices_);
  }

  std::size_t colorBaseIndex(const Domain& domain, int cache_block,
                             uint32_t geom_id) const {
    return cache_block * max_nvertices_ +
           domain.getNumVerticesPrefixSum(geom_id);
  }

  // std::size_t materialBaseIndex(int cache_block) const {
  //   return (cache_block * max_nvertices_);
  // }

  const Material* getTriMeshMaterial(int cache_block,
                                     unsigned int geom_id) const {
    return domains_[cache_block]->getMaterial(geom_id);
  }

  void cleanup();
  void mapEmbreeBuffer(int cache_block, float* vertices,
                       std::size_t num_vertices, uint32_t* faces,
                       std::size_t num_faces, std::size_t num_models,
                       std::size_t model_id);

  const Material* getShapeMaterial(int cache_block, int prim_id) const {
    const Shape* shape = shapes_[cache_block]->at(prim_id);
    return shape->material;
  }

  void updateTriangleIntersection(int cache_block,
                                  RTCRayIntersection* isect) const;
  void updateTriangleIntersectionNoColor(int cache_block,
                                         RTCRayIntersection* isect) const;

  void updateShapeIntersection(int cache_block,
                               RTCRayIntersection* isect) const {
    isect->color = SPRAY_INVALID_COLOR;
    isect->Ns[0] = isect->Ng[0];
    isect->Ns[1] = isect->Ng[1];
    isect->Ns[2] = isect->Ng[2];
    isect->material = getShapeMaterial(cache_block, isect->primID);
  }

 private:
  enum MeshStatus { CREATED = -1, DESTROYED = 0 };

 private:
  std::size_t cache_size_;
  std::size_t max_nvertices_;
  std::size_t max_nfaces_;

  std::size_t vertex_buffer_size_;
  std::size_t face_buffer_size_;
  std::size_t color_buffer_size_;

  float* vertices_;   ///< per-cache-block vertices. 2d array.
  float* normals_;    ///< per-cache-block normals. unnormalized. 2d array.
  uint32_t* faces_;   ///< per-cache-block faces. 2d array.
  uint32_t* colors_;  ///< per-cache-block packed rgb colors. 2d array.
  std::vector<const Domain*> domains_;

  RTCDevice device_;
  RTCScene* scenes_;  ///< 1D array of per-cache-block Embree scenes.

  int* embree_mesh_created_;  // -1: initialized, 0: not initialized
  int* shape_created_;  // -1: initialized, 0: not initialized

  unsigned int* shape_geom_ids_;

  Material** materals_;  ///< Materials indexed by geomID and prim ID. 2D array.

  MemoryArena arena_;
  PlyLoader loader_;

  std::vector<const std::vector<Shape*>*> shapes_;
};

}  // namespace spray
