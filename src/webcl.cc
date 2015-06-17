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

#include "webcl.h"
#include "platform.h"
#include "context.h"
#include "device.h"
#include "event.h"
#include "commandqueue.h"

#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>

using namespace v8;
using namespace node;
using namespace std;

namespace webcl {

static bool atExit=false;
void registerCLObj(void *clid, WebCLObject* obj) {
  if(!clid || !obj) return;
  Manager::instance()->add(&obj->persistent(), clid);
}

void unregisterCLObj(WebCLObject *obj) {
  if(atExit || !obj) return;
  Manager::instance()->remove(&obj->persistent());
}

WebCLObject* findCLObj(void *clid, CLObjType::CLObjType type) {
  if(!clid) return nullptr;
  Persistent<Object>* p=Manager::instance()->find(clid);

  return p ? ObjectWrap::Unwrap<WebCLObject>(NanNew(*p)) : nullptr;
}

void onExit() {
#ifdef LOGGING
  printf("**** onExit %d ****\n",atExit);
#endif

  Manager::instance()->stats();
  Manager::instance()->clear();

#ifdef LOGGING
  printf("**** GC running ****\n");
#endif
  while(!NanIdleNotification(16)); // force GC
}

NAN_METHOD(getPlatforms) {
  NanScope();

  cl_uint num_entries = 0;
  cl_int ret = ::clGetPlatformIDs(0, NULL, &num_entries);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  cl_platform_id* platforms=new cl_platform_id[num_entries];
  ret = ::clGetPlatformIDs(num_entries, platforms, NULL);
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }


  Local<Array> platformArray = NanNew<Array>(num_entries);
  for (uint32_t i=0; i<num_entries; i++) {
    platformArray->Set(i, NanObjectWrapHandle(Platform::New(platforms[i])));
  }

  delete[] platforms;

  NanReturnValue(platformArray);
}

NAN_METHOD(releaseAll) {
  NanScope();
  atExit = args[0]->IntegerValue();
  onExit();

  NanReturnUndefined();
}

