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

enum ResEnum1 {
  case a
  case b
  case s(S)
}

enum ResEnum2 {
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

  let r_enum1_a = ResEnum1.a
  let r_enum1_a_opt : Optional<ResEnum1> = r_enum1_a

  let r_enum1_s = ResEnum1.s(s)
  let r_enum1_s_opt : Optional<ResEnum1> = r_enum1_s
   
  let r_enum2_a = ResEnum2.a
  let r_enum2_a_opt : Optional<ResEnum2> = r_enum2_a

  let r_enum2_t = ResEnum2.t(t)
  let r_enum2_t_opt : Optional<ResEnum2> = r_enum2_t

  let r_enum2_s = ResEnum2.s(s)
  let r_enum2_s_opt : Optional<ResEnum2> = r_enum2_s
   
  print(s.a) // break here
}

main()

