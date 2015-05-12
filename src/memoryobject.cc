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

#include "memoryobject.h"
#include "context.h"
#include <node_buffer.h>

using namespace v8;
using namespace node;

namespace webcl {

Persistent<Function> MemoryObject::constructor;

void MemoryObject::Init(Handle<Object> exports)
{
  NanScope();

  // constructor
  Local<FunctionTemplate> ctor = FunctionTemplate::New(v8::Isolate::GetCurrent(), MemoryObject::New);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(NanNew<String>("WebCLMemoryObject"));

  // prototype
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getInfo", getInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getGLObjectInfo", getGLObjectInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_release", release);

  NanAssignPersistent<Function>(constructor, ctor->GetFunction());
  exports->Set(NanNew<String>("WebCLMemoryObject"), ctor->GetFunction());
}

MemoryObject::MemoryObject(Handle<Object> wrapper) : memory(0)
{
  _type=CLObjType::MemoryObject;
}

MemoryObject::~MemoryObject() {
#ifdef LOGGING
  printf("In ~MemoryObject\n");
#endif
  // Destructor();
}

void MemoryObject::Destructor() {
  if(memory) {
    cl_uint count;
    ::clGetMemObjectInfo(memory,CL_MEM_REFERENCE_COUNT,sizeof(cl_uint),&count,NULL);
#ifdef LOGGING
    cout<<"  Destroying MemoryObject, CLrefCount is: "<<count<<endl;
#endif
    ::clReleaseMemObject(memory);
    if(count==1) {
      unregisterCLObj(this);
      memory=0;
    }
  }
}

NAN_METHOD(MemoryObject::release)
{
  NanScope();

  MemoryObject *mo = ObjectWrap::Unwrap<MemoryObject>(args.This());
#ifdef LOGGING
  printf("  In MemoryObject::release%p\n",mo);
#endif
  mo->Destructor();

  NanReturnUndefined();
}

NAN_METHOD(MemoryObject::getInfo)
{
  NanScope();

  MemoryObject *mo = ObjectWrap::Unwrap<MemoryObject>(args.This());
  cl_mem_info param_name = args[0]->Uint32Value();

  switch (param_name) {
  case CL_MEM_TYPE: {
    cl_mem_object_type param_value=0;
    cl_int ret=::clGetMemObjectInfo(mo->getMemory(),param_name,sizeof(cl_mem_object_type), &param_value, NULL);
     if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(INVALID_MEM_OBJECT);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    NanReturnValue(NanNew<Uint32>(param_value));
  }
  case CL_MEM_FLAGS: {
    cl_mem_flags param_value=0;
    cl_int ret=::clGetMemObjectInfo(mo->getMemory(),param_name,sizeof(cl_mem_flags), &param_value, NULL);
     if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(INVALID_MEM_OBJECT);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    NanReturnValue(NanNew<Uint32>((uint32_t)param_value));
  }
  case CL_MEM_SIZE:
  case CL_MEM_OFFSET: {
    size_t param_value=0;
    cl_int ret=::clGetMemObjectInfo(mo->getMemory(),param_name,sizeof(size_t), &param_value, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(INVALID_MEM_OBJECT);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    NanReturnValue(JS_INT((int32_t)param_value));
  }
  case CL_MEM_ASSOCIATED_MEMOBJECT: {
    cl_mem param_value=NULL;
    cl_int ret=::clGetMemObjectInfo(mo->getMemory(),param_name,sizeof(cl_mem), &param_value, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(INVALID_MEM_OBJECT);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }

    if(param_value) {
      WebCLObject *obj=findCLObj((void*)param_value, CLObjType::MemoryObject);
      if(obj)
        NanReturnValue(NanObjectWrapHandle(obj));
      else
        NanReturnValue(NanObjectWrapHandle(MemoryObject::New(param_value)));
    }
    NanReturnNull();
  }
  case CL_MEM_CONTEXT: {
    cl_context ctx=NULL;
    cl_int ret=::clGetMemObjectInfo(mo->getMemory(),param_name,sizeof(cl_context), &ctx, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(INVALID_MEM_OBJECT);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }
    if(ctx) {
      WebCLObject *obj=findCLObj((void*)ctx, CLObjType::Context);
      if(obj)
        NanReturnValue(NanObjectWrapHandle(obj));
      else
        NanReturnValue(NanObjectWrapHandle(Context::New(ctx)));
    }
    NanReturnNull();
  }
  case CL_MEM_HOST_PTR: {
    char *param_value=NULL;
    cl_int ret=::clGetMemObjectInfo(mo->getMemory(),param_name,sizeof(char*), &param_value, NULL);
    if (ret != CL_SUCCESS) {
      REQ_ERROR_THROW(INVALID_VALUE);
      REQ_ERROR_THROW(INVALID_MEM_OBJECT);
      REQ_ERROR_THROW(OUT_OF_RESOURCES);
      REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
      return NanThrowError("UNKNOWN ERROR");
    }
    size_t nbytes = *(size_t*)param_value;
    NanReturnValue(NanNewBufferHandle(param_value, (int)nbytes));
  }
  default: {
    cl_int ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnUndefined();
  }
  }
}

NAN_METHOD(MemoryObject::getGLObjectInfo)
{
  NanScope();
  MemoryObject *memobj = ObjectWrap::Unwrap<MemoryObject>(args.This());
  cl_gl_object_type gl_object_type = 0;
  cl_GLuint gl_object_name = 0;
  int ret = ::clGetGLObjectInfo(memobj->getMemory(), &gl_object_type, &gl_object_name);

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_MEM_OBJECT);
    REQ_ERROR_THROW(INVALID_GL_OBJECT);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  Local<Array> arr=NanNew<Array>();
  arr->Set(JS_STR("glObject"), JS_INT(gl_object_name));
  arr->Set(JS_STR("type"), JS_INT(gl_object_type));
  if(gl_object_type==CL_GL_OBJECT_TEXTURE2D || gl_object_type==CL_GL_OBJECT_TEXTURE3D) {
    int textureTarget=0, mipmapLevel=0;
    ::clGetGLTextureInfo(memobj->getMemory(),CL_GL_TEXTURE_TARGET,sizeof(GLenum),&textureTarget,NULL);
    ::clGetGLTextureInfo(memobj->getMemory(),CL_GL_MIPMAP_LEVEL,sizeof(GLint),&mipmapLevel,NULL);
    arr->Set(JS_STR("textureTarget"), JS_INT(textureTarget));
    arr->Set(JS_STR("mipmapLevel"), JS_INT(mipmapLevel));
  }

  NanReturnValue(arr);
}

NAN_METHOD(MemoryObject::New)
{
  NanScope();
  MemoryObject *mo = new MemoryObject(args.This());
  mo->Wrap(args.This());
  NanReturnValue(args.This());
}

MemoryObject *MemoryObject::New(cl_mem mw)
{

  NanScope();

  Local<Function> cons = NanNew<Function>(constructor);
  Local<Object> obj = cons->NewInstance();

  MemoryObject *memobj = ObjectWrap::Unwrap<MemoryObject>(obj);
  memobj->memory = mw;
  registerCLObj(mw, memobj);

  return memobj;
}

///////////////////////////////////////////////////////////////////////////////
// WebCLBuffer
///////////////////////////////////////////////////////////////////////////////

Persistent<Function> WebCLBuffer::constructor;

void WebCLBuffer::Init(Handle<Object> exports)
{
  NanScope();

  // constructor
  Local<FunctionTemplate> ctor = FunctionTemplate::New(v8::Isolate::GetCurrent(), WebCLBuffer::New);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(NanNew<String>("WebCLBuffer"));

  // prototype
  NODE_SET_PROTOTYPE_METHOD(ctor, "_createSubBuffer", createSubBuffer);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getInfo", getInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getGLObjectInfo", getGLObjectInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_release", release);

  NanAssignPersistent<Function>(constructor, ctor->GetFunction());
  exports->Set(NanNew<String>("WebCLBuffer"), ctor->GetFunction());
}

WebCLBuffer::WebCLBuffer(Handle<Object> wrapper) : MemoryObject(wrapper),isSubBuffer_(false)
{
}

NAN_METHOD(WebCLBuffer::getInfo)
{
  return MemoryObject::getInfo(args);
}

NAN_METHOD(WebCLBuffer::getGLObjectInfo)
{
  return MemoryObject::getGLObjectInfo(args);
}

NAN_METHOD(WebCLBuffer::release)
{
  NanScope();

  MemoryObject *mo = (MemoryObject*) ObjectWrap::Unwrap<WebCLBuffer>(args.This());
  mo->Destructor();

  NanReturnUndefined();
}

// CL 1.1
NAN_METHOD(WebCLBuffer::createSubBuffer)
{
  NanScope();
  WebCLBuffer *mo = ObjectWrap::Unwrap<WebCLBuffer>(args.This());
  cl_int ret=CL_SUCCESS;

  // can'treate subbuffer from a subbuffer
  if(mo->isSubBuffer()) {
    ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnNull();
  }

  cl_mem_flags flags = args[0]->Uint32Value();

  // can't create subbuffer with different flags than parent buffer
  cl_mem_flags pflags;
  clGetMemObjectInfo(mo->getMemory(),CL_MEM_FLAGS,sizeof(cl_mem_flags),&pflags,NULL);
  if(((pflags & CL_MEM_READ_ONLY) && (flags & (CL_MEM_WRITE_ONLY | CL_MEM_READ_WRITE))) ||
     ((pflags & CL_MEM_WRITE_ONLY) && (flags & (CL_MEM_READ_WRITE | CL_MEM_READ_ONLY)))
  ) {
    ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnNull();
  }

  cl_buffer_region region;
  region.origin = args[1]->Uint32Value();
  region.size = args[2]->Uint32Value();

  if(region.origin>=region.size) {
    ret=CL_INVALID_VALUE;
    REQ_ERROR_THROW(INVALID_VALUE);
    NanReturnNull();
  }
  // bug on Mac to avoid core dump
  cl_mem parent=NULL;
  ::clGetMemObjectInfo(mo->getMemory(),CL_MEM_ASSOCIATED_MEMOBJECT,sizeof(cl_mem),&parent,NULL);
  if(parent) {
    ret=CL_INVALID_MEM_OBJECT;
    REQ_ERROR_THROW(INVALID_MEM_OBJECT);
    NanReturnNull();
  }

  cl_mem sub_buffer = ::clCreateSubBuffer(
      mo->getMemory(),
      flags,
      CL_BUFFER_CREATE_TYPE_REGION,
      &region,
      &ret);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_MEM_OBJECT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_BUFFER_SIZE);
    REQ_ERROR_THROW(MEM_OBJECT_ALLOCATION_FAILURE);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  WebCLBuffer *newsub=WebCLBuffer::New(sub_buffer, mo);
  newsub->isSubBuffer_=true;