NAN_METHOD(createContext) {
  NanScope();
  cl_int ret=CL_SUCCESS;
  cl_context cw=NULL;
  vector<cl_device_id> devices;
  vector<cl_context_properties> properties;
  Local<Object> glRenderingContext;

  // Case 1: WebCLContext createContext(optional CLenum deviceType = WebCL.DEVICE_TYPE_DEFAULT);
  if(args[0]->IsUndefined() || args[0]->IsNumber()) {
    // we must use the default platform
    cl_uint numPlatforms=0; //the NO. of platforms
    cl_int  status = ::clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS)
    {
      NanThrowError("Can NOT get an OpenCL platform!");
    }

    // For simplicity, choose the first available platform.
    if (numPlatforms > 0)
    {
      cl_platform_id* platforms = new cl_platform_id[numPlatforms];
      status = clGetPlatformIDs(numPlatforms, platforms, NULL);
      properties.push_back(CL_CONTEXT_PLATFORM);
      properties.push_back((cl_context_properties) platforms[0]);

      delete[] platforms;
    }

    // terminate properties array
    if(properties.size()) properties.push_back(0);

    cl_uint device_type=(args[0]->IsUndefined() ? CL_DEVICE_TYPE_DEFAULT : args[0]->Uint32Value());
    cw = ::clCreateContextFromType(properties.size() ? &properties.front() : NULL,
            device_type,
            NULL, NULL, // no callback
            &ret);
  }
  else if(args[0]->IsObject()) {
    Local<Object> obj=args[0]->ToObject();
    Local<String> GLname = NanNew<String>("WebGLTexture"); // any WebGL class will do

    if(!args[0]->IsArray()) {
      Local<String> name=obj->GetConstructorName();
      NanAsciiString astr(name);
      // printf("Found object type: %s\n",*astr);

      if(!strcmp(*astr,"WebCLPlatform")) {
        // Case 2: WebCLContext createContext(WebCLPlatform platform, optional CLenum deviceType = WebCL.DEVICE_TYPE_DEFAULT);
        Platform *platform=ObjectWrap::Unwrap<Platform>(obj);
        properties.push_back(CL_CONTEXT_PLATFORM);
        properties.push_back((cl_context_properties) platform->getPlatformId());
        properties.push_back(0);

        cw = ::clCreateContextFromType(&properties.front(),
                                      (args[1]->IsUndefined() ? CL_DEVICE_TYPE_DEFAULT : args[1]->Uint32Value()),
                                      NULL, NULL, // no callback
                                      &ret);

      }
      else if(!strcmp(*astr,"WebCLDevice")) {
        // Case 3: WebCLContext createContext(WebCLDevice device);
        Device *d = ObjectWrap::Unwrap<Device>(obj);
        cl_device_id device=d->getDevice();

        cl_platform_id platform;
        ::clGetDeviceInfo(device,CL_DEVICE_PLATFORM,sizeof(cl_platform_id),&platform,NULL);

        properties.push_back(CL_CONTEXT_PLATFORM);
        properties.push_back((cl_context_properties) platform);

        // check if device has gl_sharing extension enabled and update properties accordingly
        if(d->hasGLSharingEnabled()) {
          // printf("Device has GL sharing enabled\n");

#if defined (__APPLE__)
          CGLContextObj kCGLContext = CGLGetCurrentContext();
          CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
          properties.push_back(CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE);
          properties.push_back((cl_context_properties) kCGLShareGroup);
#else
  #ifdef _WIN32
          properties.push_back(CL_GL_CONTEXT_KHR);
          properties.push_back((cl_context_properties) wglGetCurrentContext());
          properties.push_back(CL_WGL_HDC_KHR);
          properties.push_back((cl_context_properties) wglGetCurrentDC());
  #else // Unix
          properties.push_back(CL_GL_CONTEXT_KHR);
          properties.push_back((cl_context_properties) glXGetCurrentContext());
          properties.push_back(CL_GLX_DISPLAY_KHR);
          properties.push_back((cl_context_properties) glXGetCurrentDisplay());
  #endif
#endif
        }

        properties.push_back(0);

        cw = ::clCreateContext(&properties.front(),
                               1, &device,
                               NULL, NULL, // no callback
                               &ret);
      }
      else if(!strcmp(*astr,"Object") && obj->HasOwnProperty(GLname)) { // Case 5: for CL-GL extension
        // 5.1 WebCLContext createContext(WebGLRenderingContext gl, optional CLenum deviceType);
        // 5.2 WebCLContext createContext(WebGLRenderingContext gl, WebCLPlatform platform, optional CLenum deviceType);
        // 5.3 WebCLContext createContext(WebGLRenderingContext gl, WebCLDevice device);
        // 5.4 WebCLContext createContext(WebGLRenderingContext gl, sequence<WebCLDevice> devices);

#if defined (__APPLE__)
        CGLContextObj kCGLContext = CGLGetCurrentContext();
        CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
        properties.push_back(CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE);
        properties.push_back((cl_context_properties) kCGLShareGroup);
#else
  #ifdef _WIN32
        properties.push_back(CL_GL_CONTEXT_KHR);
        properties.push_back((cl_context_properties) wglGetCurrentContext());
        properties.push_back(CL_WGL_HDC_KHR);
        properties.push_back((cl_context_properties) wglGetCurrentDC());
  #else // Unix
        properties.push_back(CL_GL_CONTEXT_KHR);
        properties.push_back((cl_context_properties) glXGetCurrentContext());
        properties.push_back(CL_GLX_DISPLAY_KHR);
        properties.push_back((cl_context_properties) glXGetCurrentDisplay());
  #endif
#endif

        if(args[1]->IsUndefined() || args[1]->IsNumber()) {
          // case 5.1
          cl_uint numPlatforms=0; //the NO. of platforms
          cl_int  status = ::clGetPlatformIDs(0, NULL, &numPlatforms);
          if (status != CL_SUCCESS)
            return NanThrowError("Can NOT get an OpenCL platform!");

          // For simplicity, choose the first available platform.
          if (numPlatforms > 0)
          {
            cl_platform_id* platforms = new cl_platform_id[numPlatforms];
            status = clGetPlatformIDs(numPlatforms, platforms, NULL);
            properties.push_back(CL_CONTEXT_PLATFORM);
            properties.push_back((cl_context_properties) platforms[0]);

            delete[] platforms;
          }

          // [MBS] what if CL device doesn't provide CLGL?

          cl_uint deviceType=(args[1]->IsUndefined() ? CL_DEVICE_TYPE_DEFAULT : args[1]->Uint32Value());
          if(deviceType!=CL_DEVICE_TYPE_ALL && deviceType>CL_DEVICE_TYPE_CUSTOM) {
            cl_int ret=CL_INVALID_DEVICE_TYPE;
            REQ_ERROR_THROW(INVALID_DEVICE_TYPE);
            NanReturnNull();
          }

          // terminate properties array
          properties.push_back(0);

          cw = ::clCreateContextFromType(properties.size() ? &properties.front() : NULL,
                                          deviceType,
                                          NULL, NULL, // no callback
                                          &ret);
        }
        else {
          Local<Object> obj=args[1]->ToObject();

          if(!args[1]->IsArray()) {
            Local<String> name=obj->GetConstructorName();
            NanAsciiString astr(name);

            if(!strcmp(*astr,"WebCLPlatform")) {
              // case 5.2: WebCLContext createContext(WebGLRenderingContext gl, WebCLPlatform platform, optional CLenum deviceType);
              cl_uint deviceType=(args[2]->IsUndefined() ? CL_DEVICE_TYPE_DEFAULT : args[2]->Uint32Value());
              if(deviceType!=CL_DEVICE_TYPE_ALL && deviceType>CL_DEVICE_TYPE_CUSTOM) {
                cl_int ret=CL_INVALID_DEVICE_TYPE;
                REQ_ERROR_THROW(INVALID_DEVICE_TYPE);
                NanReturnNull();
              }

              Platform *platform=ObjectWrap::Unwrap<Platform>(obj);
              properties.push_back(CL_CONTEXT_PLATFORM);
              properties.push_back((cl_context_properties) platform->getPlatformId());
              properties.push_back(0);

              cw = ::clCreateContextFromType(&properties.front(),
                                            deviceType,
                                            NULL, NULL, // no callback
                                            &ret);

            }
            else if(!strcmp(*astr,"WebCLDevice")) {
              // case 5.3: WebCLContext createContext(WebGLRenderingContext gl, WebCLDevice device);
              Device *d = ObjectWrap::Unwrap<Device>(obj);
              cl_device_id device=d->getDevice();

              cl_platform_id platform;
              ::clGetDeviceInfo(device,CL_DEVICE_PLATFORM,sizeof(cl_platform_id),&platform,NULL);

              properties.push_back(CL_CONTEXT_PLATFORM);
              properties.push_back((cl_context_properties) platform);

              properties.push_back(0);

              cw = ::clCreateContext(&properties.front(),
                                     1, &device,
                                     NULL, NULL, // no callback
                                     &ret);
     // String::AsciiValue str(args[0]->ToObject()->GetConstructorName());
     // printf("GL object: %s hasGLTexture %d\n",*str,args[0]->ToObject()->HasOwnProperty(JS_STR("WebGLTexture")));
              glRenderingContext=Local<Object>(args[0]->ToObject());
            }
            else {
              ret=CL_INVALID_VALUE;
              REQ_ERROR_THROW(INVALID_VALUE);
              NanReturnNull();
            }
          }
          else {
            // 5.4 WebCLContext createContext(WebGLRenderingContext gl, sequence<WebCLDevice> devices);
            Local<Array> deviceArray = Local<Array>::Cast(obj);
            if(deviceArray->Length()==0) {
              ret=CL_INVALID_VALUE;
              REQ_ERROR_THROW(INVALID_VALUE);
              NanReturnNull();
            }

            for (uint32_t i=0; i<deviceArray->Length(); i++) {
              Local<Object> obj = deviceArray->Get(i)->ToObject();
              Device *d = ObjectWrap::Unwrap<Device>(obj);
              #ifdef LOGGING
                cout<<"adding device "<<hex<<d->getDevice()<<dec<<endl;
              #endif
              cl_device_id device=d->getDevice();
              devices.push_back(device);
            }

            // assume all devices are on the same platform
            cl_platform_id platform;
            ::clGetDeviceInfo(devices[0],CL_DEVICE_PLATFORM,sizeof(cl_platform_id),&platform,NULL);
            properties.push_back(CL_CONTEXT_PLATFORM);
            properties.push_back((cl_context_properties) platform);
            properties.push_back(0);

            cw = ::clCreateContext(&properties.front(),
                                  (int) devices.size(), &devices.front(),
                                   NULL, NULL, // no callback
                                   &ret);

          }
        }
      }
      else {
        ret=CL_INVALID_VALUE;
        REQ_ERROR_THROW(INVALID_VALUE);
        NanReturnNull();
      }
    }
    else {
      // Case 4: WebCLContext createContext(sequence<WebCLDevice> devices);
      Local<Array> deviceArray = Local<Array>::Cast(obj);
      if(deviceArray->Length()==0) {
        ret=CL_INVALID_VALUE;
        REQ_ERROR_THROW(INVALID_VALUE);
        NanReturnNull();
      }
      for (uint32_t i=0; i<deviceArray->Length(); i++) {
        Local<Object> obj = deviceArray->Get(i)->ToObject();
        Device *d = ObjectWrap::Unwrap<Device>(obj);
        #ifdef LOGGING
        cout<<"adding device "<<hex<<d->getDevice()<<dec<<endl;
        #endif
        cl_device_id device=d->getDevice();
        devices.push_back(device);
      }

      // assume all devices are on the same platform
      cl_platform_id platform;
      ::clGetDeviceInfo(devices[0],CL_DEVICE_PLATFORM,sizeof(cl_platform_id),&platform,NULL);
      properties.push_back(CL_CONTEXT_PLATFORM);
      properties.push_back((cl_context_properties) platform);
      properties.push_back(0);

      cw = ::clCreateContext(&properties.front(),
                            (int) devices.size(), &devices.front(),
                             NULL, NULL, // no callback
                             &ret);
    }
  }

  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_PLATFORM);
    REQ_ERROR_THROW(INVALID_PROPERTY);
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_DEVICE);
    REQ_ERROR_THROW(INVALID_OPERATION);
    REQ_ERROR_THROW(DEVICE_NOT_AVAILABLE);
    REQ_ERROR_THROW(DEVICE_NOT_FOUND);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    REQ_ERROR_THROW(INVALID_GL_SHAREGROUP_REFERENCE_KHR);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnValue(NanObjectWrapHandle(Context::New(cw, glRenderingContext)));
}

