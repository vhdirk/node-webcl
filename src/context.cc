// Copyright (c) 2011-2012, Motorola Mobility, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the Motorola Mobility, Inc. nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "context.h"
#include "device.h"
#include "commandqueue.h"
#include "event.h"
#include "platform.h"
#include "memoryobject.h"
#include "program.h"
#include "sampler.h"
#include "cl_checks.h"

#include <node_buffer.h>
#include <vector>

using namespace node;
using namespace v8;

namespace webcl {

Persistent<Function> Context::constructor;

void Context::Init(Handle<Object> exports)
{
  NanScope();

  // constructor
  Local<FunctionTemplate> ctor = NanNew<FunctionTemplate>(Context::New);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(NanNew<String>("WebCLContext"));

  // prototype
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getInfo", getInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createProgram", createProgram);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createCommandQueue", createCommandQueue);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createBuffer", createBuffer);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createImage", createImage);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createSampler", createSampler);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createUserEvent", createUserEvent);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createFromGLBuffer", createFromGLBuffer);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createFromGLTexture", createFromGLTexture);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createFromGLRenderbuffer", createFromGLRenderbuffer);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getSupportedImageFormats", getSupportedImageFormats);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_release", release);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_retain", retain);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_releaseAll", releaseAll);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getGLContext", getGLContext);

  NanAssignPersistent<Function>(constructor, ctor->GetFunction());
  exports->Set(NanNew<String>("WebCLContext"), ctor->GetFunction());
}

Context::Context(Handle<Object> wrapper) : context(0)
{
  _type=CLObjType::Context;
}

Context::~Context() {
#ifdef LOGGING
  printf("In ~Context\n");
#endif
}

void Context::Destructor()
{
  if(context) {

    cl_uint count;
    ::clGetContextInfo(context,CL_CONTEXT_REFERENCE_COUNT,sizeof(cl_uint),&count,NULL);
#ifdef LOGGING
    printf("  Destroying Context, CLrefCount is: %d\n",count);
#endif
    ::clReleaseContext(context);
    if(count==1) {
      unregisterCLObj(this);
      context=0;
    }
  }
}

NAN_METHOD(Context::release)
{
#ifdef LOGGING
  printf("[Context::release]\n");
#endif
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  context->Destructor();

  NanReturnUndefined();
}

NAN_METHOD(Context::retain)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  clRetainContext(context->getContext());
  NanReturnUndefined();
}

NAN_METHOD(Context::releaseAll)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());

#ifdef LOGGING
  printf("[Context::releaseAll]\n");
#endif
  AtExit(NULL);
  context->Destructor();

  NanReturnUndefined();
}

NAN_METHOD(Context::getInfo)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_context_info param_name = args[0]->Uint32Value();

  switch (param_name) {
  case CL_CONTEXT_REFERENCE_COUNT:
  case CL_CONTEXT_NUM_DEVICES: {
    cl_uint param_value=0;
    cl_int ret=::clGetContextInfo(context->getContext(),param_name,sizeof(cl_uint), &param_value, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_CONTEXT);
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }
    NanReturnValue(JS_INT(param_value));
  }
  case CL_CONTEXT_DEVICES: {
    size_t n=0;
    cl_int ret=::clGetContextInfo(context->getContext(),param_name,0,NULL, &n);
    n /= sizeof(cl_device_id);

    cl_device_id *devices=new cl_device_id[n];
    ret=::clGetContextInfo(context->getContext(),param_name,sizeof(cl_device_id)*n, devices, NULL);
    if (ret != CL_SUCCESS) {
      delete[] devices;
      REQ_ERROR_THROW(INVALID_CONTEXT);
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    Local<Array> arr = NanNew<Array>((int)n);
    for(uint32_t i=0;i<n;i++) {
      if(devices[i]) {
        WebCLObject *obj=findCLObj((void*)devices[i], CLObjType::Device);

        if(obj)
          arr->Set(i,NanObjectWrapHandle(obj));
        else
          arr->Set(i,NanObjectWrapHandle(Device::New(devices[i])));
      }
    }
    delete[] devices;
    NanReturnValue(arr);
  }
  case CL_CONTEXT_PROPERTIES: {
    size_t n=0;
    cl_int ret=::clGetContextInfo(context->getContext(),param_name,0,NULL, &n);
    cl_context_properties *ctx=new cl_context_properties[n];
    ret=::clGetContextInfo(context->getContext(),param_name,sizeof(cl_context_properties)*n, ctx, NULL);
    if (ret != CL_SUCCESS) {
	  delete[] ctx;
      REQ_ERROR_THROW(INVALID_CONTEXT);
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    Local<Array> arr = NanNew<Array>((int)n);
    for(uint32_t i=0;i<n;i++) {
      arr->Set(i,JS_INT((int32_t)ctx[i]));
    }
	delete[] ctx;
    NanReturnValue(arr);
  }
  default: {
    cl_int ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnUndefined();
  }
  }
}

