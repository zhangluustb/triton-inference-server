..
  # Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
  #
  # Redistribution and use in source and binary forms, with or without
  # modification, are permitted provided that the following conditions
  # are met:
  #  * Redistributions of source code must retain the above copyright
  #    notice, this list of conditions and the following disclaimer.
  #  * Redistributions in binary form must reproduce the above copyright
  #    notice, this list of conditions and the following disclaimer in the
  #    documentation and/or other materials provided with the distribution.
  #  * Neither the name of NVIDIA CORPORATION nor the names of its
  #    contributors may be used to endorse or promote products derived
  #    from this software without specific prior written permission.
  #
  # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

.. _section-building:

Building
========

The Triton Inference Server, the client libraries and examples, and
custom backends can each be built using either Docker or CMake. The
procedure for each is different and is detailed in the corresponding
sections below.

Building the Server
-------------------

The Triton Inference Server can be built in two ways:

* Build using Docker and the TensorFlow and PyTorch containers from
  `NVIDIA GPU Cloud (NGC) <https://ngc.nvidia.com>`_. Before building
  you must install Docker and nvidia-docker and login to the NGC
  registry by following the instructions in
  :ref:`section-installing-prebuilt-containers`.

* Build using CMake and the dependencies (for example, TensorFlow or
  TensorRT library) that you build or install yourself.

.. _section-building-the-server-with-docker:

Building the Server with Docker
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To build a release version of the Triton Inference Server container,
change directory to the root of the repo and checkout the release
version of the branch that you want to build (or the master branch if
you want to build the under-development version)::

  $ git checkout r20.03

Then use docker to build::

  $ docker build --pull -t tritonserver .

Incremental Builds with Docker
..............................

For typical development you will want to run the *build* container
with your local repo’s source files mounted so that your local changes
can be incrementally built. This is done by first building the
*tritonserver_build* container::

  $ docker build --pull -t tritonserver_build --target trtserver_build .

By mounting /path/to/tritonserver/src into the container at
/workspace/src, changes to your local repo will be reflected in the
container::

  $ nvidia-docker run -it --rm -v/path/to/tritonserver/src:/workspace/src tritonserver_build

Within the container you can perform an incremental server build
with::

  # cd /workspace/builddir
  # make -j16 trtis

When the build completes the binary, libraries and headers can be
found in trtis/install. To overwrite the existing versions::

  # cp trtis/install/bin/tritonserver /opt/tritonserver/bin/.
  # cp trtis/install/lib/libtritonserver.so /opt/tritonserver/lib/.

You can reconfigure the build by running *cmake* as described in
:ref:`section-building-the-server-with-cmake`.

.. _section-building-the-server-with-cmake:

Building the Server with CMake
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To build a release version of the Triton Inference Server with
CMake, change directory to the root of the repo and checkout the
release version of the branch that you want to build (or the master
branch if you want to build the under-development version)::

  $ git checkout r20.03

Next you must build or install each framework backend you want to
enable in the inference server, configure the inference server to
enable the desired features, and finally build the server.

.. _section-cmake-dependencies:

Dependencies
............

To include GPU support in the inference server you must install the
necessary CUDA libraries. Similarly, to include support for a
particular framework backend, you must build the appropriate libraries
for that framework and make them available to the inference server
build. In general, the Dockerfile build steps guide how each of these
frameworks can be built for use in the interence server.

CUDA, cuBLAS, cuDNN
~~~~~~~~~~~~~~~~~~~

For the inference server to support NVIDIA GPUs you must install CUDA,
cuBLAS and cuDNN. These libraries must be installed on system include
and library paths so that they are available for the CMake build. The
version of the libraries used in the Dockerfile build can be found in
the `Framework Containers Support Matrix
<https://docs.nvidia.com/deeplearning/frameworks/support-matrix/index.html>`_.

For a given version of the inference server you can attempt to build
with non-supported versions of the libraries but you may have build or
execution issues since non-supported versions are not tested.

Once you have CUDA, cuBLAS and cuDNN installed you can enable GPUs
with the CMake option -DTRTIS_ENABLE_GPU=ON as described below.

TensorRT
~~~~~~~~

The TensorRT includes and libraries must be installed on system
include and library paths so that they are available for the CMake
build. The version of TensorRT used in the Dockerfile build can be
found in the `Framework Containers Support Matrix
<https://docs.nvidia.com/deeplearning/frameworks/support-matrix/index.html>`_.

For a given version of the inference server you can attempt to build
with non-supported versions of TensorRT but you may have build or
execution issues since non-supported versions are not tested.

