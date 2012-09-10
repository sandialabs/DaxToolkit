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
#ifndef __dax_exec_arg_BindDirect_h
#define __dax_exec_arg_BindDirect_h
#if defined(DAX_DOXYGEN_ONLY)

#else // !defined(DAX_DOXYGEN_ONLY)

#include <dax/Types.h>
#include <dax/cont/internal/Bindings.h>
#include <boost/utility/enable_if.hpp>

namespace dax { namespace exec { namespace arg {

template <typename Invocation, int N>
struct BindDirect
{
  typedef dax::cont::internal::Bindings<Invocation> AllControlBindings;
  typedef typename AllControlBindings::template GetType<N>::type MyControlBinding;
  typedef typename MyControlBinding::ExecArg ExecArgType;
  typedef typename dax::cont::arg::ConceptMapTraits<MyControlBinding>::Tags Tags;
  ExecArgType ExecArg;

  typedef typename ExecArgType::ReturnType ReturnType;

  BindDirect(AllControlBindings& bindings):
    ExecArg(bindings.template Get<N>().GetExecArg()) {}

  template<typename Worklet>
  DAX_EXEC_EXPORT ReturnType operator()(dax::Id id, const Worklet& worklet)
    {
    return this->ExecArg(id, worklet);
    }

  template<typename Worklet>
  DAX_EXEC_EXPORT
  void SaveExecutionResult(int id, const Worklet& worklet)
    {
    //Look at the concept map traits. If we have the Out tag
    //we know that we must call our ExecArgs SaveExecutionResult.
    //Otherwise we are an input argument and that behavior is undefined
    //and very bad things could happen
    typedef typename Tags::
          template Has<typename dax::cont::sig::Out>::type HasOutTag;
    this->saveResult(id,worklet,HasOutTag());
    }

  //method enabled when we do have the out tag ( or InOut)
  template <typename Worklet, typename HasOutTag>
  DAX_EXEC_EXPORT
  void saveResult(int id, const Worklet& worklet, HasOutTag,
     typename boost::enable_if<HasOutTag>::type* dummy = 0)
  {
  (void)dummy;
  this->ExecArg.SaveExecutionResult(id,worklet);
  }

  template <typename Worklet, typename HasOutTag>
  DAX_EXEC_EXPORT
  void saveResult(int, const Worklet&, HasOutTag,
     typename boost::disable_if<HasOutTag>::type* dummy = 0)
    {
    (void)dummy;
    }

};

}}} // namespace dax::exec::arg

#endif // !defined(DAX_DOXYGEN_ONLY)
#endif //__dax_exec_arg_BindDirect_h