NAN_METHOD(Context::createProgram)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_program pw=NULL;
  cl_int ret = CL_SUCCESS;

  if(args[0]->IsNull() || args[0]->IsUndefined()) {
    ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnUndefined();
  }

  // either we pass a code (string) or binary buffers
  if(args[0]->IsString()) {
    Local<String> str = args[0]->ToString();
    NanAsciiString astr(str);

    size_t lengths[]={(size_t) astr.length()};
    const char *strings[]={*astr};
    pw=::clCreateProgramWithSource(context->getContext(), 1, strings, lengths, &ret);

    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_CONTEXT);
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }
    NanReturnValue(NanObjectWrapHandle(Program::New(pw, context)));
  }
  else if(args[0]->IsArray()){
    Local<Array> devArray = Local<Array>::Cast(args[0]);
    const size_t num=devArray->Length();
    vector<cl_device_id> devices;

    for (uint32_t i=0; i<num; i++) {
      Device *device = ObjectWrap::Unwrap<Device>(devArray->Get(i)->ToObject());
      devices.push_back(device->getDevice());
    }

    Local<Array> binArray = Local<Array>::Cast(args[1]);
    const ::size_t n = binArray->Length();
    ::size_t* lengths = new size_t[n];
    const unsigned char** images =  new const unsigned char*[n];

    for (uint32_t i = 0; i < n; ++i) {
      Local<Object> obj=binArray->Get(i)->ToObject();
        images[i] = (const unsigned char*) obj->GetIndexedPropertiesExternalArrayData();
        lengths[i] = obj->GetIndexedPropertiesExternalArrayDataLength();
    }

    pw=::clCreateProgramWithBinary(
                context->getContext(), (cl_uint) devices.size(),
                &devices.front(),
                lengths, images,
                NULL, &ret);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_CONTEXT);
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(INVALID_DEVICE);
      REQ_ERROR_THROW(INVALID_BINARY);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    // TODO should we return binaryStatus?
    NanReturnValue(NanObjectWrapHandle(Program::New(pw, context)));
  }

  NanReturnUndefined();
}