  NanReturnValue(NanObjectWrapHandle(newsub));
}

NAN_METHOD(WebCLBuffer::New)
{
  NanScope();
  WebCLBuffer *mo = new WebCLBuffer(args.This());
  mo->Wrap(args.This());
  NanReturnValue(args.This());
}

WebCLBuffer *WebCLBuffer::New(cl_mem mw, WebCLObject *parent)
{
  NanScope();

  Local<Function> cons = NanNew<Function>(constructor);
  Local<Object> obj = cons->NewInstance();

  WebCLBuffer *memobj = ObjectWrap::Unwrap<WebCLBuffer>(obj);
  memobj->memory = mw;
  registerCLObj(mw, memobj);

  return memobj;
}

///////////////////////////////////////////////////////////////////////////////
// WebCLImage
///////////////////////////////////////////////////////////////////////////////

Persistent<Function> WebCLImage::constructor;

void WebCLImage::Init(Handle<Object> exports)
{
  NanScope();

  // constructor
  Local<FunctionTemplate> ctor = FunctionTemplate::New(v8::Isolate::GetCurrent(), WebCLImage::New);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(NanNew<String>("WebCLImage"));

  // prototype
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getInfo", getInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getGLObjectInfo", getGLObjectInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_getGLTextureInfo", getGLTextureInfo);
  NODE_SET_PROTOTYPE_METHOD(ctor, "_release", release);

  NanAssignPersistent<Function>(constructor, ctor->GetFunction());
  exports->Set(NanNew<String>("WebCLImage"), ctor->GetFunction());
}

