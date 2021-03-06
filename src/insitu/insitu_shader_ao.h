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
#include "pbrt/memory.h"

#include "insitu/insitu_ray.h"
#include "render/config.h"
#include "render/light.h"
#include "render/rays.h"
#include "render/reflection.h"
#include "render/scene.h"
#include "utils/util.h"

namespace spray {
namespace insitu {

template <typename SceneT>
class ShaderAo {
 public:
  typedef SceneT SceneType;

  void init(const spray::Config &cfg, const SceneT *scene) {
    bounces_ = cfg.bounces;
    samples_ = cfg.ao_samples;  // number of samples for area lights
    ks_ = cfg.ks;
    shininess_ = cfg.shininess;
    scene_ = scene;
    lights_ = scene->getLights();  // copy lights
  }

 private:
  const SceneT *scene_;
  std::vector<Light *> lights_;
  int bounces_;
  int samples_;
  glm::vec3 ks_;
  float shininess_;

 public:
  bool isAo() { return true; }

 public:
  void operator()(int domain_id, const Ray &rayin,
                  const spray::RTCRayIntersection &isect,
                  spray::MemoryArena *mem, std::queue<Ray *> *sq,
                  std::queue<Ray *> *rq, int ray_depth);

 private:
  void genR2(const Ray &rayin, const glm::vec3 &org, const glm::vec3 &dir,
             const glm::vec3 &w, float t, spray::MemoryArena *mem,
             std::queue<Ray *> *rq) {
    Ray *r2 = mem->Alloc<Ray>(1, false);
    RayUtil::makeRay(rayin, org, dir, w, t, r2);
    rq->push(r2);
  }
};

template <typename SceneT>
void ShaderAo<SceneT>::operator()(int domain_id, const Ray &rayin,
                                  const spray::RTCRayIntersection &isect,
                                  spray::MemoryArena *mem,
                                  std::queue<Ray *> *sq, std::queue<Ray *> *rq,
                                  int ray_depth) {
  glm::vec3 pos = RTCRayUtil::hitPosition(rayin.org, rayin.dir, isect.tfar);
  glm::vec3 surf_radiance;
  util::unpack(isect.color, surf_radiance);

  glm::vec3 normal(isect.Ns[0], isect.Ns[1], isect.Ns[2]);
  glm::vec3 wo(-rayin.dir[0], -rayin.dir[1], -rayin.dir[2]);
  glm::vec3 Lin(rayin.w[0], rayin.w[1], rayin.w[2]);

  float cos_theta_i = glm::dot(wo, normal);
  bool entering = (cos_theta_i > 0.0f);
  glm::vec3 normal_ff = entering ? normal : -normal;
  normal_ff = glm::normalize(normal_ff);

  glm::vec3 wi, light_radiance, Lr;
  float pdf, costheta;
  int nlights = lights_.size();

  std::size_t color_idx = sq->size();

  const Bsdf *bsdf = scene_->getBsdf(domain_id);
  bool delta_dist = bsdf->isDelta();

  int next_ray_depth = ray_depth + 1;

  const float ao_weight = 1.0f / static_cast<float>(samples_);
  RandomSampler light_sampler;

  for (int l = 0; l < samples_; ++l) {
    RandomSampler_init(light_sampler, rayin.pixid * (l + 1));
    bsdf->sampleRandom(normal_ff, &light_sampler, &wi, &pdf);

    costheta = glm::clamp(glm::dot(normal_ff, wi), 0.0f, 1.0f);
    Lr = Lin * surf_radiance * (SPRAY_ONE_OVER_PI * costheta * ao_weight / pdf);

    if (hasPositive(Lr)) {
      Ray *shadow = mem->Alloc<Ray>(1, false);
      CHECK_NOTNULL(shadow);

      RayUtil::makeShadow(rayin, l, pos, wi, Lr, isect.tfar, shadow);
      sq->push(shadow);
    }
  }

#ifdef SPRAY_GLOG_CHECK
  CHECK_LT(ray_depth, bounces_);
#endif

  if (next_ray_depth < bounces_) {
    wo = glm::normalize(wo);

    if (delta_dist) {
      if (cos_theta_i != 0.0f) {  // rule out 90 degree
        cos_theta_i = glm::clamp(cos_theta_i, -1.0f, 1.0f);
        float abs_cos_theta_i = glm::abs(cos_theta_i);

        if (!entering) {  // i.e cos_theta_i < 0.0f
          cos_theta_i = abs_cos_theta_i;
        }

        uint32_t sample_type;
        float fr;      // prob. of reflection
        glm::vec3 wt;  // direction of transmitted ray
        bsdf->sampleDelta(entering, cos_theta_i, wo, normal_ff, &sample_type,
                          &fr, &wt);
        bool has_reflect = hasReflection(sample_type);

        if (has_reflect) {
          glm::vec3 wr = Reflect(wo, normal_ff);
          wi = glm::normalize(wr);
          Lr = Lin * (fr / abs_cos_theta_i);
          if (hasPositive(Lr)) {
            genR2(rayin, pos, wi, Lr, isect.tfar, mem, rq);
          }
        }

        if (hasTransmission(sample_type)) {
          // TODO: support both reflection and refraction
          CHECK_EQ(has_reflect, false);
          wi = glm::normalize(wt);
          Lr = Lin * ((1.0f - fr) / abs_cos_theta_i);
          if (hasPositive(Lr)) {
            genR2(rayin, pos, wi, Lr, isect.tfar, mem, rq);
          }
        }
      }
    } else {
      RandomSampler sampler;
      RandomSampler_init(sampler, rayin.samid * next_ray_depth);

      bsdf->sampleRandom(normal_ff, &sampler, &wi, &pdf);
      costheta = glm::clamp(glm::dot(normal_ff, wi), 0.0f, 1.0f);
      Lr = Lin * surf_radiance * SPRAY_ONE_OVER_PI * costheta / pdf;
      if (hasPositive(Lr)) {
        genR2(rayin, pos, wi, Lr, isect.tfar, mem, rq);
      }
    }
  }
}

}  // namespace insitu
}  // namespace spray