NAN_METHOD(Context::createCommandQueue)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_device_id device = 0;
  cl_command_queue_properties properties = 0;
  cl_int ret=CL_SUCCESS;
  cl_command_queue cw;
  cl_context ctx=context->getContext();

  if(args[0]->IsNull() || args[0]->IsObject()) {
    if(!args[1]->IsUndefined())
      properties = args[1]->Uint32Value();

    if(args[0]->IsNull()) {
      size_t nDevices=0;
      ret = ::clGetContextInfo(ctx,CL_CONTEXT_NUM_DEVICES,0,NULL, &nDevices);
      // printf("Found %d devices in context\n",nDevices);

      cl_device_id *devices=new cl_device_id[nDevices];
      ret = ::clGetContextInfo(ctx,CL_CONTEXT_DEVICES,sizeof(cl_device_id)*nDevices, devices, NULL);
      if (ret != CL_SUCCESS) {
        delete[] devices;
        REQ_ERROR_THROW(INVALID_CONTEXT);
        REQ_ERROR_THROW(INVALID_VALUE);
        REQ_ERROR_THROW(OUT_OF_RESOURCES);
        REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
        return NanThrowError("UNKNOWN ERROR");
      }

      bool device_found = false;
      device=devices[0];
      if(properties==0) {
        cl_device_type type;
          // ret = ::clGetDeviceInfo(devices[0], CL_DEVICE_TYPE, sizeof(cl_device_type), &type, NULL);
          // printf("Device 0, type %d, ret %d: %d\n",type,ret, devices[0]);

        for(size_t j=0;j<nDevices;j++) {
          ret = ::clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(cl_device_type), &type, NULL);
          // printf("Device %d, type %d, ret %d: %d\n",j,type,ret, devices[j]);
          if(type==CL_DEVICE_TYPE_GPU) {
            // printf("Selecting device %d: %d\n",j,devices[j]);
            device=devices[j];
            break;
          }
        }
        device_found = true;
      }
      delete[] devices;
    }
    else
      device = ObjectWrap::Unwrap<Device>(args[0]->ToObject())->getDevice();

  }
  else {
    // find a device automatically that support properties in that context
    if(!args[0]->IsUndefined())
      properties = args[0]->Uint32Value();

    size_t nDevices=0;
    ret = ::clGetContextInfo(ctx,CL_CONTEXT_NUM_DEVICES,0,NULL, &nDevices);
    // printf("Found %d devices in context\n",nDevices);

    cl_device_id *devices=new cl_device_id[nDevices];
    ret = ::clGetContextInfo(ctx,CL_CONTEXT_DEVICES,sizeof(cl_device_id)*nDevices, devices, NULL);
    if (ret != CL_SUCCESS) {
      delete[] devices;
      REQ_ERROR_THROW(INVALID_CONTEXT);
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    bool device_found = false;
    if(properties == 0) {
      // no property, choose the first one, preferably GPU
      device=devices[0];
      cl_device_type type;
        // ret = ::clGetDeviceInfo(devices[0], CL_DEVICE_TYPE, sizeof(cl_device_type), &type, NULL);
        // printf("Device 0, type %d, ret %d: %d\n",type,ret, devices[0]);

      for(size_t j=0;j<nDevices;j++) {
        ret = ::clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(cl_device_type), &type, NULL);
        // printf("Device %d, type %d, ret %d: %d\n",j,type,ret, devices[j]);
        if(type==CL_DEVICE_TYPE_GPU) {
          // printf("Selecting device %d: %d\n",j,devices[j]);
          device=devices[j];
          break;
        }
      }
      device_found = true;
    }

    for(size_t j=0;j<nDevices && !device_found;j++) {
      cl_command_queue_properties device_q_props=0;
      ret = ::clGetDeviceInfo(devices[j], CL_DEVICE_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties),
                              &device_q_props, NULL);
      // printf("Device %d, Qproperties %d, ret=%d\n",j,device_q_props,ret);

      if (ret != CL_SUCCESS) {
        delete[] devices;
        REQ_ERROR_THROW(INVALID_DEVICE);
        REQ_ERROR_THROW(INVALID_VALUE);
        REQ_ERROR_THROW(OUT_OF_RESOURCES);
        REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
        return NanThrowError("UNKNOWN ERROR");
      }
      if(device_q_props & properties) {
        device=devices[j];
        device_found=true;
        break;
      }
    }
    delete[] devices;
  }

  // printf("Using device %p\n",device);
  int hasOutOfOrderExec=false;
  clGetDeviceInfo(device,CL_DEVICE_QUEUE_PROPERTIES,sizeof(int),&hasOutOfOrderExec,NULL);

  if((properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) && !hasOutOfOrderExec) {
    // some drivers silently accept the flag even if they don't support the feature
    ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnNull();
  }

  // printf("context = %p device=%p properties %llu\n",context->getContext(),device,properties);
  cw = ::clCreateCommandQueue(ctx, device, properties, &ret);
  // printf("clCreateCommandQueue ret=%d\n",ret);

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_DEVICE);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_QUEUE_PROPERTIES);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(CommandQueue::New(cw, context)));
}

