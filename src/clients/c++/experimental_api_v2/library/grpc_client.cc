// Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#define DLL_EXPORTING

#include "src/clients/c++/experimental_api_v2/library/grpc_client.h"

#include <grpcpp/grpcpp.h>
#include <cstdint>
#include <future>
#include <iostream>

namespace nvidia { namespace inferenceserver { namespace client {

namespace {

//==============================================================================

// Use map to keep track of GRPC channels. <key, value> : <url, Channel*>
// If context is created on url that has established Channel, then reuse it.
std::map<std::string, std::shared_ptr<grpc::Channel>> grpc_channel_map_;
std::shared_ptr<grpc::Channel>
GetChannel(const std::string& url)
{
  const auto& channel_itr = grpc_channel_map_.find(url);
  if (channel_itr != grpc_channel_map_.end()) {
    return channel_itr->second;
  } else {
    grpc::ChannelArguments arguments;
    arguments.SetMaxSendMessageSize(MAX_GRPC_MESSAGE_SIZE);
    arguments.SetMaxReceiveMessageSize(MAX_GRPC_MESSAGE_SIZE);
    std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(
        url, grpc::InsecureChannelCredentials(), arguments);
    grpc_channel_map_.insert(std::make_pair(url, channel));
    return channel;
  }
}

void
InitModelInferRequest(ModelInferRequest* request, const InferOptions& options)
{
  // Populate the request protobuf
  request->set_model_name(options.model_name_);
  if (!options.model_version_.empty()) {
    request->set_model_version(options.model_version_);
  }
  if (!options.request_id_.empty()) {
    request->set_id(options.request_id_);
  }

  if (options.sequence_id_ != 0) {
    (*request->mutable_parameters())["sequence_id"].set_int64_param(
        options.sequence_id_);
    (*request->mutable_parameters())["sequence_start"].set_bool_param(
        options.sequence_start_);
    (*request->mutable_parameters())["sequence_end"].set_bool_param(
        options.sequence_end_);
  }

  if (options.priority_ != 0) {
    (*request->mutable_parameters())["priority"].set_int64_param(
        options.priority_);
  }

  if (options.timeout_ != 0) {
    (*request->mutable_parameters())["timeout"].set_int64_param(
        options.timeout_);
  }
}

}  // namespace

//==============================================================================


Error
InferenceServerGrpcClient::Create(
    std::unique_ptr<InferenceServerGrpcClient>* client,
    const std::string& server_url, bool verbose)
{
  client->reset(new InferenceServerGrpcClient(server_url, verbose));
  return Error::Success;
}

Error
InferenceServerGrpcClient::IsServerLive(bool* live, const Headers& headers)
{
  Error err;

  ServerLiveRequest request;
  ServerLiveResponse response;
  grpc::ClientContext context;

  for (const auto& it : headers) {
    context.AddMetadata(it.first, it.second);
  }

  grpc::Status grpc_status = stub_->ServerLive(&context, request, &response);
  if (grpc_status.ok()) {
    *live = response.live();
    if (verbose_) {
      std::cout << "Server Live : " << *live << std::endl;
    }
  } else {
    err = Error(grpc_status.error_message());
  }

  return err;
}

Error
InferenceServerGrpcClient::IsServerReady(bool* ready, const Headers& headers)
{
  Error err;

  ServerReadyRequest request;
  ServerReadyResponse response;
  grpc::ClientContext context;

  for (const auto& it : headers) {
    context.AddMetadata(it.first, it.second);
  }

  grpc::Status grpc_status = stub_->ServerReady(&context, request, &response);
  if (grpc_status.ok()) {
    *ready = response.ready();
    if (verbose_) {
      std::cout << "Server Ready : " << *ready << std::endl;
    }
  } else {
    err = Error(grpc_status.error_message());
  }

  return err;
}

Error
InferenceServerGrpcClient::IsModelReady(
    bool* ready, const std::string& model_name,
    const std::string& model_version, const Headers& headers)
{
  Error err;

  ModelReadyRequest request;
  ModelReadyResponse response;
  grpc::ClientContext context;

  for (const auto& it : headers) {
    context.AddMetadata(it.first, it.second);
  }

  request.set_name(model_name);
  request.set_version(model_version);
  grpc::Status grpc_status = stub_->ModelReady(&context, request, &response);
  if (grpc_status.ok()) {
    *ready = response.ready();
    if (verbose_) {
      std::cout << "Model Ready : name: " << model_name;
      if (!model_version.empty()) {
        std::cout << "(version: " << model_version << ") ";
      }
      std::cout << ": " << *ready << std::endl;
    }
  } else {
    err = Error(grpc_status.error_message());
  }

  return err;
}


Error
InferenceServerGrpcClient::GetServerMetadata(
    ServerMetadataResponse* server_metadata, const Headers& headers)
{
  server_metadata->Clear();
  Error err;

  ServerMetadataRequest request;
  grpc::ClientContext context;

  for (const auto& it : headers) {
    context.AddMetadata(it.first, it.second);
  }

  grpc::Status grpc_status =
      stub_->ServerMetadata(&context, request, server_metadata);
  if (grpc_status.ok()) {
    if (verbose_) {
      std::cout << server_metadata->DebugString() << std::endl;
    }
  } else {
    err = Error(grpc_status.error_message());
  }

  return err;
}


Error
InferenceServerGrpcClient::GetModelMetadata(
    ModelMetadataResponse* model_metadata, const std::string& model_name,
    const std::string& model_version, const Headers& headers)
{
  model_metadata->Clear();
  Error err;

  ModelMetadataRequest request;
  grpc::ClientContext context;

  for (const auto& it : headers) {
    context.AddMetadata(it.first, it.second);
  }

  request.set_name(model_name);
  request.set_version(model_version);
  grpc::Status grpc_status =
      stub_->ModelMetadata(&context, request, model_metadata);
  if (grpc_status.ok()) {
    if (verbose_) {
      std::cout << model_metadata->DebugString() << std::endl;
    }
  } else {
    err = Error(grpc_status.error_message());
  }

  return err;
}


Error
InferenceServerGrpcClient::GetModelConfig(
    ModelConfigResponse* model_config, const std::string& model_name,
    const std::string& model_version, const Headers& headers)
{
  model_config->Clear();
  Error err;

  ModelConfigRequest request;
  grpc::ClientContext context;

  for (const auto& it : headers) {
    context.AddMetadata(it.first, it.second);
  }

  request.set_name(model_name);
  request.set_version(model_version);
  grpc::Status grpc_status =
      stub_->ModelConfig(&context, request, model_config);
  if (grpc_status.ok()) {
    if (verbose_) {
      std::cout << model_config->DebugString() << std::endl;
    }
  } else {
    err = Error(grpc_status.error_message());
  }

  return err;
}

Error
InferenceServerGrpcClient::Infer(
    std::shared_ptr<InferResultGrpc>* result, const InferOptions& options,
    const std::vector<std::shared_ptr<InferInputGrpc>>& inputs,
    const std::vector<std::shared_ptr<InferOutputGrpc>>& outputs,
    const Headers& headers)
{
  Error err;

  ModelInferRequest request;
  std::shared_ptr<ModelInferResponse> response_ptr(new ModelInferResponse());

  InitModelInferRequest(&request, options);
  for (auto& input : inputs) {
    request.mutable_inputs()->Add(input->GetTensor());
  }
  for (auto& output : outputs) {
    request.mutable_outputs()->Add(output->GetTensor());
  }

  grpc::ClientContext context;
  for (const auto& it : headers) {
    context.AddMetadata(it.first, it.second);
  }

  grpc::Status grpc_status =
      stub_->ModelInfer(&context, request, response_ptr.get());

  if (grpc_status.ok()) {
    if (verbose_) {
      std::cout << response_ptr->DebugString() << std::endl;
    }
  } else {
    err = Error(grpc_status.error_message());
  }

  InferResultGrpc::Create(result, response_ptr);

  return err;
}

InferenceServerGrpcClient::InferenceServerGrpcClient(
    const std::string& url, bool verbose)
    : stub_(GRPCInferenceService::NewStub(GetChannel(url))), verbose_(verbose)
{
}

//==============================================================================

}}}  // namespace nvidia::inferenceserver::client