class WaitForEventsWorker : public NanAsyncWorker {
 public:
  WaitForEventsWorker(Baton *baton)
    : NanAsyncWorker(baton->callback), baton_(baton) {
    }

  ~WaitForEventsWorker() {
    if(baton_) {
      NanScope();
      if (!baton_->parent.IsEmpty()) NanDisposePersistent(baton_->parent);
      if (!baton_->data.IsEmpty()) NanDisposePersistent(baton_->data);
      // if (baton_->callback) delete baton_->callback;
      delete baton_;
    }
  }

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    // SetErrorMessage("Error");
    // printf("[async event] execute\n");
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    NanScope();

    Local<Array> eventsArray = Local<Array>::Cast(NanNew(baton_->data));
    std::vector<cl_event> events;
    for (uint32_t i=0; i<eventsArray->Length(); i++) {
     Event *we=ObjectWrap::Unwrap<Event>(eventsArray->Get(i)->ToObject());
      cl_event e = we->getEvent();
      events.push_back(e);
    }
    baton_->error = ::clWaitForEvents( (int) events.size(), &events.front());

    // must return passed data
    Local<Value> argv[] = {
      JS_INT(baton_->error)
    };

    // printf("[async event] callback JS\n");
    callback->Call(1, argv);
  }

  private:
    Baton *baton_;
};