WebCLImage::WebCLImage(Handle<Object> wrapper) : MemoryObject(wrapper)
{
}

NAN_METHOD(WebCLImage::release)
{
  NanScope();

  MemoryObject *mo = (MemoryObject*) ObjectWrap::Unwrap<WebCLImage>(args.This());
  mo->Destructor();

  NanReturnUndefined();
}

NAN_METHOD(WebCLImage::getInfo)
{
  NanScope();
  WebCLImage *mo = ObjectWrap::Unwrap<WebCLImage>(args.This());;

  cl_image_format param_value;
  cl_int ret=::clGetImageInfo(mo->getMemory(),CL_IMAGE_FORMAT,sizeof(cl_image_format), &param_value, NULL);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_MEM_OBJECT);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  size_t w,h,d,rp,sp;
  ret |= ::clGetImageInfo(mo->getMemory(),CL_IMAGE_WIDTH,sizeof(size_t), &w, NULL);
  ret |= ::clGetImageInfo(mo->getMemory(),CL_IMAGE_HEIGHT,sizeof(size_t), &h, NULL);
  ret |= ::clGetImageInfo(mo->getMemory(),CL_IMAGE_DEPTH,sizeof(size_t), &d, NULL);
  ret |= ::clGetImageInfo(mo->getMemory(),CL_IMAGE_ROW_PITCH,sizeof(size_t), &rp, NULL);
  ret |= ::clGetImageInfo(mo->getMemory(),CL_IMAGE_SLICE_PITCH,sizeof(size_t), &sp, NULL);

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_MEM_OBJECT);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  WebCLImageDescriptor* obj = WebCLImageDescriptor::New(
    param_value.image_channel_order,param_value.image_channel_data_type,
    (int)w,(int)h,(int)d,
    rp,sp
  );
  NanReturnValue(NanObjectWrapHandle(obj));
}

