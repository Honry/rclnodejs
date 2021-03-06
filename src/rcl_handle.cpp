// Copyright (c) 2017 Intel Corporation. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rcl_handle.hpp"

#include <rcl/rcl.h>

#include <sstream>
#include <string>

namespace rclnodejs {

Nan::Persistent<v8::Function> RclHandle::constructor;

// TODO(Kenny): attach publisher/subscription/service/client to node handle
// When node handle is destroyed, make sure the attached ones are destroyed

RclHandle::RclHandle()
    : pointer_(nullptr), type_(RclHandleType_None), other_(nullptr) {}

RclHandle::~RclHandle() {
  DestroyMe();
}

void RclHandle::DestroyMe() {
  if (pointer_) {
    rcl_ret_t ret = RCL_RET_OK;

    switch (type_) {
      case RclHandleType_None:
        break;
      case RclHandleType_ROSNode:
        ret = rcl_node_fini(reinterpret_cast<rcl_node_t*>(pointer_));
        free(pointer_);
        break;
      case RclHandleType_ROSPublisher:
        if (other_) {
          auto publisher = reinterpret_cast<rcl_publisher_t*>(pointer_);
          auto node = reinterpret_cast<rcl_node_t*>(other_);
          ret = rcl_publisher_fini(publisher, node);
        }
        free(pointer_);
        break;
      case RclHandleType_ROSSubscription:
        if (other_) {
          auto subscription = reinterpret_cast<rcl_subscription_t*>(pointer_);
          auto node = reinterpret_cast<rcl_node_t*>(other_);
          ret = rcl_subscription_fini(subscription, node);
        }
        free(pointer_);
        break;
      case RclHandleType_ROSService:
        if (other_) {
          auto service = reinterpret_cast<rcl_service_t*>(pointer_);
          auto node = reinterpret_cast<rcl_node_t*>(other_);
          ret = rcl_service_fini(service, node);
        }
        free(pointer_);
        break;
      case RclHandleType_ROSClient:
        if (other_) {
          auto client = reinterpret_cast<rcl_client_t*>(pointer_);
          auto node = reinterpret_cast<rcl_node_t*>(other_);
          ret = rcl_client_fini(client, node);
        }
        free(pointer_);
        break;
      case RclHandleType_Timer:
        ret = rcl_timer_fini(reinterpret_cast<rcl_timer_t*>(pointer_));
        free(pointer_);
        break;
      case RclHandleType_ROSIDLString:
        if (pointer_) {
          free(pointer_);
        }
        break;
      case RclHandleType_Malloc:
        if (pointer_) {
          free(pointer_);
        }
        break;
      case RclHandleType_Count:  // No need to do anything
        break;
    }
  }
  // TODO(Kenny): log pointer_, other_, type_ and ret
  pointer_ = nullptr;
  type_ = RclHandleType_None;
  other_ = nullptr;
}

void RclHandle::Init(v8::Local<v8::Object> exports) {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("RclHandle").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "destroy", Destroy);
  Nan::SetPrototypeMethod(tpl, "dismiss", Dismiss);

  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("handle").ToLocalChecked(),
                   HandleGetter, nullptr, v8::Local<v8::Value>(), v8::DEFAULT,
                   static_cast<v8::PropertyAttribute>(v8::ReadOnly));

  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("type").ToLocalChecked(),
                   TypeGetter, nullptr, v8::Local<v8::Value>(), v8::DEFAULT,
                   static_cast<v8::PropertyAttribute>(v8::ReadOnly));

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("RclHandle").ToLocalChecked(), tpl->GetFunction());
}

void RclHandle::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (info.IsConstructCall()) {
    RclHandle* obj = new RclHandle();
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }
}

NAN_METHOD(RclHandle::Destroy) {
  auto me = Nan::ObjectWrap::Unwrap<RclHandle>(info.Holder());
  if (me) {
    me->DestroyMe();
  }
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(RclHandle::Dismiss) {
  auto me = Nan::ObjectWrap::Unwrap<RclHandle>(info.Holder());
  if (me) {
    me->SetPtr(nullptr);
    me->SetType(RclHandleType_None);
  }
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_GETTER(RclHandle::HandleGetter) {
  auto me = ObjectWrap::Unwrap<RclHandle>(info.Holder());
  std::stringstream ss;
  ss << std::hex << "0x" << me->GetPtr();
  info.GetReturnValue().Set(Nan::New(ss.str().c_str()).ToLocalChecked());
}

NAN_GETTER(RclHandle::TypeGetter) {
  auto me = ObjectWrap::Unwrap<RclHandle>(info.Holder());
  std::string str;
  switch (me->GetType()) {
    case RclHandleType_None:
    case RclHandleType_Count:
      str = "Unknown";
      break;
    case RclHandleType_ROSNode:
      str = "ROS Node";
      break;
    case RclHandleType_ROSPublisher:
      str = "ROS Publisher";
      break;
    case RclHandleType_ROSSubscription:
      str = "ROS Subscription";
      break;
    case RclHandleType_ROSService:
      str = "ROS Service";
      break;
    case RclHandleType_ROSClient:
      str = "ROS Client";
      break;
    case RclHandleType_Timer:
      str = "ROS Timer";
      break;
    case RclHandleType_ROSIDLString:
      str = "ROS String";
      break;
    case RclHandleType_Malloc:
      str = "Memory";
      break;
  }
  info.GetReturnValue().Set(Nan::New(str.c_str()).ToLocalChecked());
}

v8::Local<v8::Object> RclHandle::NewInstance(void* handle,
                                             RclHandleType type,
                                             void* other) {
  Nan::EscapableHandleScope scope;

  v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
  v8::Local<v8::Context> context =
      v8::Isolate::GetCurrent()->GetCurrentContext();

  v8::Local<v8::Object> instance =
      cons->NewInstance(context, 0, nullptr).ToLocalChecked();

  auto wrapper = Nan::ObjectWrap::Unwrap<RclHandle>(instance);
  wrapper->SetPtr(handle);
  wrapper->SetType(type);
  wrapper->SetOther(other);

  return scope.Escape(instance);
}

}  // namespace rclnodejs
