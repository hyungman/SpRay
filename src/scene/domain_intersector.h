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

#include "cmake_config.h"  // auto generated by cmake
#include "render/rays.h"

namespace spray {

template <typename CacheT, typename RayT, typename SceneT>
class DomainIntersector {
 public:
  /**
   * An intersector used for processing eye rays.
   *
   * \param ndomains Total number of domains in the scene.
   * \param scene Target scene to test against.
   * \param ray_buf An allocated ray buffer.
   * \param qs A set of ray queues used to save ray pointers as a result of
   * intersection tests.
   */
  void intersect(int ndomains, SceneT* scene, RayBuf<RayT> ray_buf,
                 spray::QVector<RayT*>* qs) {
    RayT* rays = ray_buf.rays;

    for (std::size_t i = 0; i < ray_buf.num; ++i) {
      RayT* ray = &rays[i];

      RTCRayUtil::makeRayForDomainIntersection(ray->org, ray->dir, &domains_,
                                               &eray_);

      scene->intersectDomains(eray_);

      if (domains_.count) {
        RTCRayUtil::sortDomains(domains_, hits_);

        for (int d = 0; d < domains_.count; ++d) {
          int id = hits_[d].id;
#ifdef SPRAY_GLOG_CHECK
          CHECK_LT(id, ndomains);
#endif
          qs->push(id, ray);
        }
      }
    }
  }

  /**
   * A ray-domain intersector for secondary rays.
   *
   * This intersector is useful when one wants to rule out the current domain
   * the the ray is in when the tests are performed.
   *
   * \param exclude_id A domain ID where rays are not enqueued.
   * \param ndomains Total number of domains in the scene.
   * \param scene Target scene to test against.
   * \param ray A pointer to the ray to be tested.
   * \param qs A set of ray queues used to save ray pointers as a result of
   * intersection tests.
   */
  void intersect(int exclude_id, int ndomains, SceneT* scene, RayT* ray,
                 spray::QVector<RayT*>* qs) {
    RTCRayUtil::makeRayForDomainIntersection(ray->org, ray->dir, &domains_,
                                             &eray_);

    scene->intersectDomains(eray_);

    if (domains_.count) {
      RTCRayUtil::sortDomains(domains_, hits_);

      for (int d = 0; d < domains_.count; ++d) {
        int id = hits_[d].id;
#ifdef SPRAY_GLOG_CHECK
        CHECK_LT(id, ndomains);
#endif
        if (id != exclude_id) {
          qs->push(id, ray);
        }
      }
    }
  }

  /**
   * A ray-domain intersector for secondary rays.
   *
   * This intersector is useful when a ray has hit some object and one wants to
   * consider that hit point as part of the ray-domain intersection tests.
   * Additionally, one can rule out the current domain the the ray is in when
   * the tests are performed.
   *
   * \param exclude_id A domain ID where rays are not enqueued.
   * \param t A t-value for the closest intersection point found so far.
   * \param ndomains Total number of domains in the scene.
   * \param scene Target scene to test against.
   * \param ray A pointer to the ray to be tested.
   * \param qs A set of ray queues used to save ray pointers as a result of
   * intersection tests.
   */
  void intersect(int exclude_id, float t, int ndomains, SceneT* scene,
                 RayT* ray, spray::QVector<RayT*>* qs) {
    RTCRayUtil::makeRayForDomainIntersection(ray->org, ray->dir, &domains_,
                                             &eray_);

    scene->intersectDomains(eray_);

    if (domains_.count) {
      RTCRayUtil::sortDomains(domains_, hits_);

      for (int d = 0; d < domains_.count; ++d) {
        int id = hits_[d].id;
#ifdef SPRAY_GLOG_CHECK
        CHECK_LT(id, ndomains);
#endif
        if (id != exclude_id && hits_[d].t < t) {
          qs->push(id, ray);
        }
      }
    }
  }

 private:
  DomainList domains_;  ///< A fixed-size list of hit domains.
  DomainHit1 hits_[SPRAY_RAY_DOMAIN_LIST_SIZE];
  RTCRayExt eray_;
};

}  // namespace spray
