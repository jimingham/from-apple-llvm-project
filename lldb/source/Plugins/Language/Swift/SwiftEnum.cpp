//===-- SwiftEnum.cpp
//------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/AST/Decl.h"
#include "swift/AST/Type.h"
#include "swift/AST/Types.h"
#include "swift/Basic/ClusteredBitVector.h"

#include "swift/../../lib/IRGen/FixedTypeInfo.h"
#include "swift/../../lib/IRGen/GenEnum.h"
#include "swift/../../lib/IRGen/GenHeap.h"
#include "swift/../../lib/IRGen/IRGenModule.h"
#include "swift/../../lib/IRGen/TypeInfo.h"

#include "Plugins/Language/Swift/SwiftEnum.h"
#include "Plugins/TypeSystem/Swift/SwiftASTContext.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/Target/SwiftLanguageRuntime.h"

using namespace lldb_private;
using namespace lldb;

// This is the synthetic child provider:

lldb_private::formatters::swift::EnumSyntheticFrontEnd::EnumSyntheticFrontEnd(
    lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp.get()), m_exe_ctx_ref(),
      m_element_name(), m_element_offset(LLDB_INVALID_ADDRESS),
      m_element_length(LLDB_INVALID_ADDRESS), m_is_optional(false),
      m_has_payload(false), m_is_valid(false) {
  if (valobj_sp)
    Update();
}

size_t
lldb_private::formatters::swift::EnumSyntheticFrontEnd::CalculateNumChildren() {
  if (m_current_value_sp)
    return m_current_value_sp->GetNumChildren();
  return 0;
}

lldb::ValueObjectSP 
lldb_private::formatters::swift::EnumSyntheticFrontEnd::GetSyntheticValue() {
  return {};
}

lldb::ValueObjectSP
lldb_private::formatters::swift::EnumSyntheticFrontEnd::GetChildAtIndex(
    size_t idx) {
  if (!m_is_valid || !m_has_payload || !m_current_value_sp)
    return {};
  else
    return m_current_value_sp->GetChildAtIndex(idx, true);
}

bool lldb_private::formatters::swift::EnumSyntheticFrontEnd::Update() {
  m_element_name.clear();
  m_current_value_sp.reset();
  m_is_valid = false;
  
  m_exe_ctx_ref = m_backend.GetExecutionContextRef();
  ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
  if (!process_sp) {
    m_element_name = "<unknown:no process>";
    return false;
  }
  SwiftLanguageRuntime *runtime = SwiftLanguageRuntime::Get(process_sp.get());
  if (!runtime) {
    m_element_name = "<unknown: no runtime>";
    return false;
  }
  Status error;
  struct SwiftLanguageRuntime::SwiftEnumValueInfo enum_info;
  m_is_valid = runtime->GetCurrentEnumValue(m_backend, enum_info,
                                              error);
  if (!m_is_valid)
    return false;

  m_element_type = enum_info.case_type;
  m_element_name = std::move(enum_info.case_name);
  m_element_offset = enum_info.case_offset;
  m_element_length = enum_info.case_length;
  m_is_optional = enum_info.is_optional;
  m_has_payload = enum_info.has_payload;

  // If we don't have a payload we can't create a value, so just return.
  if (!m_has_payload)
    return true;

  DataExtractor backend_data;
  uint64_t bytes = m_backend.GetData(backend_data, error);
  if (!error.Success()) {
    // FIXME: Propagate error
    return false;
  }
  if (bytes < m_element_length) {
    return false;
  }

  DataExtractor element_data(backend_data, m_element_offset, m_element_length);
  m_current_value_sp 
      = CreateValueObjectFromData(m_element_name.c_str(), element_data, 
                                  m_backend.GetExecutionContextRef(), 
                                  m_element_type);
  
  if (!m_current_value_sp) {
    m_is_valid = false;
    return false;
  }

  return true;
}

bool lldb_private::formatters::swift::EnumSyntheticFrontEnd::
    MightHaveChildren() {
  if (!m_is_valid || !m_has_payload || !m_current_value_sp)
    return false;
  else
    return m_current_value_sp->MightHaveChildren();
}

// FIXME: Is this right when we are treating the value of the enum as its payload?
size_t
lldb_private::formatters::swift::EnumSyntheticFrontEnd::GetIndexOfChildWithName(
    ConstString name) {
  if (!m_has_payload || !m_current_value_sp)
    return UINT32_MAX;
  return m_current_value_sp->GetIndexOfChildWithName(name);
}

SyntheticChildrenFrontEnd *
lldb_private::formatters::swift::EnumSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp)
    return NULL;
  return (new EnumSyntheticFrontEnd(valobj_sp));
}

// This is the summary provider:
// FIXME: Is there any way to not have to compute the current value twice?

bool lldb_private::formatters::swift::SwiftEnum_SummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {
  ConstString elem_name;
  
  ExecutionContextRef exe_ctx_ref = valobj.GetExecutionContextRef();
  ProcessSP process_sp = exe_ctx_ref.GetProcessSP();
  if (!process_sp) {
    stream.PutCString("<unknown:no process>");
    return false;
  }
  SwiftLanguageRuntime *runtime = SwiftLanguageRuntime::Get(process_sp.get());
  if (!runtime) {
    stream.PutCString("<unknown: no runtime>");
    return false;
  }
  Status error;
  
  SwiftLanguageRuntime::SwiftEnumValueInfo enum_info;
  bool success = runtime->GetCurrentEnumValue(valobj, enum_info,
                                              error);
  if (!success) {
    stream.Printf("<could not fetch current value: %s>", error.AsCString());
    return true; // We
  }
  if (enum_info.is_optional) {
    if (enum_info.case_name =="none") {
      stream.PutCString("nil");
    }
  } else
    stream.PutCString(enum_info.case_name.c_str());
  
  return true;
  
}