NAN_METHOD(WebCLImage::getGLObjectInfo)
{
  return MemoryObject::getGLObjectInfo(args);
}

NAN_METHOD(WebCLImage::getGLTextureInfo)
{
  NanScope();
  WebCLImage *memobj = ObjectWrap::Unwrap<WebCLImage>(args.This());;
  cl_gl_texture_info param_name = args[0]->Uint32Value();
  GLint param_value;

  // TODO no other value that GLenum/GLint returned in OpenCL 1.1
  int ret = ::clGetGLTextureInfo(memobj->getMemory(), param_name, sizeof(GLint), &param_value, NULL);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_MEM_OBJECT);
    REQ_ERROR_THROW(INVALID_GL_OBJECT);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(JS_INT(param_value));
}

NAN_METHOD(WebCLImage::New)
{
  NanScope();
  WebCLImage *mo = new WebCLImage(args.This());
  mo->Wrap(args.This());
  NanReturnValue(args.This());
}

WebCLImage *WebCLImage::New(cl_mem mw, WebCLObject *parent)
{

  NanScope();

  Local<Function> cons = NanNew<Function>(constructor);
  Local<Object> obj = cons->NewInstance();

  WebCLImage *memobj = ObjectWrap::Unwrap<WebCLImage>(obj);
  memobj->memory = mw;
  registerCLObj(mw, memobj);

  return memobj;
}

