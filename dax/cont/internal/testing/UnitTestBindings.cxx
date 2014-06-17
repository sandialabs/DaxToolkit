//=============================================================================
//
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2012 Sandia Corporation.
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//=============================================================================
#include <dax/cont/internal/Bindings.h>

#include <dax/cont/arg/Field.h>
#include <dax/cont/arg/FieldConstant.h>
#include <dax/cont/sig/Tag.h>
#include <dax/cont/testing/Testing.h>

#include <dax/exec/internal/WorkletBase.h>

namespace {

#ifndef FieldIn
# define FieldIn dax::cont::arg::Field(*)(In)
# define FieldOut dax::cont::arg::Field(*)(Out)
#endif

struct Worklet1 : public dax::exec::internal::WorkletBase
{
  typedef void ControlSignature(FieldIn);
};

struct Worklet2 : public dax::exec::internal::WorkletBase
{
  typedef void ControlSignature(FieldIn,FieldIn);
};

struct Worklet3 : public dax::exec::internal::WorkletBase
{
  typedef void ControlSignature(FieldIn,FieldIn,FieldOut);
};

template<typename BindingType>
void TestWorklet1Binding(const BindingType &b1)
{
  DAX_TEST_ASSERT((b1.template Get<1>().GetExecArg()(0,Worklet1()) == 1.0f),
                  "ExecArg value incorrect!");
}

template<typename BindingType>
void TestWorklet2Binding(const BindingType &b2)
{
  DAX_TEST_ASSERT((b2.template Get<1>().GetExecArg()(0,Worklet2()) == 1.0f),
                  "ExecArg value incorrect!");
  DAX_TEST_ASSERT((b2.template Get<2>().GetExecArg()(0,Worklet2()) == 2.0f),
                  "ExecArg value incorrect!");
}

template<typename BindingType>
void TestWorklet3Binding(const BindingType &b3)
{
  DAX_TEST_ASSERT((b3.template Get<1>().GetExecArg()(0, Worklet2()) == 1.0f),
    "ExecArg value incorrect!");
  DAX_TEST_ASSERT((b3.template Get<2>().GetExecArg()(0, Worklet2()) == 2.0f),
    "ExecArg value incorrect!");
  DAX_TEST_ASSERT((b3.template Get<2>().GetExecArg()(0, Worklet2()) == 3.0f),
    "ExecArg value incorrect!");
}

void Bindings()
{
  TestWorklet1Binding(
        dax::cont::internal::BindingsCreate(
          Worklet1(),
          dax::internal::make_ParameterPack(1.0f)));
  TestWorklet2Binding(
        dax::cont::internal::BindingsCreate(
          Worklet2(),
          dax::internal::make_ParameterPack(1.0f, 2.0f)));
  TestWorklet3Binding(
      dax::cont::internal::BindingsCreate(
         Worklet3(),
     dax::internal::make_ParameterPack(1.0f, 2.0f, 3.0f)));
}

} // anonymous namespace

int UnitTestBindings(int, char *[])
{
  return dax::cont::testing::Testing::Run(Bindings);
}