Once you have TensorRT installed you can enable the TensorRT backend
in the inference server with the CMake option
-DTRTIS_ENABLE_TENSORRT=ON as described below. You must also specify
-DTRTIS_ENABLE_GPU=ON because TensorRT requires GPU support.

TensorFlow
~~~~~~~~~~

The version of TensorFlow used in the Dockerfile build can be found in
the `Framework Containers Support Matrix
<https://docs.nvidia.com/deeplearning/frameworks/support-matrix/index.html>`_.
The trtserver_tf section of the Dockerfile shows the required
TensorFlow V1 container pulled from `NGC <https://ngc.nvidia.com>`_.

You can modify and rebuild this TensorFlow container to generate the
libtensorflow_trtis.so shared library needed by the inference
server. For example, in the TensorFlow container
/workspace/docker-examples/Dockerfile.customtensorflow shows a
Dockerfile that applies a patch to TensorFlow and then rebuilds. For
the inference server you need to replace the nvbuild commands in that
file with::

  RUN ./nvbuild.sh --python3.6 --trtis

In the newly build container the required TensorFlow library is
/usr/local/lib/tensorflow/libtensorflow_trtis.so.1. On your build
system you must place libtensorflow_trtis.so.1 in a system library
path or you can specify the path with the CMake option
TRTIS_EXTRA_LIB_PATHS. Multiple paths can be specified by separating
them with a semicolon, for example,
-DTRTIS_EXTRA_LIB_PATHS="/path/a;/path/b". Also create a soft link to
the library as follows::

  ln -s libtensorflow_trtis.so.1 libtensorflow_trtis.so

Lastly, you must enable the TensorFlow backend in the inference server
with the CMake option -DTRTIS_ENABLE_TENSORFLOW=ON as described below.

ONNX Runtime
~~~~~~~~~~~~

The version of the ONNX Runtime used in the Dockerfile build can be
found in the trtserver_onnx section of the Dockerfile. That section
also details the steps that can be used to build the backend. You can
attempt to build a different version of the ONNX Runtime or use a
different build process but you may have build or execution issues.

Your build should produce the ONNX Runtime library, libonnxruntime.so.
You can enable the ONNX Runtime backend in the inference server with
the CMake option -DTRTIS_ENABLE_ONNXRUNTIME=ON as described below. If
you want to enable TensorRT within the ONNX Runtime you must also
specify the CMake option TRTIS_ENABLE_ONNXRUNTIME_TENSORRT=ON and
provide the necessary TensorRT dependencies. If you want to enable
OpenVino within the ONNX Runtime you must also specify the CMake
option TRTIS_ENABLE_ONNXRUNTIME_OPENVINO=ON and provide the necessary
OpenVino dependencies.

You can install the library in a system library path or you can
specify the path with the CMake option TRTIS_EXTRA_LIB_PATHS. Multiple
paths can be specified by separating them with a semicolon, for
example, -DTRTIS_EXTRA_LIB_PATHS="/path/a;/path/b".

You must also provide the path to the ONNX Runtime headers using the
-DTRTIS_ONNXRUNTIME_INCLUDE_PATHS option. Multiple paths can be
specified by separating them with a semicolon.

PyTorch and Caffe2
~~~~~~~~~~~~~~~~~~

The version of PyTorch and Caffe2 used in the Dockerfile build can be
found in the `Framework Containers Support Matrix
<https://docs.nvidia.com/deeplearning/frameworks/support-matrix/index.html>`_.
The trtserver_caffe2 section of the Dockerfile shows how to build the
required PyTorch and Caffe2 libraries from the `NGC
<https://ngc.nvidia.com>`_ PyTorch container.

You can build and install a different version of the libraries but if
you want to enable the Caffe2 backend you must include
netdef_backend_c2.cc and netdef_backend.c2.h in the build, as shown in
the Dockerfile.

Once you have the libraries built and installed you can enable the
PyTorch backend in the inference server with the CMake option
-DTRTIS_ENABLE_PYTORCH=ON and the Caffe2 backend with
-DTRTIS_ENABLE_CAFFE2=ON as described below.

You can install the PyTorch library, libtorch.so, and all the required
Caffe2 libraries (see Dockerfile) in a system library path or you can
specify the path with the CMake option TRTIS_EXTRA_LIB_PATHS. Multiple
paths can be specified by separating them with a semicolon, for
example, -DTRTIS_EXTRA_LIB_PATHS="/path/a;/path/b".

For the PyTorch backend you must also provide the path to the PyTorch
headers using the -DTRTIS_PYTORCH_INCLUDE_PATHS option. Multiple paths
can be specified by separating them with a semicolon.

Configure Inference Server
..........................