///////////////////////////////////////////////////////////////////////////////
// WebCLImageDescriptor
///////////////////////////////////////////////////////////////////////////////
Persistent<Function> WebCLImageDescriptor::constructor;

void WebCLImageDescriptor::Init(Handle<Object> exports)
{
  NanScope();

  // constructor
  Local<FunctionTemplate> ctor = FunctionTemplate::New(v8::Isolate::GetCurrent(), WebCLImageDescriptor::New);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(NanNew<String>("WebCLImageDescriptor"));

  // prototype
  Local<ObjectTemplate> proto = ctor->PrototypeTemplate();
  proto->SetAccessor(JS_STR("channelOrder"), WebCLImageDescriptor::getChannelOrder);
  proto->SetAccessor(JS_STR("channelType"), WebCLImageDescriptor::getChannelType);
  proto->SetAccessor(JS_STR("width"), WebCLImageDescriptor::getWidth);
  proto->SetAccessor(JS_STR("height"), WebCLImageDescriptor::getHeight);
  proto->SetAccessor(JS_STR("depth"), WebCLImageDescriptor::getDepth);
  proto->SetAccessor(JS_STR("rowPitch"), WebCLImageDescriptor::getRowPitch);
  proto->SetAccessor(JS_STR("slicePitch"), WebCLImageDescriptor::getSlicePitch);

  NanAssignPersistent<Function>(constructor, ctor->GetFunction());
  exports->Set(NanNew<String>("WebCLImageDescriptor"), ctor->GetFunction());
}

WebCLImageDescriptor::WebCLImageDescriptor(Handle<Object> wrapper) :
  channelOrder(0), channelType(0),
  width(0), height(0), depth(0),
  rowPitch(0), slicePitch(0)
{
}

NAN_GETTER(WebCLImageDescriptor::getChannelOrder) {
  NanScope();

  WebCLImageDescriptor* desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(args.This());
  NanReturnValue(JS_INT(desc->channelOrder));
}

NAN_GETTER(WebCLImageDescriptor::getChannelType) {
  NanScope();

  WebCLImageDescriptor* desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(args.This());
  NanReturnValue(JS_INT(desc->channelType));
}

NAN_GETTER(WebCLImageDescriptor::getWidth) {
  NanScope();

  WebCLImageDescriptor* desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(args.This());
  NanReturnValue(JS_INT(desc->width));
}

NAN_GETTER(WebCLImageDescriptor::getHeight) {
  NanScope();

  WebCLImageDescriptor* desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(args.This());
  NanReturnValue(JS_INT(desc->height));
}

NAN_GETTER(WebCLImageDescriptor::getDepth) {
  NanScope();

  WebCLImageDescriptor* desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(args.This());
  NanReturnValue(JS_INT(desc->depth));
}

NAN_GETTER(WebCLImageDescriptor::getRowPitch) {
  NanScope();

  WebCLImageDescriptor* desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(args.This());
  NanReturnValue(JS_INT(desc->rowPitch));
}

NAN_GETTER(WebCLImageDescriptor::getSlicePitch) {
  NanScope();

  WebCLImageDescriptor* desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(args.This());
  NanReturnValue(JS_INT(desc->slicePitch));
}

NAN_METHOD(WebCLImageDescriptor::New)
{
  NanScope();
  WebCLImageDescriptor *mo = new WebCLImageDescriptor(args.This());
  mo->Wrap(args.This());

  NanReturnValue(args.This());
}

WebCLImageDescriptor *WebCLImageDescriptor::New(int order, int type, int w, int h, int d, int rp, int sp)
{

  NanScope();

  Local<Function> cons = NanNew<Function>(constructor);
  Local<Object> obj = cons->NewInstance();

  WebCLImageDescriptor *desc = ObjectWrap::Unwrap<WebCLImageDescriptor>(obj);
  desc->channelOrder=order;
  desc->channelType=type;
  desc->width=w;
  desc->height=h;
  desc->depth=d;
  desc->rowPitch=rp;
  desc->slicePitch=sp;

  return desc;
}

} // namespace webcl
