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

namespace spray {
namespace insitu {

struct Ray;

template <class T>
std::size_t getNumItems(void* base, std::size_t bytes) {
  uint8_t* end = ((uint8_t*)base) + bytes;
  return ((T*)end - (T*)base);
}

struct RayBuf {
  RayBuf() { reset(); }
  void reset() {
    num = 0;
    rays = nullptr;
  }
  std::size_t num;
  Ray* rays;
};

}  // namespace insitu
}  // namespace spray
