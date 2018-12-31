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

#include <queue>

#include "glm/glm.hpp"
#include "glog/logging.h"

#include "cmake_config.h"  // auto generated by cmake

#include "insitu/insitu_ray.h"
#include "render/qvector.h"
#include "render/rays.h"

namespace spray {
namespace insitu {

template <typename CacheT, typename SceneT>
class Isector {
 private:
  DomainList domains_;
  DomainHit1 hits_[SPRAY_RAY_DOMAIN_LIST_SIZE];

  RTCRayExt eray_;

 public:
  // used for parallel ray queuing
  void intersect(int ndomains, SceneT* scene, Ray* ray,
                 spray::QVector<Ray*>* qs) {
    RTCRayUtil::makeRayForDomainIntersection(ray->org, ray->dir, &domains_,
                                             &eray_);

    // ray-domain intersection tests
    scene->intersectDomains(eray_);

    // place ray in hit domains
    if (domains_.count) {
      // sort hit domains
      RTCRayUtil::sortDomains(domains_, hits_);

      // place the ray (
      for (int d = 0; d < domains_.count; ++d) {
        int id = hits_[d].id;
#ifdef SPRAY_GLOG_CHECK
        CHECK_LT(id, ndomains);
#endif
        qs->push(id, ray);
      }
    }
  }

  // for processing eye rays
  void intersect(int ndomains, SceneT* scene, RayBuf<Ray> ray_buf,
                 spray::QVector<Ray*>* qs) {
    Ray* rays = ray_buf.rays;

    for (std::size_t i = 0; i < ray_buf.num; ++i) {
      Ray* ray = &rays[i];

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

  // for processing eye rays (with background support)
  void intersect(int ndomains, SceneT* scene, RayBuf<Ray> ray_buf,
                 spray::QVector<Ray*>* qs, std::queue<Ray*>* background_q) {
    Ray* rays = ray_buf.rays;

    for (std::size_t i = 0; i < ray_buf.num; ++i) {
      Ray* ray = &rays[i];

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
      } else {
        // RayUtil::setOccluded(RayUtil::OFLAG_BACKGROUND, ray);
        background_q->push(ray);
      }
    }
  }

  void intersect(int exclude_id, int ndomains, SceneT* scene, Ray* ray,
                 spray::QVector<Ray*>* qs) {
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

  void intersect(int exclude_id, int ndomains, SceneT* scene, Ray* ray,
                 spray::QVector<Ray*>* qs, std::queue<Ray*>* background_q) {
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
    } else {
      background_q->push(ray);
    }
  }

  void intersect(int exclude_id, float t, int ndomains, SceneT* scene, Ray* ray,
                 spray::QVector<Ray*>* qs) {
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

  void intersect(int exclude_id, float t, int ndomains, SceneT* scene, Ray* ray,
                 spray::QVector<Ray*>* qs, std::queue<Ray*>* background_q) {
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
    } else {
      background_q->push(ray);
    }
  }
};

}  // namespace insitu
}  // namespace spray
