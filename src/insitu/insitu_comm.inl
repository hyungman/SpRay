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

#if !defined(SPRAY_INSITU_COMM_INL)
#error An implementation of Comm
#endif

namespace spray {
namespace insitu {

template <typename ReceiverT>
void Comm<ReceiverT>::mpiIsendWords(SendQItem* item) {
  mpi_requests_.push_back(MPI_Request());
  MPI_Request& r = mpi_requests_.back();
  item->isend(&r);
}

template <typename ReceiverT>
void Comm<ReceiverT>::serveRecv(const MPI_Status& status, MemoryArena* mem,
                                ReceiverT* receiver) {
  int tag = status.MPI_TAG;
  int msg_count;
  MPI_Get_count(&status, MPI_WORD_T, &msg_count);

  msg_word_t* msg = mem->Alloc<msg_word_t>(msg_count);
  CHECK_NOTNULL(msg);

  MPI_Recv(msg, msg_count, MPI_WORD_T, status.MPI_SOURCE, status.MPI_TAG,
           MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  (*receiver)(tag, msg);
}

template <typename ReceiverT>
void Comm<ReceiverT>::run(const WorkStats& work_stats, MemoryArena* mem,
                          ReceiverT* receiver) {
  MPI_Status status;
  int flag;

  int num_blocks_recved = 0;
  bool recv_done = work_stats.recvDone(num_blocks_recved);

#ifdef SPRAY_TIMING
  spray::tStart(spray::TIMER_SYNC_RAYS);
#endif
  while (1) {
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

#ifdef SPRAY_GLOG_CHECK
    CHECK(!(recv_done && flag));
#endif
    if (!recv_done && flag) {
      serveRecv(status, mem, receiver);
      ++num_blocks_recved;
      recv_done = work_stats.recvDone(num_blocks_recved);
    }

    if (!send_q_.empty()) {
      SendQItem* item = send_q_.front();
      send_q_.pop();

      int t = item->getType();
#ifdef SPRAY_GLOG_CHECK
      CHECK((t == Work::SEND_SHADOW_RAYS) || (t == Work::SEND_RADIANCE_RAYS))
          << t;
#endif
      mpiIsendWords(item);
    } else if (recv_done) {
      break;
    }
  }
#ifdef SPRAY_TIMING
  spray::tStop(spray::TIMER_SYNC_RAYS);
#endif
}

template <typename ReceiverT>
void Comm<ReceiverT>::waitForSend() {
  for (std::list<MPI_Request>::iterator it = mpi_requests_.begin();
       it != mpi_requests_.end(); ++it) {
    MPI_Wait(&(*it), MPI_STATUS_IGNORE);
  }
  mpi_requests_.clear();
}

}  // namespace insitu
}  // namespace spray

