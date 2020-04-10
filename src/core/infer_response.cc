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

#include "src/core/infer_response.h"

#include "src/core/logging.h"
#include "src/core/server.h"

namespace nvidia { namespace inferenceserver {

//
// InferenceResponseFactory
//
Status
InferenceResponseFactory::CreateResponse(
    std::unique_ptr<InferenceResponse>* response) const
{
  response->reset(new InferenceResponse(
      backend_, id_, allocator_, alloc_fn_, release_fn_, alloc_userp_));

  return Status::Success;
}

//
// InferenceResponse
//
Status
InferenceResponse::AddOutput(
    const std::string& name, const DataType datatype,
    const std::vector<int64_t>& shape)
{
  outputs_.emplace_back(
      name, datatype, shape, allocator_, alloc_fn_, release_fn_, alloc_userp_);

  LOG_VERBOSE(1) << "add response output: " << outputs_.back();

  return Status::Success;
}

//
// InferenceResponse::Output
//
InferenceResponse::Output::~Output()
{
  Status status = ReleaseBuffer();
  if (!status.IsOk()) {
    LOG_ERROR << "failed to release buffer for output '" << name_
              << "': " << status.AsString();
  }
}

Status
InferenceResponse::Output::Buffer(
    void** buffer, size_t* buffer_byte_size,
    TRITONSERVER_Memory_Type* memory_type, int64_t* memory_type_id)
{
  *buffer = allocated_buffer_;
  *buffer_byte_size = allocated_buffer_byte_size_;
  *memory_type = allocated_memory_type_;
  *memory_type_id = allocated_memory_type_id_;
  return Status::Success;
}

Status
InferenceResponse::Output::AllocateBuffer(
    void** buffer, size_t buffer_byte_size,
    TRITONSERVER_Memory_Type* memory_type, int64_t* memory_type_id)
{
  if (allocated_buffer_ != nullptr) {
    return Status(
        Status::Code::ALREADY_EXISTS,
        "allocated buffer for output '" + name_ + "' already exists");
  }

  TRITONSERVER_Memory_Type actual_memory_type = *memory_type;
  int64_t actual_memory_type_id = *memory_type_id;
  void* alloc_buffer_userp = nullptr;

  RETURN_IF_TRITONSERVER_ERROR(alloc_fn_(
      allocator_, name_.c_str(), buffer_byte_size, *memory_type,
      *memory_type_id, alloc_userp_, buffer, &alloc_buffer_userp,
      &actual_memory_type, &actual_memory_type_id));

  allocated_buffer_ = *buffer;
  allocated_buffer_byte_size_ = buffer_byte_size;
  allocated_memory_type_ = actual_memory_type;
  allocated_memory_type_id_ = actual_memory_type_id;
  allocated_userp_ = alloc_buffer_userp;

  *memory_type = actual_memory_type;
  *memory_type_id = actual_memory_type_id;

  return Status::Success;
}

Status
InferenceResponse::Output::ReleaseBuffer()
{
  TRITONSERVER_Error* err = nullptr;

  if (allocated_buffer_ != nullptr) {
    err = release_fn_(
        allocator_, allocated_buffer_, allocated_userp_,
        allocated_buffer_byte_size_, allocated_memory_type_,
        allocated_memory_type_id_);
  }

  allocated_buffer_ = nullptr;
  allocated_buffer_byte_size_ = 0;
  allocated_memory_type_ = TRITONSERVER_MEMORY_CPU;
  allocated_memory_type_id_ = 0;
  allocated_userp_ = nullptr;

  RETURN_IF_TRITONSERVER_ERROR(err);

  return Status::Success;
}

std::ostream&
operator<<(std::ostream& out, const InferenceResponse& response)
{
  out << "[0x" << std::addressof(response) << "] "
      << "response id: " << response.Id() << ", model: " << response.ModelName()
      << ", actual version: " << response.ActualModelVersion() << std::endl;

  out << "status:" << response.ResponseStatus().AsString() << std::endl;

  out << "outputs:" << std::endl;
  for (const auto& output : response.Outputs()) {
    out << "[0x" << std::addressof(output) << "] " << output << std::endl;
  }

  return out;
}

std::ostream&
operator<<(std::ostream& out, const InferenceResponse::Output& output)
{
  out << "output: " << output.Name()
      << ", type: " << DataTypeToProtocolString(output.DType())
      << ", shape: " << DimsListToString(output.Shape());
  return out;
}

}}  // namespace nvidia::inferenceserver
