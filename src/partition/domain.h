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

#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "partition/aabb.h"

namespace spray {

class Bsdf;
class Light;

struct Domain {
  Domain() : num_vertices(0), num_faces(0), bsdf(nullptr) {}
  unsigned id;  // single domain id
  std::size_t num_vertices;
  std::size_t num_faces;
  std::string filename;
  Aabb object_aabb;
  Aabb world_aabb;
  glm::mat4 transform;
  Bsdf* bsdf;
};

void loadDescriptor(const std::string& filename, const std::string& ply_path,
                    std::vector<Domain>* domains_out,
                    std::vector<Light*>* lights_out);

}  // namespace spray

