// Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "src/core/api.pb.h"
#include "src/core/model_config.pb.h"
#include "src/core/model_repository_manager.h"
#include "src/core/provider.h"
#include "src/core/server_status.h"
#include "src/core/server_status.pb.h"
#include "src/core/status.h"

namespace nvidia { namespace inferenceserver {

class InferenceBackend;

enum ModelControlMode { MODE_NONE, MODE_POLL, MODE_EXPLICIT };

// Inference server information.
class InferenceServer {
 public:
  // Construct an inference server.
  InferenceServer();

  // Initialize the server. Return true on success, false otherwise.
  Status Init();

  // Stop the server.  Return true if all models are unloaded, false
  // if exit timeout occurs.
  Status Stop();

  // Check the model repository for changes and update server state
  // based on those changes.
  Status PollModelRepository();

  // Server and model health
  Status IsLive(bool* live);
  Status IsReady(bool* ready);
  Status ModelIsReady(
      const std::string& model_name, const int64_t model_version, bool* ready);
  Status ModelReadyVersions(
      const std::string& model_name, std::vector<int64_t>* versions);

  // Inference
  Status InferAsync(const std::shared_ptr<InferenceRequest>& request);

  // Update the ServerStatus object with the status of the model. If
  // 'model_name' is empty, update with the status of all models.
  Status GetStatus(ServerStatus* server_status, const std::string& model_name);

  // Update the ModelRepositoryIndex object with the index of the model
  // repository.
  Status GetModelRepositoryIndex(ModelRepositoryIndex* repository_index);

  // Load the corresponding model. Reload the model if it has been loaded.
  Status LoadModel(const std::string& model_name);

  // Unload the corresponding model.
  Status UnloadModel(const std::string& model_name);

  // Return the ready state for the server.
  ServerReadyState ReadyState() const { return ready_state_; }

  // Return the server version.
  const std::string& Version() const { return version_; }

  // Return the server extensions.
  const std::vector<const char*>& Extensions() const { return extensions_; }

  // Get / set the ID of the server.
  const std::string& Id() const { return id_; }
  void SetId(const std::string& id) { id_ = id; }

  // Get / set the protocol version of the server.
  uint32_t ProtocolVersion() const { return protocol_version_; }
  void SetProtocolVersion(const uint32_t v)
  {
    protocol_version_ = v;
    status_manager_->SetProtocolVersion(v);
  }

  // Get / set the model repository path
  const std::set<std::string>& ModelRepositoryPaths() const
  {
    return model_repository_paths_;
  }

  void SetModelRepositoryPaths(const std::set<std::string>& p)
  {
    model_repository_paths_ = p;
  }

  // Get / set model control mode.
  ModelControlMode GetModelControlMode() const { return model_control_mode_; }
  void SetModelControlMode(ModelControlMode m) { model_control_mode_ = m; }

  // Get / set the startup models
  const std::set<std::string>& StartupModels() const { return startup_models_; }
  void SetStartupModels(const std::set<std::string>& m) { startup_models_ = m; }

  // Get / set strict model configuration enable.
  bool StrictModelConfigEnabled() const { return strict_model_config_; }
  void SetStrictModelConfigEnabled(bool e) { strict_model_config_ = e; }

  // Get / set the pinned memory pool byte size.
  int64_t PinnedMemoryPoolByteSize() const { return pinned_memory_pool_size_; }
  void SetPinnedMemoryPoolByteSize(int64_t s)
  {
    pinned_memory_pool_size_ = std::max((int64_t)0, s);
  }

  // Get / set CUDA memory pool size
  const std::map<int, uint64_t>& CudaMemoryPoolByteSize() const
  {
    return cuda_memory_pool_size_;
  }

  void SetCudaMemoryPoolByteSize(const std::map<int, uint64_t>& s)
  {
    cuda_memory_pool_size_ = s;
  }

  // Get / set the minimum support CUDA compute capability.
  double MinSupportedComputeCapability() const
  {
    return min_supported_compute_capability_;
  }
  void SetMinSupportedComputeCapability(double c)
  {
    min_supported_compute_capability_ = c;
  }

  // Get / set strict readiness enable.
  bool StrictReadinessEnabled() const { return strict_readiness_; }
  void SetStrictReadinessEnabled(bool e) { strict_readiness_ = e; }

  // Get / set the server exit timeout, in seconds.
  int32_t ExitTimeoutSeconds() const { return exit_timeout_secs_; }
  void SetExitTimeoutSeconds(int32_t s) { exit_timeout_secs_ = std::max(0, s); }

  // Get / set Tensorflow soft placement enable.
  bool TensorFlowSoftPlacementEnabled() const
  {
    return tf_soft_placement_enabled_;
  }
  void SetTensorFlowSoftPlacementEnabled(bool e)
  {
    tf_soft_placement_enabled_ = e;
  }

  // Get / set Tensorflow GPU memory fraction.
  float TensorFlowGPUMemoryFraction() const { return tf_gpu_memory_fraction_; }
  void SetTensorFlowGPUMemoryFraction(float f) { tf_gpu_memory_fraction_ = f; }

  // Get / set Tensorflow vGPU memory limits
  const std::map<int, std::pair<int, uint64_t>>& TensorFlowVGPUMemoryLimits()
      const
  {
    return tf_vgpu_memory_limits_;
  }

  void SetTensorFlowVGPUMemoryLimits(
      const std::map<int, std::pair<int, uint64_t>>& memory_limits)
  {
    tf_vgpu_memory_limits_ = memory_limits;
  }

  // Return the status manager for this server.
  std::shared_ptr<ServerStatusManager> StatusManager() const
  {
    return status_manager_;
  }

  // Return the requested InferenceBackend object.
  Status GetInferenceBackend(
      const std::string& model_name, const int64_t model_version,
      std::shared_ptr<InferenceBackend>* backend)
  {
    return model_repository_manager_->GetInferenceBackend(
        model_name, model_version, backend);
  }

 private:
  // Return the uptime of the server in nanoseconds.
  uint64_t UptimeNs() const;

  const std::string version_;
  std::string id_;
  std::vector<const char*> extensions_;

  uint64_t start_time_ns_;
  uint32_t protocol_version_;

  std::set<std::string> model_repository_paths_;
  std::set<std::string> startup_models_;
  ModelControlMode model_control_mode_;
  bool strict_model_config_;
  bool strict_readiness_;
  uint32_t exit_timeout_secs_;
  uint64_t pinned_memory_pool_size_;
  std::map<int, uint64_t> cuda_memory_pool_size_;
  double min_supported_compute_capability_;

  // Tensorflow options
  bool tf_soft_placement_enabled_;
  float tf_gpu_memory_fraction_;
  std::map<int, std::pair<int, uint64_t>> tf_vgpu_memory_limits_;

  // Current state of the inference server.
  ServerReadyState ready_state_;

  // Number of in-flight requests. During shutdown we attempt to wait
  // for all in-flight requests to complete before exiting.
  std::atomic<uint64_t> inflight_request_counter_;

  std::shared_ptr<ServerStatusManager> status_manager_;
  std::unique_ptr<ModelRepositoryManager> model_repository_manager_;
};

}}  // namespace nvidia::inferenceserver