NAN_METHOD(Context::createBuffer)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_mem_flags flags = args[0]->Uint32Value();
  size_t size = args[1]->Uint32Value();
  void *host_ptr = NULL;
  if(!args[2]->IsNull() && !args[2]->IsUndefined()) {
    if(args[2]->IsArray()) {
      // JS Array
      Local<Array> arr=Local<Array>::Cast(args[2]);
      host_ptr=arr->GetIndexedPropertiesExternalArrayData();
    }
    else if(args[2]->IsObject()) {
      Local<Object> obj=args[2]->ToObject();
      NanAsciiString name(obj->GetConstructorName());
      if(!strcmp("Buffer",*name))
        host_ptr=Buffer::Data(obj);
      else {
        // TypedArray
        assert(obj->HasIndexedPropertiesInExternalArrayData());
        host_ptr=obj->GetIndexedPropertiesExternalArrayData();
        // printf("external array data type %d\n",obj->GetIndexedPropertiesExternalArrayDataType());
      }
    }
    else
      NanThrowError("Invalid memory object");
  }

  cl_int ret=CL_SUCCESS;
  cl_mem mw = ::clCreateBuffer(context->getContext(), flags, size, host_ptr, &ret);
  // printf("cl_mem %p, ret %d (%s)\n",mw,ret,ErrorDesc(ret));

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_BUFFER_SIZE);
    REQ_ERROR_THROW(INVALID_HOST_PTR);
    REQ_ERROR_THROW(MEM_OBJECT_ALLOCATION_FAILURE);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(WebCLBuffer::New(mw, context)));
}

NAN_METHOD(Context::createImage)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_mem_flags flags = args[0]->Uint32Value();

  Local<Object> obj = args[1]->ToObject();
  cl_image_format image_format;
  image_format.image_channel_order = obj->Get(JS_STR("channelOrder"))->Uint32Value();
  image_format.image_channel_data_type = obj->Get(JS_STR("channelType"))->Uint32Value();

  size_t width = obj->Get(JS_STR("width"))->Uint32Value();;
  size_t height = obj->Get(JS_STR("height"))->Uint32Value();
  size_t row_pitch =  obj->Get(JS_STR("rowPitch"))->IsUndefined() ? 0 : obj->Get(JS_STR("rowPitch"))->Uint32Value();

  void *host_ptr=NULL;
  if(!args[2]->IsNull() && !args[2]->IsUndefined() && args[2]->IsObject()) {
    Local<Object> obj=args[2]->ToObject();
    NanAsciiString name(obj->GetConstructorName());
    if(!strcmp("Buffer",*name))
      host_ptr=Buffer::Data(obj);
    else
      host_ptr = obj->GetIndexedPropertiesExternalArrayData();
  }
  cl_int ret=CL_SUCCESS;
  cl_mem mw;

#ifndef CL_VERSION_1_2
  bool is2D = obj->Get(JS_STR("depth"))->IsUndefined();
  if(is2D) {
    mw = ::clCreateImage2D(
                context->getContext(), flags, &image_format,
                width, height, row_pitch,
                host_ptr, &ret);

  }
  else {
    size_t depth = obj->Get(JS_STR("depth"))->IsUndefined() ? 0 : obj->Get(JS_STR("depth"))->Uint32Value();
    size_t slice_pitch =obj->Get(JS_STR("slicePitch"))->IsUndefined() ? 0 : obj->Get(JS_STR("slicePitch"))->Uint32Value();
    mw = ::clCreateImage3D(
                context->getContext(), flags, &image_format,
                width, height, depth, row_pitch,
                slice_pitch, host_ptr, &ret);
  }
