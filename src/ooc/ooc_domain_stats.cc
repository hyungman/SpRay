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

#include "ooc/ooc_domain_stats.h"

#include <algorithm>
#include <cstdint>

#include "glog/logging.h"

#include "cmake_config.h"  // auto generated by cmake
#include "utils/comm.h"

namespace spray {
namespace ooc {

void DomainStats::resize(int num_domains, bool stats_only) {
  //
  int num_ranks = mpi::size();
  int rank = mpi::rank();
  num_domains_ = num_domains;

  stats_.resize(SPRAY_RAY_DOMAIN_LIST_SIZE * num_domains);

  if (!stats_only) {
    scores_.resize(num_domains);
    schedule_.resize(num_domains);
  }
}

void DomainStats::addStats(int id, const DomainStats& stats) {
#ifdef SPRAY_GLOG_CHECK
  CHECK_GT(stats_.size(), 0);
  CHECK_EQ(stats_.size(), stats.stats_.size());
#endif
  int offset = id * SPRAY_RAY_DOMAIN_LIST_SIZE;
  for (int d = 0; d < SPRAY_RAY_DOMAIN_LIST_SIZE; ++d) {
    int i = offset + d;
    stats_[i] += stats.stats_[i];
  }
}

void DomainStats::schedule() {
#ifdef SPRAY_GLOG_CHECK
  CHECK_GT(schedule_.size(), 0);
#endif
  // evaluate the score of each domain
  evalScores();
  // add up all ray counts at all depth levels
  // to get aggregated ray count of each domain
  sortScoresInDescendingOrder();
  // update traversal buffer by reading sorted scores in order
  updateTraversalOrder();
}

void DomainStats::evalScores() {
  for (int id = 0; id < num_domains_; ++id) {
    int64_t score = 0;
    for (int depth = 0; depth < SPRAY_RAY_DOMAIN_LIST_SIZE; ++depth) {
      int w = SPRAY_RAY_DOMAIN_LIST_SIZE - depth;
      int64_t c = getStats(id, depth);
      score += (c * w);
    }
    scores_[id].domain_id = id;
    scores_[id].score = score;
  }
}

int64_t DomainStats::getStats(int id, int depth) const {
  int i = statsIndex(id, depth);
#ifdef SPRAY_GLOG_CHECK
  CHECK_LT(i, stats_.size());
  CHECK_LT(depth, SPRAY_RAY_DOMAIN_LIST_SIZE);
#endif
  return stats_[i];
}

void DomainStats::sortScoresInDescendingOrder() {
  Score* s = &scores_[0];
  std::sort(s, s + scores_.size(), [](const Score& a, const Score& b) -> bool {
    return a.score > b.score;
  });
}

void DomainStats::updateTraversalOrder() {
  for (std::size_t i = 0; i < schedule_.size(); ++i) {
    schedule_[i] = scores_[i].domain_id;
#ifdef SPRAY_GLOG_CHECK
    int id = scores_[i].domain_id;
    CHECK_GE(id, 0);
    CHECK_LT(id, num_domains_);
#endif
  }
}

}  // namespace ooc
}  // namespace spray