NAN_METHOD(waitForEvents) {
  NanScope();

  if (!args[0]->IsArray())
    NanThrowError("INVALID_VALUE");

  if(args[1]->IsFunction()) {
      Baton *baton=new Baton();
      NanAssignPersistent(baton->data, args[0]);
      baton->callback=new NanCallback(args[1].As<Function>());
      NanAsyncQueueWorker(new WaitForEventsWorker(baton));
      NanReturnUndefined();
  }

  Local<Array> eventsArray = Local<Array>::Cast(args[0]);
  std::vector<cl_event> events;
  for (uint32_t i=0; i<eventsArray->Length(); i++) {
    Event *we=ObjectWrap::Unwrap<Event>(eventsArray->Get(i)->ToObject());
    cl_event e = we->getEvent();
    events.push_back(e);
  }

  // printf("Waiting for %d events\n",events.size());
  // for(int i=0;i<events.size();i++)
    // printf("   %p\n",events[i]);
  cl_int ret=::clWaitForEvents( (int) events.size(), &events.front());
  if (ret != CL_SUCCESS) {
    REQ_ERROR_THROW(INVALID_VALUE);
    REQ_ERROR_THROW(INVALID_CONTEXT);
    REQ_ERROR_THROW(INVALID_EVENT);
    REQ_ERROR_THROW(EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
    REQ_ERROR_THROW(OUT_OF_RESOURCES);
    REQ_ERROR_THROW(OUT_OF_HOST_MEMORY);
    return NanThrowError("UNKNOWN ERROR");
  }

  NanReturnUndefined();
}



}
