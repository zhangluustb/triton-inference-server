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

#include "src/core/infer_request.h"

#include <deque>
#include "src/core/backend.h"
#include "src/core/logging.h"
#include "src/core/server.h"

namespace nvidia { namespace inferenceserver {

TRTSERVER_Memory_Type
TritonMemTypeToTrt(TRITONSERVER_Memory_Type mem_type)
{
  switch (mem_type) {
    case TRITONSERVER_MEMORY_CPU:
      return TRTSERVER_MEMORY_CPU;
      break;
    case TRITONSERVER_MEMORY_CPU_PINNED:
      return TRTSERVER_MEMORY_CPU_PINNED;
    default:
      return TRTSERVER_MEMORY_GPU;
      break;
  }
}

TRITONSERVER_Memory_Type
TrtMemTypeToTriton(TRTSERVER_Memory_Type mem_type)
{
  switch (mem_type) {
    case TRTSERVER_MEMORY_CPU:
      return TRITONSERVER_MEMORY_CPU;
      break;
    case TRTSERVER_MEMORY_CPU_PINNED:
      return TRITONSERVER_MEMORY_CPU_PINNED;
    default:
      return TRITONSERVER_MEMORY_GPU;
      break;
  }
}

// FIXME these should be inline once provider is removed and we can
// include backend.h in infer_request.h
const std::string&
InferenceRequest::ModelName() const
{
  return backend_raw_->Name();
}
int64_t
InferenceRequest::ActualModelVersion() const
{
  return backend_raw_->Version();
}
Status
InferenceRequest::Run(std::unique_ptr<InferenceRequest>& request)
{
  return request->backend_raw_->Run(nullptr, request);
}

Status
InferenceRequest::MutableOriginalInput(
    const std::string& name, InferenceRequest::Input** input)
{
  auto itr = original_inputs_.find(name);
  if (itr == original_inputs_.end()) {
    return Status(
        Status::Code::INVALID_ARG,
        "input '" + name + "' does not exist in request");
  }

  *input = &(itr->second);
  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::ImmutableInput(
    const std::string& name, const InferenceRequest::Input** input) const
{
  auto itr = inputs_.find(name);
  if (itr == inputs_.end()) {
    return Status(
        Status::Code::INVALID_ARG,
        "input '" + name + "' does not exist in request");
  }

  *input = itr->second;
  return Status::Success;
}

Status
InferenceRequest::MutableRequestedOutput(
    const std::string& name, RequestedOutput** output)
{
  auto itr = requested_outputs_.find(name);
  if (itr == requested_outputs_.end()) {
    return Status(
        Status::Code::INVALID_ARG,
        "output '" + name + "' does not exist in request");
  }

  *output = &(itr->second);
  needs_normalization_ = true;
  return Status::Success;
}


Status
InferenceRequest::AddOriginalInput(
    const std::string& name, const DimsList& shape,
    const uint64_t batch_byte_size, InferenceRequest::Input** input)
{
  std::vector<int64_t> lshape;
  for (const auto d : shape) {
    lshape.push_back(d);
  }

  return AddOriginalInput(name, lshape, batch_byte_size, input);
}

Status
InferenceRequest::AddOriginalInput(
    const std::string& name, const std::vector<int64_t>& shape,
    const uint64_t batch_byte_size, InferenceRequest::Input** input)
{
  const auto& pr = original_inputs_.emplace(std::make_pair(
      name, InferenceRequest::Input(name, shape, batch_byte_size)));
  if (!pr.second) {
    return Status(
        Status::Code::INVALID_ARG,
        "input '" + name + "' already exists in request");
  }

  LOG_VERBOSE(1) << "add original input: " << *this;

  if (input != nullptr) {
    *input = std::addressof(pr.first->second);
  }

  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::AddOriginalInput(
    const std::string& name, const DataType datatype, const int64_t* shape,
    const uint64_t dim_count, InferenceRequest::Input** input)
{
  const auto& pr = original_inputs_.emplace(std::make_pair(
      name, InferenceRequest::Input(name, datatype, shape, dim_count)));
  if (!pr.second) {
    return Status(
        Status::Code::INVALID_ARG,
        "input '" + name + "' already exists in request");
  }

  LOG_VERBOSE(1) << "add original input: " << *this;

  if (input != nullptr) {
    *input = std::addressof(pr.first->second);
  }

  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::RemoveOriginalInput(const std::string& name)
{
  if (original_inputs_.erase(name) != 1) {
    return Status(
        Status::Code::INVALID_ARG,
        "input '" + name + "' does not exist in request");
  }

  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::RemoveAllOriginalInputs()
{
  original_inputs_.clear();
  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::AddOverrideInput(
    const std::string& name, const DataType datatype,
    const std::vector<int64_t>& shape, const uint64_t batch_byte_size,
    std::shared_ptr<InferenceRequest::Input>* input)
{
  std::shared_ptr<Input> i =
      std::make_shared<Input>(name, datatype, shape, batch_byte_size);
  *(i->MutableShape()) = i->OriginalShape();

  RETURN_IF_ERROR(AddOverrideInput(i));
  if (input != nullptr) {
    *input = std::move(i);
  }

  return Status::Success;
}

Status
InferenceRequest::AddOverrideInput(
    const std::shared_ptr<InferenceRequest::Input>& input)
{
  LOG_VERBOSE(1) << "adding input override for " << input->Name() << ": "
                 << *this;

  const auto& pr =
      override_inputs_.emplace(std::make_pair(input->Name(), input));
  if (!pr.second) {
    pr.first->second = input;
  }

  // Add or replace this override in the inputs...
  const auto res = inputs_.emplace(std::make_pair(input->Name(), input.get()));
  if (!res.second) {
    res.first->second = input.get();
  }

  LOG_VERBOSE(1) << "added input override for " << input->Name() << ": "
                 << *this;

  return Status::Success;
}

Status
InferenceRequest::AddRequestedOutput(
    const std::string& name, const uint32_t classification_cnt)
{
  const auto& pr = requested_outputs_.emplace(std::make_pair(
      name, InferenceRequest::RequestedOutput(name, classification_cnt)));
  if (!pr.second) {
    return Status(
        Status::Code::INVALID_ARG, "output '" + name + "' already requested");
  }

  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::RemoveRequestedOutput(const std::string& name)
{
  if (requested_outputs_.erase(name) != 1) {
    return Status(
        Status::Code::INVALID_ARG,
        "output '" + name + "' does not exist in request");
  }

  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::RemoveAllRequestedOutputs()
{
  requested_outputs_.clear();
  needs_normalization_ = true;
  return Status::Success;
}

Status
InferenceRequest::PrepareForInference()
{
  // Remove override inputs as those are added during any previous
  // inference execution.
  inputs_.clear();
  override_inputs_.clear();

  // If anything has potentially changed in the inference request then
  // need to renormalize.
  if (needs_normalization_) {
    if (protocol_version_ == 1) {
      RETURN_IF_ERROR(NormalizeV1());
    } else {
      RETURN_IF_ERROR(NormalizeV2());
    }

    needs_normalization_ = false;
  }

  // Initially show the actual inputs to be only the original
  // inputs. If overrides are added later they will be added to
  // 'inputs_'.
  for (auto& pr : original_inputs_) {
    inputs_.emplace(std::make_pair(pr.first, std::addressof(pr.second)));
  }

  LOG_VERBOSE(1) << "prepared: " << *this;

  return Status::Success;
}

Status
InferenceRequest::NormalizeV1()
{
  const ModelConfig& model_config = backend_raw_->Config();

  if ((priority_ == 0) || (priority_ > backend_raw_->MaxPriorityLevel())) {
    priority_ = backend_raw_->DefaultPriorityLevel();
  }

  // Make sure the request has a batch-size > 0. Even for models that
  // don't support batching the requested batch size must be 1.
  if (batch_size_ < 1) {
    return Status(
        Status::Code::INVALID_ARG,
        "inference request batch-size must be >= 1 for '" + ModelName() + "'");
  }

  // Make sure request batch-size doesn't exceed what is supported by
  // the model. For models that don't support batching the request
  // batch-size will still be 1.
  if ((batch_size_ != 1) &&
      ((int)batch_size_ > model_config.max_batch_size())) {
    return Status(
        Status::Code::INVALID_ARG,
        "inference request batch-size must be <= " +
            std::to_string(model_config.max_batch_size()) + " for '" +
            ModelName() + "'");
  }

  // Validate if the requested output name exists in the model configuration
  for (const auto& pr : requested_outputs_) {
    const ModelOutput* output_config;
    RETURN_IF_ERROR(backend_raw_->GetOutput(pr.first, &output_config));
  }

  // Make sure that the request is providing the same number of inputs
  // as is expected by the model.
  if (original_inputs_.size() != (size_t)model_config.input_size()) {
    return Status(
        Status::Code::INVALID_ARG,
        "expected " + std::to_string(model_config.input_size()) +
            " inputs but got " + std::to_string(original_inputs_.size()) +
            " inputs for model '" + ModelName() + "'");
  }

  // Update each input to have shape, datatype and batch-byte-size.
  for (auto& pr : original_inputs_) {
    const ModelInput* input_config;
    RETURN_IF_ERROR(backend_raw_->GetInput(pr.first, &input_config));
    auto& input = pr.second;

    input.SetDType(input_config->data_type());

    auto new_shape = input.MutableShape();
    *new_shape = input.OriginalShape();

    // If the inference request specifies a shape for an input, make
    // sure it matches what the model expects.
    if (new_shape->size() > 0) {
      if (!CompareDimsWithWildcard(input_config->dims(), *new_shape)) {
        return Status(
            Status::Code::INVALID_ARG,
            "unexpected shape for input '" + pr.first + "' for model '" +
                ModelName() + "'. Expected " +
                DimsListToString(input_config->dims()) + ", got " +
                DimsListToString(*new_shape));
      }

      // If there is a reshape for this input then set the shape to
      // match the reshape. As reshape may have variable-size
      // dimensions, we need to record corresponding value so that we
      // can set the value correctly for reshape.
      if (input_config->has_reshape()) {
        std::deque<int64_t> variable_size_values;
        for (int64_t idx = 0; idx < input_config->dims_size(); idx++) {
          if (input_config->dims(idx) == -1) {
            variable_size_values.push_back((*new_shape)[idx]);
          }
        }

        new_shape->clear();
        for (const auto& dim : input_config->reshape().shape()) {
          if (dim == -1) {
            new_shape->push_back(variable_size_values.front());
            variable_size_values.pop_front();
          } else {
            new_shape->push_back(dim);
          }
        }
      }
    }

    // If we don't have shape for the input at this point then the
    // request didn't specify it, or it has a reshape that we must use
    // instead.
    if (new_shape->size() == 0) {
      const DimsList& dims = (input_config->has_reshape())
                                 ? input_config->reshape().shape()
                                 : input_config->dims();

      // Inference request doesn't specify shape, make sure input
      // shape is fully specified in the model and calculate expected
      // size from the model configuration.
      for (auto dim : dims) {
        if (dim < 0) {
          return Status(
              Status::Code::INVALID_ARG,
              "model supports variable-size for input '" + pr.first +
                  "', request must specify input shape for model '" +
                  ModelName() + "'");
        }

        new_shape->push_back(dim);
      }
    }

    // For fixed-size datatype the tensor used to calculate byte-size
    // is:
    //
    //   [ batch-size, tensor-shape ] : for batching model and
    //   non-zero-rank tensor. For example, batch-size 4 and dims [ 1,
    //   2 ] the full tensor shape is [ 4, 1, 2 ].
    //
    //   [ tensor-shape ] : for non-batching model and non-zero-rank
    //   tensor. For example, dims [ 1, 2 ] the full tensor shape is [
    //   1, 2 ].
    //
    //   [ batch-size ] : for batching model and zero-rank tensor. For
    //   example, batch-size 4 with dims [ 1 ] and reshape [ ], the
    //   full tensor shape is [ 4 ].
    //
    // Note that non-batching zero-rank tensor is not allowed since
    // that will always be shape [], i.e. a tensor with no contents.
    //
    uint64_t bs = 0;
    if (IsFixedSizeDataType(input_config->data_type())) {
      bs = GetByteSize(input_config->data_type(), *new_shape);
      int multiplier = (input_config->is_shape_tensor() ? 1 : batch_size_);
      if (model_config.max_batch_size() > 0) {
        if (new_shape->size() == 0) {
          bs = GetDataTypeByteSize(input_config->data_type()) * multiplier;
        } else {
          bs *= multiplier;
        }
      }

      // If batch-byte-size is given check to make sure that the
      // calculated batch size matches
      if ((input.BatchByteSize() != 0) && (input.BatchByteSize() != bs)) {
        return Status(
            Status::Code::INVALID_ARG,
            "specific batch-byte-size for input '" + pr.first +
                "' does not match expected byte-size calculated from shape and "
                "datatype for model '" +
                ModelName() + "'");
      }
    } else {
      // The input's datatype is not fixed-sized (like TYPE_STRING),
      // use the full-batch size specified by the request.
      bs = input.BatchByteSize();
    }

    input.SetBatchByteSize(bs);
  }

  return Status::Success;
}

Status
InferenceRequest::NormalizeV2()
{
  const ModelConfig& model_config = backend_raw_->Config();

  if ((priority_ == 0) || (priority_ > backend_raw_->MaxPriorityLevel())) {
    priority_ = backend_raw_->DefaultPriorityLevel();
  }

  // Validate if the requested output name exists in the model configuration
  for (const auto& pr : requested_outputs_) {
    const ModelOutput* output_config;
    RETURN_IF_ERROR(backend_raw_->GetOutput(pr.first, &output_config));
  }

  // Make sure that the request is providing the same number of inputs
  // as is expected by the model.
  if (original_inputs_.size() != (size_t)model_config.input_size()) {
    return Status(
        Status::Code::INVALID_ARG,
        "expected " + std::to_string(model_config.input_size()) +
            " inputs but got " + std::to_string(original_inputs_.size()) +
            " inputs for model '" + ModelName() + "'");
  }

  // Determine the batch size and shape of each input.
  if (model_config.max_batch_size() == 0) {
    // Model does not support Triton-style batching so treat as
    // batch-size 1 and leave the tensor shapes as they are.
    batch_size_ = 1;
    for (auto& pr : original_inputs_) {
      auto& input = pr.second;
      *input.MutableShape() = input.OriginalShape();
    }
  } else {
    // Model does support Triton-style batching so each input tensor
    // must have the same first dimension which is the batch
    // size. Adjust the shape of the input tensors to remove the batch
    // dimension.
    batch_size_ = 0;
    for (auto& pr : original_inputs_) {
      auto& input = pr.second;
      if (input.OriginalShape().size() == 0) {
        return Status(
            Status::Code::INVALID_ARG,
            "input '" + input.Name() +
                "' has no shape but model requires batch dimension for '" +
                ModelName() + "'");
      }

      if (batch_size_ == 0) {
        batch_size_ = input.OriginalShape()[0];
      } else if (input.OriginalShape()[0] != batch_size_) {
        return Status(
            Status::Code::INVALID_ARG,
            "input '" + input.Name() +
                "' batch size does not match other inputs for '" + ModelName() +
                "'");
      }

      input.MutableShape()->assign(
          input.OriginalShape().begin() + 1, input.OriginalShape().end());
    }
  }

  // Make sure the request has a batch-size > 0. Even for models that
  // don't support batching the requested batch size must be 1.
  if (batch_size_ < 1) {
    return Status(
        Status::Code::INVALID_ARG,
        "inference request batch-size must be >= 1 for '" + ModelName() + "'");
  }

  // Make sure request batch-size doesn't exceed what is supported by
  // the model. For models that don't support batching the request
  // batch-size will still be 1.
  if ((batch_size_ != 1) &&
      ((int)batch_size_ > model_config.max_batch_size())) {
    return Status(
        Status::Code::INVALID_ARG,
        "inference request batch-size must be <= " +
            std::to_string(model_config.max_batch_size()) + " for '" +
            ModelName() + "'");
  }

  // Verify that each input shape is valid for the model, make
  // adjustments for reshapes and find the total tensor size.
  for (auto& pr : original_inputs_) {
    const ModelInput* input_config;
    RETURN_IF_ERROR(backend_raw_->GetInput(pr.first, &input_config));

    auto& input = pr.second;
    auto shape = input.MutableShape();

    // FIXMEV2 Can't have this check until the GRPCV2 and HTTPV2
    // endpoints switch to new TRITONSERVER APIs where they always
    // specify datatype.
#if 0
    if (input.DType() != input_config->data_type()) {
      return Status(
          Status::Code::INVALID_ARG,
          "inference input data-type is '" +
              std::string(DataTypeToProtocolString(input.DType())) +
              "', model expects '" +
              std::string(DataTypeToProtocolString(input_config->data_type())) +
              "' for '" + ModelName() + "'");
    }
#else
    input.SetDType(input_config->data_type());
#endif

    if (!CompareDimsWithWildcard(input_config->dims(), *shape)) {
      return Status(
          Status::Code::INVALID_ARG,
          "unexpected shape for input '" + pr.first + "' for model '" +
              ModelName() + "'. Expected " +
              DimsListToString(input_config->dims()) + ", got " +
              DimsListToString(*shape));
    }

    // If there is a reshape for this input then adjust them to
    // match the reshape. As reshape may have variable-size
    // dimensions, we need to record corresponding value so that we
    // can set the value correctly for reshape.
    if (input_config->has_reshape()) {
      std::deque<int64_t> variable_size_values;
      for (int64_t idx = 0; idx < input_config->dims_size(); idx++) {
        if (input_config->dims(idx) == -1) {
          variable_size_values.push_back((*shape)[idx]);
        }
      }

      shape->clear();
      for (const auto& dim : input_config->reshape().shape()) {
        if (dim == -1) {
          shape->push_back(variable_size_values.front());
          variable_size_values.pop_front();
        } else {
          shape->push_back(dim);
        }
      }
    }

    // If no data was given for an input just add an empty memory
    // reference.
    if (input.Data() == nullptr) {
      input.SetData(std::make_shared<MemoryReference>());
    }

    // Get the full size of the data from the input's data. We should
    // ultimately be able to remove this "batch_byte_size" parameter
    // since the same information is in the Memory object.
    input.SetBatchByteSize(input.Data()->TotalByteSize());
  }

  return Status::Success;
}

//
// Input
//
InferenceRequest::Input::Input() : batch_byte_size_(0) {}

InferenceRequest::Input::Input(
    const std::string& name, const std::vector<int64_t>& shape,
    const uint64_t batch_byte_size)
    : name_(name), original_shape_(shape), batch_byte_size_(batch_byte_size)
{
}

InferenceRequest::Input::Input(
    const std::string& name, const DataType datatype, const int64_t* shape,
    const uint64_t dim_count)
    : name_(name), datatype_(datatype),
      original_shape_(shape, shape + dim_count), batch_byte_size_(0)
{
}

InferenceRequest::Input::Input(
    const std::string& name, const DataType datatype,
    const std::vector<int64_t>& shape, const uint64_t batch_byte_size)
    : name_(name), datatype_(datatype), original_shape_(shape),
      batch_byte_size_(batch_byte_size)
{
}


Status
InferenceRequest::Input::AppendData(
    const void* base, size_t byte_size, TRTSERVER_Memory_Type memory_type,
    int64_t memory_type_id)
{
  if (data_ == nullptr) {
    data_ = std::make_shared<MemoryReference>();
  }

  if (byte_size > 0) {
    std::static_pointer_cast<MemoryReference>(data_)->AddBuffer(
        static_cast<const char*>(base), byte_size, memory_type, memory_type_id);
  }

  return Status::Success;
}

Status
InferenceRequest::Input::AppendData(
    const void* base, size_t byte_size, TRITONSERVER_Memory_Type memory_type,
    int64_t memory_type_id)
{
  return AppendData(
      base, byte_size, TritonMemTypeToTrt(memory_type), memory_type_id);
}

Status
InferenceRequest::Input::SetData(const std::shared_ptr<Memory>& data)
{
  if (data_ != nullptr) {
    return Status(
        Status::Code::INVALID_ARG,
        "input '" + name_ + "' already has data, can't overwrite");
  }

  data_ = data;

  return Status::Success;
}

Status
InferenceRequest::Input::RemoveAllData()
{
  data_.reset();
  return Status::Success;
}

Status
InferenceRequest::Input::Content(
    const size_t idx, const void** content, size_t* content_byte_size,
    TRTSERVER_Memory_Type* memory_type, int64_t* memory_type_id) const
{
  if (*content_byte_size == 0) {
    *content = nullptr;
    return Status::Success;
  }

  *content =
      data_->BufferAt(idx, content_byte_size, memory_type, memory_type_id);

  return Status::Success;
}

//
// RequestedOutput
//
InferenceRequest::RequestedOutput::RequestedOutput(
    const std::string& name, const uint32_t classification_cnt)
    : name_(name), classification_cnt_(classification_cnt)
{
}

std::ostream&
operator<<(std::ostream& out, const InferenceRequest& request)
{
  out << "[0x" << std::addressof(request) << "] "
      << "request id: " << request.IdStr() << ", model: " << request.ModelName()
      << ", requested version: " << request.RequestedModelVersion()
      << ", actual version: " << request.ActualModelVersion() << ", flags: 0x"
      << std::hex << request.Flags() << std::dec
      << ", correlation id: " << request.CorrelationId()
      << ", batch size: " << request.BatchSize()
      << ", priority: " << request.Priority()
      << ", timeout (us): " << request.TimeoutMicroseconds() << std::endl;

  out << "original inputs:" << std::endl;
  for (const auto& itr : request.OriginalInputs()) {
    out << "[0x" << std::addressof(itr.second) << "] " << itr.second
        << std::endl;
  }

  out << "override inputs:" << std::endl;
  for (const auto& itr : request.OverrideInputs()) {
    out << "[0x" << itr.second.get() << "] " << *itr.second << std::endl;
  }

  out << "inputs:" << std::endl;
  for (const auto& itr : request.ImmutableInputs()) {
    out << "[0x" << itr.second << "] " << *itr.second << std::endl;
  }

  out << "requested outputs:" << std::endl;
  for (const auto& itr : request.RequestedOutputs()) {
    out << itr.second << std::endl;
  }

  return out;
}

std::ostream&
operator<<(std::ostream& out, const InferenceRequest::Input& input)
{
  out << "input: " << input.Name()
      << ", type: " << DataTypeToProtocolString(input.DType())
      << ", original shape: " << DimsListToString(input.OriginalShape())
      << ", shape: " << DimsListToString(input.Shape());
  return out;
}

std::ostream&
operator<<(std::ostream& out, const InferenceRequest::RequestedOutput& output)
{
  out << "requested output: " << output.Name()
      << ", class count: " << output.ClassificationCount();
  return out;
}

}}  // namespace nvidia::inferenceserver
