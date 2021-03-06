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

#if !defined(SPRAY_SPRAY_RENDERER_INL_)
#error An implementation of SprayRenderer
#endif

namespace spray {

template <class TracerT>
void SprayRenderer<TracerT>::init(const Config &cfg) {
  // config
  cfg_ = &cfg;

  // scene
  bool insitu_mode = (cfg.partition == spray::Config::INSITU);

  scene_.init(cfg.model_descriptor_filename, cfg.ply_path, cfg.local_disk_path,
              cfg.cache_size, cfg.view_mode, insitu_mode, cfg.num_partitions);

#ifdef SPRAY_GLOG_CHECK
  LOG(INFO) << "scene init done";
#endif

#ifdef SPRAY_TIMING
  global_profiler.init();
#endif

  // build wbvh
  scene_.buildWbvh();

  // frame buffer
  image_.resize(cfg.image_w, cfg.image_h);

  // camera
  initCamera(cfg);

  // tracer
  if (!(cfg.view_mode == VIEW_MODE_DOMAIN ||
        cfg.view_mode == VIEW_MODE_PARTITION)) {
    tracer_.init(cfg, camera_, &scene_, &image_);
  }

  // vis
  WbvhObj<WbvhEmbree> wobj;
  wobj.ptr = nullptr;

  Vis<WbvhEmbree>::init(wobj);

  // mpi command
  msgcmd_.done = 0;
  msgcmd_.image_w = cfg.image_w;
  msgcmd_.image_h = cfg.image_h;
  msgcmd_.view_mode = cfg.view_mode;
  msgcmd_.camera_cmd = CAM_NOP;

  // glfw
  if (cfg.view_mode != VIEW_MODE_FILM) {
    Glfw<WbvhEmbree, SceneType>::init(cfg, mpi::isRootProcess(), cfg.image_w,
                                      cfg.image_h, &camera_, &msgcmd_, &scene_);
  }

#ifdef SPRAY_GLOG_CHECK
  int ndomains = scene_.getNumDomains();
  LOG_IF(INFO, mpi::rank() == 0) << "number of domains: " << ndomains;
#endif
}

template <class TracerT>
void SprayRenderer<TracerT>::initCamera(const Config &cfg) {
  glm::vec3 campos, lookat, upvec;

  if (cfg.has_camera_config) {
    campos = cfg.camera_pos;
    lookat = cfg.camera_lookat;
    upvec = cfg.camera_up;

  } else {
    Aabb aabb = scene_.getBound();
    glm::vec3 extent = aabb.getExtent();
    lookat = aabb.getCenter();
    campos =
        lookat + (glm::length(extent) * 0.5f * glm::vec3(0.0f, 0.0f, 1.0f));
    upvec = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  camera_.init(campos, lookat, upvec, cfg.fov, cfg.image_w, cfg.image_h);
}

template <class TracerT>
void SprayRenderer<TracerT>::run() {
  CHECK(cfg_) << "failed to run. not configured.";

  if (cfg_->dev_mode == Config::DEVMODE_NORMAL) {
    run_normal();
  } else {
    run_dev();
  }
}

template <class TracerT>
void SprayRenderer<TracerT>::run_normal() {
  if (msgcmd_.view_mode == VIEW_MODE_FILM) {
    renderFilm();
  } else if (msgcmd_.view_mode == VIEW_MODE_GLFW) {
    if (mpi::isSingleProcess()) {
      renderGlfwSingleTask();
    } else if (mpi::isRootProcess()) {
      renderGlfwRootTask();
    } else {
      renderGlfwChildTask();
    }
  } else if (msgcmd_.view_mode == VIEW_MODE_DOMAIN ||
             msgcmd_.view_mode == VIEW_MODE_PARTITION) {
    renderGlfwDomainBounds(msgcmd_.view_mode);
  } else {
    msgcmd_.view_mode = VIEW_MODE_TERMINATE;
    glfwTerminate();
    LOG(FATAL) << "unsupported mode " << msgcmd_.view_mode;
  }
}

template <class TracerT>
void SprayRenderer<TracerT>::run_dev() {
  if (msgcmd_.view_mode == VIEW_MODE_FILM) {
    renderFilmInOmp();
  } else if (msgcmd_.view_mode == VIEW_MODE_GLFW) {
    renderGlfwInOmp();
  } else if (msgcmd_.view_mode == VIEW_MODE_DOMAIN ||
             msgcmd_.view_mode == VIEW_MODE_PARTITION) {
    LOG(FATAL) << "unsupported view mode";
    renderGlfwDomainBounds(msgcmd_.view_mode);
  } else {
    msgcmd_.view_mode = VIEW_MODE_TERMINATE;
    glfwTerminate();
    LOG(FATAL) << "unsupported mode " << msgcmd_.view_mode;
  }
}

template <class TracerT>
void SprayRenderer<TracerT>::renderGlfwSingleTask() {
#ifdef SPRAY_TIMING
  tReset();
  tStartMPI(TIMER_TOTAL);
#endif

  int64_t cfg_nframes = cfg_->nframes;
  int image_w = image_.w;
  int image_h = image_.h;
  glm::vec4 *image_buf = image_.buf;

  CHECK_EQ(msgcmd_.view_mode, VIEW_MODE_GLFW);

  int64_t nframes = 0;

  while (nframes < cfg_nframes || (cfg_nframes < 0 && !msgcmd_.done)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    image_.clear();

    tracer_.trace();

    glDrawPixels(image_w, image_h, GL_RGBA, GL_FLOAT, image_buf);
    Glfw<WbvhEmbree, SceneType>::swapBuffers();
    glfwPollEvents();
    Glfw<WbvhEmbree, SceneType>::cmdHandler();

    ++nframes;
  }

#ifdef SPRAY_TIMING
  tStop(TIMER_TOTAL);
#endif

  glfwTerminate();

#ifdef SPRAY_TIMING
  tPrint(nframes);
#endif
}

template <class TracerT>
void SprayRenderer<TracerT>::renderGlfwRootTask() {
#ifdef SPRAY_TIMING
  tReset();
  tStartMPI(TIMER_TOTAL);
#endif

  int64_t cfg_nframes = cfg_->nframes;
  int image_w = image_.w;
  int image_h = image_.h;
  glm::vec4 *image_buf = image_.buf;

  CHECK_EQ(msgcmd_.view_mode, VIEW_MODE_GLFW);

  int64_t nframes = 0;

  while (nframes < cfg_nframes || (cfg_nframes < 0 && !msgcmd_.done)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    image_.clear();
    tracer_.trace();
    image_.composite();

    glDrawPixels(image_w, image_h, GL_RGBA, GL_FLOAT, image_buf);

    Glfw<WbvhEmbree, SceneType>::swapBuffers();
    glfwPollEvents();

    // send command
    MPI_Bcast((void *)&msgcmd_, sizeof(MessageCommand), MPI_UNSIGNED_CHAR, 0,
              MPI_COMM_WORLD);

    // handle command
    Glfw<WbvhEmbree, SceneType>::cmdHandler();

    ++nframes;
  }

#ifdef SPRAY_TIMING
  tStop(TIMER_TOTAL);
#endif

  glfwTerminate();
  msgcmd_.view_mode = VIEW_MODE_TERMINATE;

#ifdef SPRAY_TIMING
  tPrint(nframes);
#endif
}

template <class TracerT>
void SprayRenderer<TracerT>::renderGlfwInOmp() {
#ifdef SPRAY_TIMING
  tReset();
  tStartMPI(TIMER_TOTAL);
#endif

  int64_t shared_nframes;

#pragma omp parallel
  {
    int64_t cfg_nframes = cfg_->nframes;
    int image_w = image_.w;
    int image_h = image_.h;
    glm::vec4 *image_buf = image_.buf;

    CHECK_EQ(msgcmd_.view_mode, VIEW_MODE_GLFW);

    int64_t nframes = 0;
    bool is_root = mpi::isRootProcess();
    bool is_multi = !(mpi::isSingleProcess());

    while (nframes < cfg_nframes || (cfg_nframes < 0 && !msgcmd_.done)) {
#pragma omp master
      {
        if (is_root) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        image_.clear();
      }
#pragma omp barrier
      tracer_.traceInOmp();
#pragma omp barrier
#pragma omp master
      {
        if (is_multi) image_.composite();

        if (is_root) {
          glDrawPixels(image_w, image_h, GL_RGBA, GL_FLOAT, image_buf);

          Glfw<WbvhEmbree, SceneType>::swapBuffers();
          glfwPollEvents();
        }

        // send command
        if (is_multi) {
          MPI_Bcast((void *)&msgcmd_, sizeof(MessageCommand), MPI_UNSIGNED_CHAR,
                    0, MPI_COMM_WORLD);
        }

        // handle command
        Glfw<WbvhEmbree, SceneType>::cmdHandler();
      }
      ++nframes;
#pragma omp barrier
    }

#pragma omp master
    shared_nframes = nframes;
  }  // omp parallel

#ifdef SPRAY_TIMING
  tStop(TIMER_TOTAL);
#endif

  if (mpi::isRootProcess()) glfwTerminate();
  msgcmd_.view_mode = VIEW_MODE_TERMINATE;

#ifdef SPRAY_TIMING
  tPrint(shared_nframes);
#endif
}

template <class TracerT>
void SprayRenderer<TracerT>::renderGlfwChildTask() {
#ifdef SPRAY_TIMING
  tReset();
  tStartMPI(TIMER_TOTAL);
#endif

  int64_t cfg_nframes = cfg_->nframes;
  int64_t nframes = 0;

  while (nframes < cfg_nframes || (cfg_nframes < 0 && !msgcmd_.done)) {
    image_.clear();
    tracer_.trace();
    image_.composite();

    // recv command
    MPI_Bcast((void *)&msgcmd_, sizeof(MessageCommand), MPI_UNSIGNED_CHAR, 0,
              MPI_COMM_WORLD);

    // handle command
    Glfw<WbvhEmbree, SceneType>::cmdHandler();

    ++nframes;
  }

#ifdef SPRAY_TIMING
  tStop(TIMER_TOTAL);
  tPrint(nframes);
#endif

  msgcmd_.view_mode = VIEW_MODE_TERMINATE;
}

template <class TracerT>
void SprayRenderer<TracerT>::renderFilm() {
#if defined(SPRAY_TIMING)
  tReset();
  tStartMPI(TIMER_TOTAL);
#endif

  bool cluster = (mpi::size() > 1);
  int64_t cfg_nframes = cfg_->nframes;

  for (int64_t i = 0; i < cfg_nframes; ++i) {
    image_.clear();
    tracer_.trace();

    if (cluster) image_.composite();
  }

#ifdef SPRAY_TIMING
  tStop(TIMER_TOTAL);
#endif

  bool root = (mpi::rank() == 0);

  if (root) image_.writePpm(cfg_->output_filename.c_str());

#ifdef SPRAY_TIMING
  tPrint(cfg_nframes);
#endif
}

template <class TracerT>
void SprayRenderer<TracerT>::renderFilmInOmp() {
#if defined(SPRAY_TIMING)
  tReset();
  tStartMPI(TIMER_TOTAL);
#endif

  bool cluster = (mpi::size() > 1);
  int64_t cfg_nframes = cfg_->nframes;

#pragma omp parallel firstprivate(cluster, cfg_nframes)
  {
    for (int64_t i = 0; i < cfg_nframes; ++i) {
#pragma omp master
      image_.clear();
#pragma omp barrier
      tracer_.traceInOmp();
#pragma omp barrier
#pragma omp master
      {
        if (cluster) image_.composite();
      }
#pragma omp barrier
    }
  }  // omp parallel

#ifdef SPRAY_TIMING
  tStop(TIMER_TOTAL);
#endif

  bool root = (mpi::rank() == 0);

  if (root) image_.writePpm(cfg_->output_filename.c_str());

#ifdef SPRAY_TIMING
  tPrint(cfg_nframes);
#endif
}

template <class TracerT>
void SprayRenderer<TracerT>::renderGlfwDomainBounds(int view_mode) {
  CHECK_EQ(mpi::size(), 1);

  while (msgcmd_.view_mode == VIEW_MODE_DOMAIN ||
         msgcmd_.view_mode == VIEW_MODE_PARTITION) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj = glm::perspective(
        camera_.getVfov(), camera_.getAspectRatio(), cfg_->znear, cfg_->zfar);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));

    glm::mat4 lookat = glm::lookAt(camera_.getPosition(), camera_.getLookAt(),
                                   camera_.getUp());

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(lookat));

    if (view_mode == VIEW_MODE_PARTITION) {
      scene_.drawPartitions();
    } else {
      scene_.drawDomains();
    }

    Glfw<WbvhEmbree, SceneType>::swapBuffers();
    glfwPollEvents();
    Glfw<WbvhEmbree, SceneType>::cmdHandler();
  }

  glfwTerminate();
}

}  // namespace spray
