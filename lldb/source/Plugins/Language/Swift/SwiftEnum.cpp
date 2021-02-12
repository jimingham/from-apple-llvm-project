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

#include "ObjCRuntimeSyntheticProvider.h"

#include "Plugins/Language/Swift/SwiftEnum.h"
#include "Plugins/TypeSystem/Swift/SwiftASTContext.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"
#include "lldb/Target/SwiftLanguageRuntime.h"

using namespace lldb_private;
using namespace lldb;

// This a ValueObject subclass used to describe "enum case labels".  We need to
// be able to hand out a synthetic child for the case where a payload-less enum
// is the one chosen to make this case and the one where we hand out a payload
// as the children.

// However, SwiftEnumCase presents its value as a summary, since that's how
// the labels get printed when there are payloads.  So make a type summary just
// for these nodes first:

struct SwiftEnumCaseSummaryProvider : public TypeSummaryImpl {
public:
  static bool WouldEvenConsiderFormatting(CompilerType) { return true; }

  SwiftEnumCaseSummaryProvider(llvm::StringRef case_name)
      : TypeSummaryImpl(TypeSummaryImpl::Kind::eInternal,
                      TypeSummaryImpl::Flags()), m_case_name(case_name) {
  }

  virtual ~SwiftEnumCaseSummaryProvider() = default;

  bool FormatObject(ValueObject *valobj, std::string &dest,
                            const TypeSummaryOptions &options) override {
    dest = m_case_name;
    return true;
  }

  std::string GetDescription() override {
    return "Internal summary for presenting swift enum C-style case labels.";
  }

  virtual bool IsScripted() { return false; }

  bool DoesPrintChildren(ValueObject *valobj) const override {
    return false;
  }

private:
  SwiftEnumCaseSummaryProvider(const SwiftEnumCaseSummaryProvider &) = delete;
  const SwiftEnumCaseSummaryProvider &
  operator=(const SwiftEnumCaseSummaryProvider &) = delete;
  std::string m_case_name;

};

class ValueObjectSwiftEnumCase : public ValueObject {
public:
  static ValueObjectSP Create(ExecutionContextScope *exe_scope, 
                              std::string &case_name) {
    auto manager_sp = ValueObjectManager::Create();
    ValueObject *ret_ptr = new ValueObjectSwiftEnumCase(exe_scope, 
                                                      *manager_sp, case_name);
    std::shared_ptr<SwiftEnumCaseSummaryProvider> summary_impl_sp(new
        SwiftEnumCaseSummaryProvider(case_name));
    ret_ptr->SetSummaryFormat(summary_impl_sp);
    // The name isn't significant, but it helps when debugging.
    //ret_ptr->SetName(ConstString(case_name));
    return ret_ptr->GetSP();
  }
  
  virtual ~ValueObjectSwiftEnumCase() {
  }
  
  bool UpdateValue() override { return true; }
  
  size_t CalculateNumChildren(uint32_t max = UINT32_MAX) override { return 0; }
  CompilerType GetCompilerTypeImpl() override { 
      // How are these represented in the swift typesystem, is it worth 
      // figuring that out?
    return {};
  }
  llvm::Optional<uint64_t> GetByteSize() override { return m_case_name.size() + 1; }

  lldb::ValueType GetValueType() const override {
    return eValueTypeConstResult;
  }
  
  lldb::LanguageType GetObjectRuntimeLanguage() override {
    return eLanguageTypeSwift;
  }
  
  bool CanProvideValue() override { return true; };
  
  bool HasSyntheticValue() override { return false; }

private:
  ValueObjectSwiftEnumCase(ExecutionContextScope *exe_scope, 
                           ValueObjectManager &manager, 
                           llvm::StringRef case_name) :
      ValueObject(exe_scope, manager), m_case_name(case_name) {
    SetIsConstant();
    // Our value is the m_case_name, so we have to set that value:
    m_value.SetBytes(m_case_name.c_str(), m_case_name.size() + 1);
    SetValueIsValid(true);
  }
  
    std::string m_case_name;
};

// This is the synthetic child provider:

lldb_private::formatters::swift::EnumSyntheticFrontEnd::EnumSyntheticFrontEnd(
    lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp.get()), m_exe_ctx_ref(),
      m_element_name(), m_element_offset(LLDB_INVALID_ADDRESS),
      m_element_length(LLDB_INVALID_ADDRESS), m_is_optional(false),
      m_is_valid(false) {
  if (valobj_sp)
    Update();
}

size_t
lldb_private::formatters::swift::EnumSyntheticFrontEnd::CalculateNumChildren() {
  if (m_current_payload_sp)
    return m_current_payload_sp->GetNumChildren();
  if (m_current_case_sp)
    return 1;
  return 0;
}

lldb::ValueObjectSP 
lldb_private::formatters::swift::EnumSyntheticFrontEnd::GetSyntheticValue() {
  if (m_current_payload_sp) return m_current_payload_sp;
  if (m_current_case_sp) return m_current_case_sp;
  return {};
}

lldb::ValueObjectSP
lldb_private::formatters::swift::EnumSyntheticFrontEnd::GetChildAtIndex(
    size_t idx) {
  if (!m_is_valid)
    return {};
  else if (m_current_payload_sp)
    return m_current_payload_sp->GetChildAtIndex(idx, true);
  else if (m_current_case_sp) {
    if (idx == 0)
      return m_current_case_sp;
    else
      return {};
  }
  else
    return {};
}