#else
  cl_image_desc desc;
  memset(&desc,0,sizeof(cl_image_desc));

  desc.image_type = obj->Get(JS_STR("depth"))->IsUndefined() ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_IMAGE3D;
  desc.image_width = width;
  desc.image_height = height;
  desc.image_depth = obj->Get(JS_STR("depth"))->IsUndefined() ? 0 : obj->Get(JS_STR("depth"))->Uint32Value();
  desc.image_array_size = 1;
  desc.image_row_pitch = row_pitch;
  desc.image_slice_pitch =obj->Get(JS_STR("slicePitch"))->IsUndefined() ? 0 :obj->Get(JS_STR("slicePitch"))->Uint32Value();

  // printf("size %d x %d, rowPitch %d, host ptr: %p\n",width,height,row_pitch, host_ptr);

  mw = ::clCreateImage(
              context->getContext(), flags,
              &image_format, &desc,
              host_ptr, &ret);
#endif

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_IMAGE_FORMAT_DESCRIPTOR);
    REQ_ERROR_THROW(INVALID_IMAGE_DESCRIPTOR);
    REQ_ERROR_THROW(INVALID_IMAGE_SIZE);
    REQ_ERROR_THROW(INVALID_HOST_PTR);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(IMAGE_FORMAT_NOT_SUPPORTED);
    REQ_ERROR_THROW(MEM_OBJECT_ALLOCATION_FAILURE);
    REQ_ERROR_THROW(INVALID_OPERATION);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
#ifdef CL_VERSION_1_2
    REQ_ERROR_THROW(INVALID_IMAGE_DESCRIPTOR);
#endif
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(WebCLImage::New(mw, context)));
}

NAN_METHOD(Context::createSampler)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_bool normalized_coords = args[0]->BooleanValue() ? CL_TRUE : CL_FALSE;
  cl_addressing_mode addressing_mode = args[1]->Uint32Value();
  cl_filter_mode filter_mode = args[2]->Uint32Value();

  if(!context->getContext()) {
    cl_int ret=CL_INVALID_CONTEXT;
    REQ_ERROR_THROW(INVALID_CONTEXT);
  }

  cl_int ret=CL_SUCCESS;
  cl_sampler sw = ::clCreateSampler(
              context->getContext(),
              normalized_coords,
              addressing_mode,
              filter_mode,
              &ret);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_OPERATION);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(Sampler::New(sw, context)));
}

NAN_METHOD(Context::getSupportedImageFormats)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_mem_flags flags = args[0]->IsUndefined() ? CL_MEM_READ_WRITE : args[0]->Uint32Value();
  cl_mem_object_type image_type = (args[0]->IsUndefined() || args[1]->IsUndefined()) ? CL_MEM_OBJECT_IMAGE2D : args[1]->Uint32Value();
  cl_uint numEntries=0;
  cl_int ret = CL_SUCCESS;

  if(flags > (CL_MEM_HOST_NO_ACCESS<<1)) {
    ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnNull();
  }

  ret = ::clGetSupportedImageFormats(
             context->getContext(),
             flags,
             image_type,
             0,
             NULL,
             &numEntries);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  cl_image_format* image_formats = new cl_image_format[numEntries];
  ret = ::clGetSupportedImageFormats(
      context->getContext(),
      flags,
      image_type,
      numEntries,
      image_formats,
      NULL);

  if (ret != CL_SUCCESS) {
    delete[] image_formats;
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  Local<Array> imageFormats = NanNew<Array>();
  for (uint32_t i=0; i<numEntries; i++) {
    Local<Object> format = NanNew<Object>();
    format->Set(JS_STR("channelOrder"), JS_INT(image_formats[i].image_channel_order));
    format->Set(JS_STR("channelType"), JS_INT(image_formats[i].image_channel_data_type));
    format->Set(JS_STR("rowPitch"), JS_INT(0));
    format->Set(JS_STR("slicePitch"), JS_INT(0));
    format->Set(JS_STR("width"), JS_INT(0));
    format->Set(JS_STR("height"), JS_INT(0));
    imageFormats->Set(i, format);
  }
  delete[] image_formats;
  NanReturnValue(imageFormats);
}

