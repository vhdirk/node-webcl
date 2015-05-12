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

#ifndef WEBCL_COMMON_H_
#define WEBCL_COMMON_H_

// Node includes
#include <node.h>
#include "nan.h"
#include <string>
#include <list>
#include <set>
#include <map>
#include <memory>
#ifdef LOGGING
#include <iostream>
#endif

using namespace std;

// OpenCL includes
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS

#if defined (__APPLE__) || defined(MACOSX)
  #ifdef __ECLIPSE__
    #include <gltypes.h>
    #include <gl3.h>
    #include <cl_platform.h>
    #include <cl.h>
    #include <cl_gl.h>
    #include <cl_gl_ext.h>
    #include <cl_ext.h>
  #else
    #include <OpenGL/gl3.h>
    #include <OpenGL/gl3ext.h>
    #include <OpenGL/OpenGL.h>
    #include <OpenCL/opencl.h>
    #define CL_GL_CONTEXT_KHR 0x2008
    #define CL_EGL_DISPLAY_KHR 0x2009
    #define CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR CL_INVALID_GL_CONTEXT_APPLE
  #endif
  #define HAS_clGetContextInfo
#elif defined(_WIN32)
    #include <GL/gl.h>
    #include <CL/opencl.h>
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    char *strcasestr(const char *s, char *find);
#else
    #include <GL/gl.h>
    #include <GL/glx.h>
    #include <CL/opencl.h>
#endif

#ifndef CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR
  #define CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR 0x2006
#endif
#ifndef CL_DEVICES_FOR_GL_CONTEXT_KHR
  #define CL_DEVICES_FOR_GL_CONTEXT_KHR 0x2007
#endif

// TODO value not defined in spec
#define WEBCL_EXTENSION_NOT_ENABLED 0x8000

namespace {
#define JS_STR(...) NanNew<v8::String>(__VA_ARGS__)
#define JS_INT(val) NanNew<v8::Integer>(uint32_t(val))
#define JS_NUM(val) NanNew<v8::Number>(val)
#define JS_BOOL(val) NanNew<v8::Boolean>(val)
#define JS_RETHROW(tc) NanNew<v8::Local<v8::Value> >(tc.Exception());

#define REQ_ARGS(N)                                                     \
  if (args.Length() < (N))                                              \
    NanThrowTypeError("Expected " #N " arguments");

#define REQ_STR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    NanThrowTypeError("Argument " #I " must be a string");              \
  String::Utf8Value VAR(args[I]->ToString());

#define REQ_EXT_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())                   \
    NanThrowTypeError("Argument " #I " invalid");                       \
  Local<External> VAR = Local<External>::Cast(args[I]);

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    NanThrowTypeError("Argument " #I " must be a function");            \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_ERROR_THROW_NONE(error) if (ret == CL_##error) NanThrowError(NanObjectWrapHandle(WebCLException::New(#error, ErrorDesc(CL_##error), CL_##error))); return;

#define REQ_ERROR_THROW(error) if (ret == CL_##error) return NanThrowError(NanObjectWrapHandle(WebCLException::New(#error, ErrorDesc(CL_##error), CL_##error)));

#define DESTROY_WEBCL_OBJECT(obj)	\
  obj->Destructor();

#define DISABLE_COPY(ClassName) \
  ClassName( const ClassName& other ); /* non construction-copyable */ \
  ClassName& operator=( const ClassName& ); /* non copyable */

} // namespace

namespace webcl {

const char* ErrorDesc(cl_int err);

// generic baton for async callbacks
struct Baton {
    NanCallback *callback;
    int error;
    char *error_msg;
    uint8_t *priv_info;

    // Custom user data
    v8::Persistent<v8::Value> data;

    // parent of this callback (WebCLEvent object)
    v8::Persistent<v8::Object> parent;

    Baton() : callback(NULL), error(CL_SUCCESS), error_msg(NULL), priv_info(NULL)  {}
    ~Baton() {
      if(error_msg) delete error_msg;
      if(priv_info) delete priv_info;
    }
};

namespace CLObjType {
enum CLObjType {
  clObjNone=0,
  Platform,
  Device,
  Context,
  CommandQueue,
  Kernel,
  Program,
  Sampler,
  Event,
  MemoryObject,
  Exception,
  MAX_WEBCL_TYPES
};
static const char* CLObjName[] = {
  "UNKNOWN",
  "Platform",
  "Device",
  "Context",
  "CommandQueue",
  "Kernel",
  "Program",
  "Sampler",
  "Event",
  "MemoryObject",
  "Exception",
};
}

class WebCLObject;
void registerCLObj(void *clid, WebCLObject* obj);
void unregisterCLObj(WebCLObject* obj);
WebCLObject* findCLObj(void* clid, CLObjType::CLObjType type);
void onExit();

class WebCLObject : public node::ObjectWrap {
public:
  inline CLObjType::CLObjType getType() const { return _type; }

  inline const char* getCLObjName() const {
    return _type<CLObjType::MAX_WEBCL_TYPES ? CLObjType::CLObjName[_type] : "\0";
  }

protected:
  WebCLObject() : _type(CLObjType::clObjNone)
  {}

  virtual ~WebCLObject() {
  #ifdef LOGGING
    printf("[~WebCLObject] %s %p is being destroyed\n",getCLObjName(),this);
  #endif
    Destructor();
    unregisterCLObj(this);
  }

public:
  virtual void Destructor() {
  #ifdef LOGGING
    printf("In WebCLObject::Destructor for %s\n",getCLObjName());
  #endif
  }

  virtual bool operator==(void *clid) { return false; }

protected:
  CLObjType::CLObjType _type;

private:
  DISABLE_COPY(WebCLObject)
};

} // namespace webcl

#include "exceptions.h"
#include "manager.h"

#endif