bool lldb_private::formatters::swift::EnumSyntheticFrontEnd::Update() {
  m_element_name.clear();
  m_current_payload_sp.reset();
  m_current_case_sp.reset();
  m_is_valid = false;
  m_backend.SetSummaryFormat({});
  m_backend.SetSyntheticChildrenGenerated(true);
  
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
  
  // If we don't have a payload return a node that represents the selected
  // case:
  if (!enum_info.has_payload) {
    m_is_valid = true;
    // Don't produce an enum leaf node for empty optionals, since we don't
    // want to show a value of "nil", we just want to have no children and
    // let the summary take over.
    if (m_is_optional)
      return true;

    std::string case_name;
    case_name = ".";
    case_name.append(m_element_name);
    m_current_case_sp = ValueObjectSwiftEnumCase::Create(process_sp.get(),
                                                       case_name);
    m_current_case_sp->SetSyntheticChildrenGenerated(true);
    return true;
  }

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
  m_current_payload_sp 
      = CreateValueObjectFromData(m_element_name.c_str(), element_data, 
                                    m_backend.GetExecutionContextRef(), 
                                    m_element_type);
  if (!m_current_payload_sp) {
    m_is_valid = false;
    return false;
  }

  m_current_payload_sp->SetSyntheticChildrenGenerated(true);

  // Now insert our summary provider into the backend:
  std::shared_ptr<SwiftEnumCaseSummaryProvider> summary_impl_sp(new
        SwiftEnumCaseSummaryProvider(m_element_name));
  m_backend.SetSummaryFormat(summary_impl_sp);

  // One more step here.  If we are an optional with a value of "some"
  // and our current value is another enum with no payload, make the enum case
  // our value.
  if (m_is_optional && m_current_payload_sp) {
    runtime->GetCurrentEnumValue(*m_current_payload_sp.get(), enum_info, error);
    if (error.Success() && !enum_info.has_payload) {
      m_current_payload_sp.reset();
      std::string case_name;
      case_name = ".";
      case_name.append(enum_info.case_name);
      m_current_case_sp = ValueObjectSwiftEnumCase::Create(process_sp.get(),
                                                         case_name);
      m_current_case_sp->SetSyntheticChildrenGenerated(true);
      return true;
    }
  }

  // We want to hand out the best version of this ValueObject we can, so compute
  // that here.
  // FIXME: we have to force eDynamicDontRunTarget here or the ObjC bridge
  // formatters don't trigger correctly.
  m_current_payload_sp 
      = m_current_payload_sp
          ->GetQualifiedRepresentationIfAvailable(eDynamicDontRunTarget, true);
  
  return true;
}

bool lldb_private::formatters::swift::EnumSyntheticFrontEnd::
    MightHaveChildren() {
  if (!m_is_valid)
    return false;
  if (m_current_case_sp)
    return true;
  if (m_current_payload_sp)
    return m_current_payload_sp->MightHaveChildren();
  return false;
}

// FIXME: Is this right when we are treating the value of the enum as its payload?
size_t
lldb_private::formatters::swift::EnumSyntheticFrontEnd::GetIndexOfChildWithName(
    ConstString name) {
  if (m_current_payload_sp)
    return m_current_payload_sp->GetIndexOfChildWithName(name);
  if (m_current_case_sp)
    return m_current_case_sp->GetIndexOfChildWithName(name);
  return UINT32_MAX;
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
// YES, I make this in Update, so when I do that, I should be able to pass in
// everything I learned there.

static Status GetCurrentEnumValue(ValueObject &valobj, 
        SwiftLanguageRuntime::SwiftEnumValueInfo &enum_info) {
  ExecutionContextRef exe_ctx_ref = valobj.GetExecutionContextRef();
  ProcessSP process_sp = exe_ctx_ref.GetProcessSP();
  Status error;
  if (!process_sp) {
    error.SetErrorString("no process");
    return error;
  }
  SwiftLanguageRuntime *runtime = SwiftLanguageRuntime::Get(process_sp.get());
  if (!runtime) {
    error.SetErrorString("no swift runtime");
    return error;
  }
  
  runtime->GetCurrentEnumValue(valobj, enum_info, error);
  return error;
}

bool formatters::swift::SwiftEnumCXXSummaryFormat::DoesPrintValue(ValueObject *valobj) const  {
  if (!valobj)
    return false;
    
  SwiftLanguageRuntime::SwiftEnumValueInfo enum_info;
  Status error = GetCurrentEnumValue(*valobj, enum_info);
  if (!error.Success())
    return false;
  if (enum_info.is_optional)
    return false;
  else
    return true;
}

formatters::swift::SwiftEnumCXXSummaryFormat::SwiftEnumCXXSummaryFormat(
    const TypeSummaryImpl::Flags &flags) :
        CXXFunctionSummaryFormat(flags, 
            lldb_private::formatters::swift::SwiftEnum_SummaryProvider, 
            "Swift enum summary provider") {}

bool lldb_private::formatters::swift::SwiftEnum_SummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options) {  
  SwiftLanguageRuntime::SwiftEnumValueInfo enum_info;
  Status error = GetCurrentEnumValue(valobj, enum_info);
  if (!error.Success()) {
    stream.Printf("<could not fetch current value: %s>", error.AsCString());
    return false;
  }
  
  if (enum_info.is_optional) {
    if (enum_info.case_name =="none") {
      stream.PutCString("nil");
    }
  } else {
    // If we have a payload, then we will print the case name and the payload.
    // Otherwise the ValueObjectSwiftEnumCase will represent the non-payload
    // value.
    stream.PutCString(".");
    stream.PutCString(enum_info.case_name.c_str());
  }
  return true;
}