NAN_METHOD(Context::createUserEvent)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_int ret=CL_SUCCESS;

  cl_event ew=::clCreateUserEvent(context->getContext(),&ret);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(UserEvent::New(ew, context)));
}

NAN_METHOD(Context::createFromGLBuffer)
{
  NanScope();
  int ret;
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_mem_flags flags = args[0]->Uint32Value();
  if(!args[1]->IsNumber()) {
    ret=CL_INVALID_GL_OBJECT;
    REQ_ERROR_THROW(INVALID_GL_OBJECT);
    NanReturnNull();
  }

  cl_GLuint bufobj = args[1]->Uint32Value();
  #ifdef LOGGING
  cout<<"createFromGLBuffer flags="<<hex<<flags<<dec<<", bufobj="<<bufobj<<endl;
  #endif

  if(!validateMemFlags((int)flags)) {
    ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnNull();
  }

  if(context->webgl_context_.IsEmpty()) {
    ret=CL_INVALID_CONTEXT;
    REQ_ERROR_THROW(INVALID_CONTEXT);
    NanReturnNull();
  }

  cl_mem clmem = ::clCreateFromGLBuffer(context->getContext(),flags,bufobj,&ret);
  #ifdef LOGGING
  cout<<" -> clmem="<<hex<<clmem<<dec<<endl;
  #endif
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_GL_OBJECT);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(WebCLBuffer::New(clmem, context)));
}

NAN_METHOD(Context::createFromGLTexture)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_mem_flags flags = args[0]->Uint32Value();
  cl_GLenum target = args[1]->Uint32Value();
  cl_GLint miplevel = args[2]->Uint32Value();
  int ret;
  if(!args[3]->IsNumber()) {
    ret=CL_INVALID_GL_OBJECT;
    REQ_ERROR_THROW(INVALID_GL_OBJECT);
    NanReturnNull();
  }
  cl_GLuint texture = args[3]->Uint32Value();

  if(context->webgl_context_.IsEmpty()) {
    ret=CL_INVALID_CONTEXT;
    REQ_ERROR_THROW(INVALID_CONTEXT);
    NanReturnNull();
  }

  if(miplevel<0) {
    ret=CL_INVALID_MIP_LEVEL;
    REQ_ERROR_THROW(INVALID_MIP_LEVEL);
    NanReturnNull();
  }

  cl_mem clmem;
#ifdef CL_VERSION_1_2
  clmem = ::clCreateFromGLTexture(context->getContext(),flags,target,miplevel,texture,&ret);
#elif defined(CL_VERSION_1_1)
  clmem = ::clCreateFromGLTexture2D(context->getContext(),flags,target,miplevel,texture,&ret);
#endif

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_MIP_LEVEL);
    REQ_ERROR_THROW(INVALID_GL_OBJECT);
    REQ_ERROR_THROW(INVALID_IMAGE_FORMAT_DESCRIPTOR);
    REQ_ERROR_THROW(INVALID_OPERATION);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(WebCLImage::New(clmem, context)));
}

NAN_METHOD(Context::createFromGLRenderbuffer)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_mem_flags flags = args[0]->Uint32Value();
  cl_GLuint renderbuffer = args[1]->Uint32Value();
  int ret;
  if(context->webgl_context_.IsEmpty()) {
    ret=CL_INVALID_CONTEXT;
    REQ_ERROR_THROW(INVALID_CONTEXT);
    NanReturnNull();
  }
  cl_mem clmem = ::clCreateFromGLRenderbuffer(context->getContext(),flags,renderbuffer, &ret);

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_GL_OBJECT);
    REQ_ERROR_THROW(INVALID_IMAGE_FORMAT_DESCRIPTOR);
    REQ_ERROR_THROW(INVALID_OPERATION);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(WebCLBuffer::New(clmem, context)));
}