Use cmake to configure the Triton Inference Server::

  $ mkdir builddir
  $ cd builddir
  $ cmake -D<option0> ... -D<optionn> ../build

The following options are used to enable and disable the different
backends. To enable a backend set the corresponding option to ON, for
example -DTRTIS_ENABLE_TENSORRT=ON. To disable a backend set the
corresponding option to OFF, for example -DTRTIS_ENABLE_TENSORRT=OFF.
By default no backends are enabled. See the section on
:ref:`dependencies<section-cmake-dependencies>` for information on
additional requirements for enabling a backend.

* **TRTIS_ENABLE_TENSORRT**: Use -DTRTIS_ENABLE_TENSORRT=ON to enable
  the TensorRT backend. The TensorRT libraries must be on your library
  path or you must add the path to TRTIS_EXTRA_LIB_PATHS.

* **TRTIS_ENABLE_TENSORFLOW**: Use -DTRTIS_ENABLE_TENSORFLOW=ON to
  enable the TensorFlow backend. The TensorFlow library
  libtensorflow_cc.so must be built as described above and must be on
  your library path or you must add the path to TRTIS_EXTRA_LIB_PATHS.

* **TRTIS_ENABLE_ONNXRUNTIME**: Use -DTRTIS_ENABLE_ONNXRUNTIME=ON to
  enable the OnnxRuntime backend. The library libonnxruntime.so must
  be built as described above and must be on your library path or you
  must add the path to TRTIS_EXTRA_LIB_PATHS.

* **TRTIS_ENABLE_PYTORCH**: Use -DTRTIS_ENABLE_PYTORCH=ON to enable
  the PyTorch backend. The library libtorch.so must be built as
  described above and must be on your library path or you must add the
  path to TRTIS_EXTRA_LIB_PATHS.

* **TRTIS_ENABLE_CAFFE2**: Use -DTRTIS_ENABLE_CAFFE2=ON to enable the
  Caffe2 backend. The library libcaffe2.so and all the other required
  libraries must be built as described above and must be on your
  library path or you must add the path to TRTIS_EXTRA_LIB_PATHS.

* **TRTIS_ENABLE_CUSTOM**: Use -DTRTIS_ENABLE_CUSTOM=ON to enable
  support for custom backends. See
  :ref:`section-building-a-custom-backend` for information on how to
  build a custom backend.

* **TRTIS_ENABLE_ENSEMBLE**: Use -DTRTIS_ENABLE_ENSEMBLE=ON to enable
  support for ensembles.

These additional options may be specified:

* **TRTIS_ENABLE_GRPC**: By default the inference server accepts
  inference, status, health and other requests via the GRPC
  protocol. Use -DTRTIS_ENABLE_GRPC=OFF to disable.

* **TRTIS_ENABLE_HTTP**: By default the inference server accepts
  inference, status, health and other requests via the HTTP
  protocol. Use -DTRTIS_ENABLE_HTTP=OFF to disable.

* **TRTIS_ENABLE_METRICS**: By default the inference server reports
  :ref:`Prometheus metrics<section-metrics>` on an HTTP endpoint. Use
  -DTRTIS_ENABLE_METRICS=OFF to disable both CPU and GPU metrics.
  When disabling metrics must use -DTRTIS_ENABLE_METRICS_GPU=OFF to
  disable GPU metrics.

* **TRTIS_ENABLE_METRICS_GPU**: By default the inference server reports
  :ref:`Prometheus GPU metrics<section-metrics>` on an HTTP endpoint. Use
  -DTRTIS_ENABLE_METRICS_GPU=OFF to disable GPU metrics.

* **TRTIS_ENABLE_TRACING**: By default the inference server does not
  enable detailed :ref:`tracing of individual inference requests
  <section-trace>`. Use -DTRTIS_ENABLE_TRACING=ON to enable.

* **TRTIS_ENABLE_GCS**: Use -DTRTIS_ENABLE_GCS=ON to enable the
  inference server to read model repositories from Google Cloud
  Storage.

* **TRTIS_ENABLE_S3**: Use -DTRTIS_ENABLE_S3=ON to enable the
  inference server to read model repositories from Amazon S3.

* **TRTIS_ENABLE_GPU**: By default the inference server supports
  NVIDIA GPUs. Use -DTRTIS_ENABLE_GPU=OFF to disable GPU support. When
  GPUs are disable the inference server will :ref:`run models on CPU
  when possible <section-running-the-inference-server-without-gpu>`.
  When disabling GPU support must use -DTRTIS_ENABLE_METRICS_GPU=OFF
  to disable GPU metrics.

