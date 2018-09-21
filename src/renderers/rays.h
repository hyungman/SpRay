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

#include <iostream>
#include <queue>
#include <algorithm>

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

#include "glm/glm.hpp"
#include "glog/logging.h"

#include "cmake_config.h"  // auto generated by cmake
#include "renderers/spray.h"

namespace spray {

template <unsigned PACKET_SIZE>
struct DomainHitN {
  int num;                                            //!< Number of hits.
  int ids[PACKET_SIZE * SPRAY_RAY_DOMAIN_LIST_SIZE];  //!< Hit domain IDs.
  float ts[PACKET_SIZE *
           SPRAY_RAY_DOMAIN_LIST_SIZE];  //!< Distance to hit domains.
};

struct SPRAY_ALIGN(16) DomainList {
  int count;                             //!< Number of hits.
  int ids[SPRAY_RAY_DOMAIN_LIST_SIZE];   //!< Hit domain IDs.
  float ts[SPRAY_RAY_DOMAIN_LIST_SIZE];  //!< Distance to hit domains.
};

#define RAY8_DOMAIN_LIST_SIZE (SPRAY_RAY_DOMAIN_LIST_SIZE << 3)

struct SPRAY_ALIGN(32) DomainList8 {
  int count[8];                     //!< Number of hits.
  int ids[RAY8_DOMAIN_LIST_SIZE];   //!< Hit domain IDs.
  float ts[RAY8_DOMAIN_LIST_SIZE];  //!< Distance to hit domains.
};

template <unsigned M>
struct SPRAY_ALIGN(16) DomainList1M {
  int count[M];                              //!< Number of hits.
  int ids[M * SPRAY_RAY_DOMAIN_LIST_SIZE];   //!< Hit domain IDs.
  float ts[M * SPRAY_RAY_DOMAIN_LIST_SIZE];  //!< Distance to hit domains.
};

template <unsigned N, unsigned M>
struct SPRAY_ALIGN(16) DomainListNM {
  int count[N * M];                              //!< Number of hits.
  int ids[N * M * SPRAY_RAY_DOMAIN_LIST_SIZE];   //!< Hit domain IDs.
  float ts[N * M * SPRAY_RAY_DOMAIN_LIST_SIZE];  //!< Distance to hit domains.
};

struct DomainHit1 {
  int id;
  float t;
};

struct SPRAY_ALIGN(16) RTCRayExt {
  /* ray data */
 public:
  float org[3];  //!< Ray origin
  float align0;

  float dir[3];  //!< Ray direction
  float align1;

  float tnear;  //!< Start of ray segment
  float tfar;   //!< End of ray segment (set to hit distance)

  float time;     //!< Time of this ray for motion blur
  unsigned mask;  //!< Used to mask out objects during traversal

  /* hit data */
 public:
  float Ng[3];  //!< Unnormalized geometry normal
  float align2;

  float u;  //!< Barycentric u coordinate of hit
  float v;  //!< Barycentric v coordinate of hit

  unsigned geomID;  //!< geometry ID
  unsigned primID;  //!< primitive ID
  unsigned instID;  //!< instance ID

  /* extension*/
 public:
  DomainList* domains;
};

struct SPRAY_ALIGN(32) RTCRayExt8 {
  /* ray data */
 public:
  float orgx[8];  //!< x coordinate of ray origin
  float orgy[8];  //!< y coordinate of ray origin
  float orgz[8];  //!< z coordinate of ray origin

  float dirx[8];  //!< x coordinate of ray direction
  float diry[8];  //!< y coordinate of ray direction
  float dirz[8];  //!< z coordinate of ray direction

  float tnear[8];  //!< Start of ray segment
  float tfar[8];   //!< End of ray segment (set to hit distance)

  float time[8];     //!< Time of this ray for motion blur
  unsigned mask[8];  //!< Used to mask out objects during traversal

