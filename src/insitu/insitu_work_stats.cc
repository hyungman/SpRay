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

#include "insitu/insitu_work_stats.h"

#include <cstdint>

#include "glog/logging.h"

#include "cmake_config.h"  // auto generated by cmake
#include "render/data_partition.h"
#include "utils/profiler_util.h"

namespace spray {
namespace insitu {

void WorkStats::reduce() {
  if (mpi::size() == 1) {
    scatter_buf_[0].world_num_blocks_to_proc = reduce_buf_[0];
    scatter_buf_[0].rank_num_blocks_to_proc = reduce_buf_[0];
    return;
  }

  int rank = mpi::rank();

  int num_blocks_already_owned = reduce_buf_[rank];

#ifdef SPRAY_TIMING
  spray::tStartMPI(spray::TIMER_SYNC_SCHED);
#endif
  if (rank == 0) {
    MPI_Reduce(MPI_IN_PLACE, &reduce_buf_[0], reduce_buf_.size(), MPI_INT,
               MPI_SUM, 0, MPI_COMM_WORLD);

    int sum = 0;
    for (int num_blocks_to_recv : reduce_buf_) {
      sum += num_blocks_to_recv;
    }

    int num_ranks = mpi::size();

    for (int i = 0; i < num_ranks; ++i) {
      scatter_buf_[i].world_num_blocks_to_proc = sum;
      scatter_buf_[i].rank_num_blocks_to_proc = reduce_buf_[i];
    }

    MPI_Scatter(&scatter_buf_[0], sizeof(ScatterEntry), MPI_UNSIGNED_CHAR,
                MPI_IN_PLACE /*recvbuf*/, 0 /*recvcount*/, MPI_UNSIGNED_CHAR, 0,
                MPI_COMM_WORLD);

  } else {
    MPI_Reduce(&reduce_buf_[0], nullptr, reduce_buf_.size(), MPI_INT, MPI_SUM,
               0, MPI_COMM_WORLD);

    MPI_Scatter(nullptr, 0, MPI_UNSIGNED_CHAR, &scatter_buf_[rank],
                sizeof(ScatterEntry), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
  }

  num_blocks_to_recv_ =
      (scatter_buf_[rank].rank_num_blocks_to_proc - num_blocks_already_owned);
#ifdef SPRAY_TIMING
  spray::tStop(spray::TIMER_SYNC_SCHED);
#endif

#ifdef SPRAY_GLOG_CHECK
  CHECK_GE(num_blocks_to_recv_, 0);
#endif
}

void WorkStats::reduceRayBlocks(std::queue<int>* blockIds) {
  while (!blockIds->empty()) {
    int id = blockIds->front();
    blockIds->pop();
    block_counters_[id] |= 1;  // 0 or 1
  }
}

void WorkStats::updateReduceBuffer(const spray::InsituPartition* partition) {
  for (int id = 0; id < ndomains_; ++id) {
    int n = block_counters_[id];
#ifdef SPRAY_GLOG_CHECK
    CHECK_LE(n, 1);
    CHECK_GE(n, 0);
#endif
    if (n) {
      int dest = partition->rank(id);
      addNumDomains(dest, n);
    }
  }
}

}  // namespace insitu
}  // namespace spray