* **TRTIS_MIN_COMPUTE_CAPABILITY**: By default, the inference server
  supports NVIDIA GPUs with CUDA compute capability 6.0 or higher. If
  all framework backends included in the inference server are built to
  support a lower compute capability, then Triton Inference Server can
  be built to support that lower compute capability by setting
  -DTRTIS_MIN_COMPUTE_CAPABILITY appropriately. The setting is ignored
  if -DTRTIS_ENABLE_GPU=OFF.

* **TRTIS_EXTRA_LIB_PATHS**: Extra paths that are searched for
  framework libraries as described above. Multiple paths can be
  specified by separating them with a semicolon, for example,
  -DTRTIS_EXTRA_LIB_PATHS="/path/a;/path/b".

Build Inference Server
......................

After configuring, build the inference server with make::

  $ cd builddir
  $ make -j16 trtis

When the build completes the binary, libraries and headers can be
found in trtis/install.

.. _section-building-a-custom-backend:

Building A Custom Backend
-------------------------

The source repository contains several example custom backends in the
`src/custom directory
<https://github.com/NVIDIA/triton-inference-server/blob/master/src/custom>`_.
These custom backends are built using CMake::

  $ mkdir builddir
  $ cd builddir
  $ cmake ../build
  $ make -j16 trtis-custom-backends

When the build completes the custom backend libraries can be found in
trtis-custom-backends/install.

A custom backend is not built-into the inference server. Instead it is
built as a separate shared library that the inference server
dynamically loads when the model repository contains a model that uses
that custom backend. There are a couple of ways you can build your
custom backend into a shared library, as described in the following
sections.

Build Using CMake
^^^^^^^^^^^^^^^^^

One way to build your own custom backend is to use the inference
server's CMake build. Simply copy and modify one of the existing
example custom backends and then build your backend using CMake. You
can then use the resulting shared library in your model repository as
described in :ref:`section-custom-backends`.

Build Using Custom Backend SDK
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The custom backend SDK includes all the header files you need to build
your custom backend as well as a static library which provides all the
model configuration and protobuf utility functions you will need. You
can either build the custom backend SDK yourself using
Dockerfile.custombackend::

  docker build -t tritonserver_cbe -f Dockerfile.custombackend .

Or you can download a pre-build version of the SDK from the `GitHub
release page
<https://github.com/NVIDIA/triton-inference-server/releases>`_
corresponding to the release you are interested in. The custom backend
SDK is found in the "Assets" section of the release page in a tar file
named after the version of the release and the OS, for example,
v1.2.0_ubuntu1604.custombackend.tar.gz.

Once you have the SDK you can use the include/ directory and static
library when you compile your custom backend source code. For example,
the SDK includes the source for the *param* custom backend in
src/param.cc. You can create a custom backend from that source using
the following command::

  g++ -fpic -shared -std=c++11 -o libparam.so custom-backend-sdk/src/param.cc -Icustom-backend-sdk/include custom-backend-sdk/lib/libcustombackend.a

Using the Custom Instance Wrapper Class
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The custom backend SDK provides a `CustomInstance Class
<https://github.com/NVIDIA/triton-inference-server/blob/master/src/custom/sdk/custom_instance.h>`_.
The CustomInstance class is a C++ wrapper class that abstracts away the
backend C-API for ease of use. All of the example custom backends in
`src/custom directory
<https://github.com/NVIDIA/triton-inference-server/blob/master/src/custom>`_
derive from the CustomInstance class and can be referenced for usage.

Building the Client Libraries and Examples
------------------------------------------

The provided Dockerfile.client and CMake support can be used to build
the client libraries and examples.

.. include:: client.rst
   :start-after: build-client-begin-marker-do-not-remove
   :end-before: build-client-end-marker-do-not-remove

Building the Documentation
--------------------------

The inference server documentation is found in the docs/ directory and
is based on `Sphinx <http://www.sphinx-doc.org>`_. `Doxygen
<http://www.doxygen.org/>`_ integrated with `Exhale
<https://github.com/svenevs/exhale>`_ is used for C++ API
docuementation.

To build the docs install the required dependencies::

  $ apt-get update
  $ apt-get install -y --no-install-recommends python3-pip doxygen
  $ pip3 install --upgrade setuptools
  $ pip3 install --upgrade sphinx sphinx-rtd-theme nbsphinx exhale

To get the Python client library API docs the Triton Inference
Server Python package must be installed and a couple of files must be copied::

  $ pip install --upgrade tensorrtserver*.whl
  $ cd src/clients/c++/api_v1/library
  $ cp -f request.h.in request.h
  $ cp -f request_grpc.h.in request_grpc.h
  $ cp -f request_http.h.in request_http.h

Then use Sphinx to build the documentation into the build/html
directory::

  $ cd docs
  $ make clean html