  /* hit data */
 public:
  float Ngx[8];  //!< x coordinate of geometry normal
  float Ngy[8];  //!< y coordinate of geometry normal
  float Ngz[8];  //!< z coordinate of geometry normal

  float u[8];  //!< Barycentric u coordinate of hit
  float v[8];  //!< Barycentric v coordinate of hit

  unsigned geomID[8];  //!< geometry ID
  unsigned primID[8];  //!< primitive ID
  unsigned instID[8];  //!< instance ID

  /* extension*/
 public:
  DomainList8* domains;
};

#define PACKET16_STACK_SIZE (SPRAY_RAY_DOMAIN_LIST_SIZE << 4)

struct SPRAY_ALIGN(64) RTCRayExt16 {
  /* ray data */
 public:
  float orgx[16];  //!< x coordinate of ray origin
  float orgy[16];  //!< y coordinate of ray origin
  float orgz[16];  //!< z coordinate of ray origin

  float dirx[16];  //!< x coordinate of ray direction
  float diry[16];  //!< y coordinate of ray direction
  float dirz[16];  //!< z coordinate of ray direction

  float tnear[16];  //!< Start of ray segment
  float tfar[16];   //!< End of ray segment (set to hit distance)

  float time[16];     //!< Time of this ray for motion blur
  unsigned mask[16];  //!< Used to mask out objects during traversal

  /* hit data */
 public:
  float Ngx[16];  //!< x coordinate of geometry normal
  float Ngy[16];  //!< y coordinate of geometry normal
  float Ngz[16];  //!< z coordinate of geometry normal

  float u[16];  //!< Barycentric u coordinate of hit
  float v[16];  //!< Barycentric v coordinate of hit

  unsigned geomID[16];  //!< geometry ID
  unsigned primID[16];  //!< primitive ID
  unsigned instID[16];  //!< instance ID

  /* extension*/
 public:
  unsigned dom_count[16];
  unsigned dom_ids[PACKET16_STACK_SIZE];
  float dom_ts[PACKET16_STACK_SIZE];
};

struct SPRAY_ALIGN(16) RTCRayIntersection {
  /* ray data */
 public:
  float org[3];  //!< Ray origin
  float align0;

  float dir[3];  //!< Ray direction
  float align1;

  float tnear;  //!< Start of ray segment
  float tfar;   //!< End of ray segment (set to hit distance)

  float time;     //!< Time of this ray for motion blur
  unsigned mask;  //!< Used to mask out objects during traversal

  /* hit data */
 public:
  float Ng[3];     //!< Unnormalized geometry normal
  uint32_t color;  // interpolated color

  float u;  //!< Barycentric u coordinate of hit
  float v;  //!< Barycentric v coordinate of hit

  unsigned geomID;  //!< geometry ID
  unsigned primID;  //!< primitive ID
  unsigned instID;  //!< instance ID

  float Ns[3];
};

struct SPRAY_ALIGN(16) DRay {
  // Ray geometry
 public:
  float org[3];
  int pixid;  //!< Sample ID of the image plane.

  float dir[3];
  int samid;

  // Intersection results
 public:
  float w[3];  //!< Radiance weight.
  int depth;   //!< Bounce number starting from 0.
  float t;
  float u;          //!< Barycentric u coordinate of hit
  float v;          //!< Barycentric v coordinate of hit
  unsigned geomID;  //!< geometry ID
  unsigned primID;
  unsigned flag;
  int domid;  // closest domain ID

