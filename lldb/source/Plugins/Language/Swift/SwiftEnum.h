//===-- SwiftEnum.h ----------------------------------------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef liblldb_SwiftEnum_h_
#define liblldb_SwiftEnum_h_

#include "Plugins/TypeSystem/Swift/TypeSystemSwift.h"
#include "Plugins/TypeSystem/Swift/TypeSystemSwiftTypeRef.h"
#include "lldb/Core/SwiftForward.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"

#include "lldb/lldb-public.h"

namespace lldb_private {
namespace formatters {
namespace swift {
bool SwiftEnum_SummaryProvider(
    ValueObject &valobj, Stream &stream, const TypeSummaryOptions &options);

class EnumSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  EnumSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp);

  virtual size_t CalculateNumChildren();

  virtual lldb::ValueObjectSP GetChildAtIndex(size_t idx);

  virtual bool Update();

  virtual bool MightHaveChildren();

  virtual size_t GetIndexOfChildWithName(ConstString name);
  
  virtual lldb::ValueObjectSP GetSyntheticValue();

  virtual ~EnumSyntheticFrontEnd() = default;

private:
  ExecutionContextRef m_exe_ctx_ref;
  CompilerType m_element_type;
  std::string m_element_name;
  lldb::addr_t m_element_offset;
  lldb::addr_t m_element_length;
  bool m_is_optional;
  bool m_is_none;
  bool m_has_payload;
  bool m_is_valid;
  lldb::ValueObjectSP m_current_value_sp;
  size_t m_child_index; // FIXME: Do I actually need this?
};

SyntheticChildrenFrontEnd *EnumSyntheticFrontEndCreator(CXXSyntheticChildren *,
                                                        lldb::ValueObjectSP);

}
}
} // namespace lldb_private

#endif // liblldb_SwiftEnum_h_