NAN_METHOD(Context::getGLContext)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());

  if(context->webgl_context_.IsEmpty()) {
    NanThrowError(NanObjectWrapHandle(WebCLException::New("WEBCL_EXTENSION_NOT_ENABLED", ErrorDesc(WEBCL_EXTENSION_NOT_ENABLED), WEBCL_EXTENSION_NOT_ENABLED)));
    NanReturnUndefined();
  }

  // TODO this must returns the WebGLRenderingContext object used to create CLGL context
  NanReturnValue(NanNew(context->webgl_context_));
}

#ifdef HAS_clGetContextInfo // disabled for now as this is not supported in all drivers
NAN_METHOD(Context::getGLContextInfo)
{
  NanScope();
  Context *context = ObjectWrap::Unwrap<Context>(args.This());
  cl_context ctx = context->getContext();
  cl_int ret = CL_SUCCESS;

  // retrieve context properties
  size_t numProps=0;
  ret = ::clGetContextInfo(context->getContext(),CL_CONTEXT_PROPERTIES,0,NULL,&numProps);
  if (ret != CL_SUCCESS)
  {
    return NanThrowError("Can NOT get content info!");
  }

  // given the way we createContext(), we should always have these properties for GL
  if(numProps==0)
    NanReturnUndefined();

  cl_context_properties *properties=new cl_context_properties[numProps];
  ret = ::clGetContextInfo(ctx,CL_CONTEXT_PROPERTIES,numProps,properties,NULL);

  // get GL context info
  cl_device_id device=0;
#ifdef __APPLE__
  ret = clGetGLContextInfoAPPLE(ctx, properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &device, NULL);
#else
  ret = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &device, NULL);
#endif

  cl_device_id *devicesCL=NULL;
  size_t numDevicesCL=0;
#ifdef __APPLE__
  ret = clGetGLContextInfoAPPLE(ctx, properties,CL_DEVICES_FOR_GL_CONTEXT_KHR, 0, NULL, &numDevicesCL);
#else
  ret = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, 0, NULL, &numDevicesCL);
#endif

  if(numDevicesCL>0) {
    devicesCL=new cl_device_id[numDevicesCL];
#ifdef __APPLE__
    ret = clGetGLContextInfoAPPLE(ctx, properties,CL_DEVICES_FOR_GL_CONTEXT_KHR, numDevicesCL, devicesCL, NULL);
#else
	ret = clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, numDevicesCL, devicesCL, NULL);
#endif
  }

  if(ret != CL_SUCCESS) {
    delete[] properties;
    delete[] devicesCL;
    REQ_ERROR_THROW(INVALID_GL_SHAREGROUP_REFERENCE_KHR);
    REQ_ERROR_THROW(INVALID_OPERATION);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  Local<Array> arr = NanNew<Array>(numDevicesCL);
  arr->Set(0,NanObjectWrapHandle(Device::New(device)));
  for(size_t i=0,j=1;i<numDevicesCL;i++)
    if(devicesCL[i]!=device)
      arr->Set(j++,NanObjectWrapHandle(Device::New(devicesCL[i])));

  delete[] properties;
  delete[] devicesCL;
  NanReturnValue(arr);
}
#endif

NAN_METHOD(Context::New)
{
  if (!args.IsConstructCall())
    return NanThrowTypeError("Constructor cannot be called as a function.");

  NanScope();
  Context *ctx = new Context(args.This());
  ctx->Wrap(args.This());
  NanReturnValue(args.This());
}

Context *Context::New(cl_context cw)
{

  NanScope();

  Local<Function> cons = NanNew<Function>(constructor);
  Local<Object> obj = cons->NewInstance();

  Context *context = ObjectWrap::Unwrap<Context>(obj);
  context->context = cw;
  registerCLObj(cw, context);

  return context;
}

Context *Context::New(cl_context cw, Handle<Object> webgl_context)
{

  NanScope();

  Local<Function> cons = NanNew<Function>(constructor);
  Local<Object> obj = cons->NewInstance();

  Context *context = ObjectWrap::Unwrap<Context>(obj);
  context->context = cw;
  registerCLObj(cw, context);
  if(!webgl_context.IsEmpty())
    NanAssignPersistent(context->webgl_context_, webgl_context);

  return context;
}

}