  int domain_pos;   // current domain position
  float next_tdom;  // distance to next domain

#ifdef SPRAY_GLOG_CHECK
  friend std::ostream& operator<<(std::ostream& os, const DRay& r);
#endif
};

struct SPRAY_ALIGN(16) DRayQItem {
  int dummy;
  DRay* ray;
};

typedef std::queue<DRay*> DRayQ;

struct RTCRayUtil {
  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void makeEyeRay(const DRay& r, DomainList* domains,
                                RTCRayExt* rout) {
    rout->org[0] = r.org[0];
    rout->org[1] = r.org[1];
    rout->org[2] = r.org[2];

    rout->dir[0] = r.dir[0];
    rout->dir[1] = r.dir[1];
    rout->dir[2] = r.dir[2];

    // rout->tnear = 0.0f;
    rout->tnear = SPRAY_RAY_EPSILON;
    rout->tfar = SPRAY_FLOAT_INF;
    // rout->instID = RTC_INVALID_GEOMETRY_ID;
    rout->geomID = RTC_INVALID_GEOMETRY_ID;
    rout->primID = RTC_INVALID_GEOMETRY_ID;
    // rout->mask = 0xFFFFFFFF;
    // rout->time = 0.0f;

    rout->domains = domains;

    domains->count = 0;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void makeRayForDomainIntersection(const float org[3],
                                                  const float dir[3],
                                                  DomainList* domains,
                                                  RTCRayExt* rout) {
    rout->org[0] = org[0];
    rout->org[1] = org[1];
    rout->org[2] = org[2];

    rout->dir[0] = dir[0];
    rout->dir[1] = dir[1];
    rout->dir[2] = dir[2];

    // rout->tnear = 0.0f;
    rout->tnear = SPRAY_RAY_EPSILON;
    rout->tfar = SPRAY_FLOAT_INF;
    // rout->instID = RTC_INVALID_GEOMETRY_ID;
    rout->geomID = RTC_INVALID_GEOMETRY_ID;
    rout->primID = RTC_INVALID_GEOMETRY_ID;
    // rout->mask = 0xFFFFFFFF;
    // rout->time = 0.0f;

    rout->domains = domains;

    domains->count = 0;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  // sort domains front-to-back
  inline static void sortDomains(const DomainList& domains,
                                 DomainHit1 hits[SPRAY_RAY_DOMAIN_LIST_SIZE]) {
    int count = domains.count;

    for (int i = 0; i < count; ++i) {
      hits[i].id = domains.ids[i];
      hits[i].t = domains.ts[i];
    }

    std::sort(hits, hits + count,
              [&](const DomainHit1& a, const DomainHit1& b) {
                // return (a.t < b.t);
                return ((a.t < b.t) || ((a.t == b.t) && (a.id < b.id)));
              });
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void sortDomains8(int p, const DomainList8& domains,
                                  DomainHit1 hits[SPRAY_RAY_DOMAIN_LIST_SIZE]) {
    int count = domains.count[p];

    for (int i = 0; i < count; ++i) {
      int offset = i * 8 + p;
      hits[i].id = domains.ids[offset];
      hits[i].t = domains.ts[offset];
    }

    std::sort(hits, hits + count,
              [&](const DomainHit1& a, const DomainHit1& b) {
                // return (a.t < b.t);
                return ((a.t < b.t) || ((a.t == b.t) && (a.id < b.id)));
              });
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void makeRadianceRay(const float org[3], const float dir[3],
                                     RTCRayIntersection* r) {
    r->org[0] = org[0];
    r->org[1] = org[1];
    r->org[2] = org[2];
    // r->samID = samid;
    r->dir[0] = dir[0];
    r->dir[1] = dir[1];
    r->dir[2] = dir[2];
    // r->depth = depth;
    // r->tnear = 0.f;
    r->tnear = SPRAY_RAY_EPSILON;
    r->tfar = SPRAY_FLOAT_INF;
    r->instID = RTC_INVALID_GEOMETRY_ID;
    r->geomID = RTC_INVALID_GEOMETRY_ID;
    r->primID = RTC_INVALID_GEOMETRY_ID;
    r->mask = 0xFFFFFFFF;
    r->time = 0.0f;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void makeRadianceRay(const glm::vec3& org, const float dir[3],
                                     RTCRayIntersection* r) {
    r->org[0] = org[0];
    r->org[1] = org[1];
    r->org[2] = org[2];
    // r->samID = samid;
    r->dir[0] = dir[0];
    r->dir[1] = dir[1];
    r->dir[2] = dir[2];
    // r->depth = depth;
    // r->tnear = 0.f;
    r->tnear = SPRAY_RAY_EPSILON;
    r->tfar = SPRAY_FLOAT_INF;
    r->instID = RTC_INVALID_GEOMETRY_ID;
    r->geomID = RTC_INVALID_GEOMETRY_ID;
    r->primID = RTC_INVALID_GEOMETRY_ID;
    r->mask = 0xFFFFFFFF;
    r->time = 0.0f;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void makeIntersection(const DRay& r,
                                      RTCRayIntersection* isect) {
    isect->org[0] = r.org[0];
    isect->org[1] = r.org[1];
    isect->org[2] = r.org[2];
    // isect->samID = r.samid;
    isect->dir[0] = r.dir[0];
    isect->dir[1] = r.dir[1];
    isect->dir[2] = r.dir[2];
    // isect->depth = r.depth;
    // // isect->tnear = 0.f;
    // isect->tnear = SPRAY_RAY_EPSILON;
    isect->tfar = r.t;
    // isect->instID = RTC_INVALID_GEOMETRY_ID;
    isect->u = r.u;
    isect->v = r.v;
    isect->geomID = r.geomID;
    isect->primID = r.primID;
    // isect->mask = 0xFFFFFFFF;
    // isect->time = 0.0f;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void makeShadowRay(const glm::vec3& org, const glm::vec3& dir,
                                   RTCRay* r) {
    r->org[0] = org[0];
    r->org[1] = org[1];
    r->org[2] = org[2];
    r->dir[0] = dir[0];
    r->dir[1] = dir[1];
    r->dir[2] = dir[2];
    // r->tnear = 0.001f;
    r->tnear = SPRAY_RAY_EPSILON;
    r->tfar = SPRAY_FLOAT_INF;
    r->geomID = RTC_INVALID_GEOMETRY_ID;
    r->primID = RTC_INVALID_GEOMETRY_ID;
    r->mask = -1;
    r->time = 0;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void makeShadowRay(const float org[3], const float dir[3],
                                   RTCRay* r) {
    r->org[0] = org[0];
    r->org[1] = org[1];
    r->org[2] = org[2];
    r->dir[0] = dir[0];
    r->dir[1] = dir[1];
    r->dir[2] = dir[2];
    // r->tnear = 0.001f;
    r->tnear = SPRAY_RAY_EPSILON;
    r->tfar = SPRAY_FLOAT_INF;
    r->geomID = RTC_INVALID_GEOMETRY_ID;
    r->primID = RTC_INVALID_GEOMETRY_ID;
    r->mask = -1;
    r->time = 0;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static void hitPosition(const float org[3], const float dir[3],
                                 const float t, float pos[3]) {
    pos[0] = dir[0] * t + org[0];
    pos[1] = dir[1] * t + org[1];
    pos[2] = dir[2] * t + org[2];
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static glm::vec3 hitPosition(const float org[3], const float dir[3],
                                      float t) {
    glm::vec3 pos(dir[0] * t + org[0], dir[1] * t + org[1],
                  dir[2] * t + org[2]);
    return pos;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static glm::vec3 hitPosition(const glm::vec3& org, const float dir[3],
                                      float t) {
    glm::vec3 pos(dir[0] * t + org[0], dir[1] * t + org[1],
                  dir[2] * t + org[2]);
    return pos;
  }

  /////////////////////////////// RTCRayUtil //////////////////////////////////

  inline static glm::vec3 hitPosition(const RTCRayIntersection& isect) {
    float t = isect.tfar;
    glm::vec3 pos(isect.dir[0] * t + isect.org[0],
                  isect.dir[1] * t + isect.org[1],
                  isect.dir[2] * t + isect.org[2]);
    return pos;
  }
};  // end of struct RTCRayUtil

}  // namespace spray
