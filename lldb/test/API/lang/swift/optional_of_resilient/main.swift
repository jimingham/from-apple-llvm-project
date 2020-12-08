// main.swift
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
// -----------------------------------------------------------------------------
import mod

struct T {
  let a = 2
}

enum ResEnum {
  case a
  case b
  case t(T)
  case s(S)
}

func main() {
  initGlobal()
  let s = S()
  let s_opt : Optional<S> = s
  let s_nil_opt : Optional<S> = nil

  let t = T()
  let t_opt : Optional<T> = t
  let t_nil_opt : Optional<T> = nil

  let r_enum_a = ResEnum.a
  let r_enum_a_opt : Optional<ResEnum> = r_enum_a

  let r_enum_t = ResEnum.t(t)
  let r_enum_t_opt : Optional<ResEnum> = r_enum_t

  let r_enum_s = ResEnum.s(s)
  let r_enum_s_opt : Optional<ResEnum> = r_enum_s
   
  print(s.a) // break here
}

main()

