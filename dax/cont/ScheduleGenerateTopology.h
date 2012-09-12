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

#ifndef __dax_cont_internal_ScheduleGenerateTopology_h
#define __dax_cont_internal_ScheduleGenerateTopology_h

#include <dax/Types.h>
#include <dax/exec/Cell.h>
#include <dax/exec/WorkletGenerateTopology.h>

#include <dax/exec/internal/ErrorMessageBuffer.h>
#include <dax/exec/internal/FieldAccess.h>
#include <dax/exec/internal/GridTopologies.h>

#include <dax/cont/DeviceAdapter.h>
#include <dax/cont/Schedule.h>

//todo still need to design a map adapter for new scheduler
#include <dax/cont/internal/ScheduleMapAdapter.h>

//where all my custom kernels I use are located
#include <dax/exec/internal/kernel/ScheduleGenerateTopology.h>

namespace dax {
namespace cont {

/// ScheduleGenerateTopology is the control enviorment representation of a
/// an algorithm that takes a classification of a given input topology
/// and will generate a new topology, but doesn't create new cells

template<class DeviceAdapterTag>
class ScheduleGenerateTopology
{
public:
  //classify type is the type
  typedef dax::Id ClassifyType;

  //mask type is the internal
  typedef dax::Id MaskType;

  typedef dax::cont::ArrayHandle< MaskType,
          ArrayContainerControlTagBasic, DeviceAdapterTag> PointMaskType;

  typedef dax::cont::ArrayHandle< ClassifyType,
          ArrayContainerControlTagBasic, DeviceAdapterTag> ClassifyResultType;

  ScheduleGenerateTopology(ClassifyResultType classification):
    RemoveDuplicatePoints(true),
    ReleaseClassification(true),
    CompactPointFieldPossible(false),
    Classification(classification)
    {
    }

  void SetReleaseClassification(bool b){ ReleaseClassification = b; }
  bool GetReleaseClassification() const { return ReleaseClassification; }


  void SetRemoveDuplicatePoints(bool b){ RemoveDuplicatePoints = b; }
  bool GetRemoveDuplicatePoints() const { return RemoveDuplicatePoints; }

  template<typename Worklet, typename InGridType, typename OutGridType>
  void CompactTopology(Worklet w, const InGridType& inGrid, OutGridType& outGrid)
    {
    this->ScheduleTopology(w,inGrid,outGrid);
    this->GeneratePointMask(inGrid,outGrid);
    if(this->RemoveDuplicatePoints)
      {
      this->CompactTopology(inGrid,outGrid);
      }
    this->CompactPointFieldPossible = true;
    }

  template<typename T, typename Container1, typename Container2>
  bool CompactPointField(
      const dax::cont::ArrayHandle<T,Container1,DeviceAdapterTag>& input,
      dax::cont::ArrayHandle<T,Container2,DeviceAdapterTag>& output)
    {
    if(!this->CompactPointFieldPossible)
      {
      return false;
      }
    dax::cont::internal::StreamCompact(input,
                                       this->PointMask,
                                       output,
                                       DeviceAdapterTag());
    return true;
    }
private:
  bool RemoveDuplicatePoints;
  bool ReleaseClassification;
  bool CompactPointFieldPossible;
  ClassifyResultType Classification;
  PointMaskType PointMask;

  typedef dax::cont::ArrayHandle< dax::Id,
    ArrayContainerControlTagBasic, DeviceAdapterTag> IdArrayHandleType;

protected:

  template<typename Worklet, typename InGridType, typename OutGridType>
  void ScheduleTopology(Worklet& w,
                        const InGridType& inGrid,
                        OutGridType& outGrid)
  {
    //do an inclusive scan of the cell count / cell mask to get the number
    //of cells in the output
    IdArrayHandleType scannedNewCellCounts;
    const dax::Id numNewCells =
        dax::cont::internal::InclusiveScan(this->Classification,
                                           scannedNewCellCounts,
                                           DeviceAdapterTag());

    if(this->ReleaseClassification)
      {
      this->Classification.ReleaseResourcesExecution();
      }

    //fill the validCellRange with the values from 1 to size+1, this is used
    //for the lower bounds to compute the right indices
    IdArrayHandleType validCellRange;
    dax::cont::internal::Schedule(
          dax::exec::internal::kernel::LowerBoundsInputFunctor
              <typename IdArrayHandleType::PortalExecution>(
                validCellRange.PrepareForOutput(numNewCells)),
          numNewCells,
          DeviceAdapterTag());

    //now do the lower bounds of the cell indices so that we figure out
    //which original topology indexs match the new indices.
    dax::cont::internal::LowerBounds(scannedNewCellCounts,
                                     validCellRange,
                                     DeviceAdapterTag());

    // We are done with scannedNewCellCounts.
    scannedNewCellCounts.ReleaseResources();


    typedef dax::exec::internal::kernel::GenerateTopologyFunctor<
        Worklet,
        typename InGridType::TopologyStructConstExecution,
        typename OutGridType::TopologyStructExecution> FunctorType;

    FunctorType functor(w,
                        inGrid.PrepareForInput(),
                        outGrid.PrepareForOutput(numNewCells));

    //Schedule Map uses the validCellRange for the lookup array
    //in this case the scheduler looks the array for the value to pass
    //instead of the index. We need a way in the current scheduler for this
    //syntax maybe something like Topology, Map, Field(Map,Topology) ??
    dax::cont::internal::ScheduleMap(functor,validCellRange);
  }

  template<class InGridType, class OutGridType>
  void GeneratePointMask(const InGridType &inGrid,
                                    const OutGridType &outGrid)
    {
    typedef typename PointMaskType::PortalExecution MaskPortalType;

    // Clear out the mask
    dax::cont::internal::Schedule(
          dax::exec::internal::kernel::ClearUsedPointsFunctor<MaskPortalType>(
            this->PointMask.PrepareForOutput(inGrid.GetNumberOfPoints())),
          inGrid.GetNumberOfPoints(),
          DeviceAdapterTag());

    // Mark every point that is used at least once.
    // This only works when outGrid is an UnstructuredGrid.
    dax::cont::internal::ScheduleMap(
          dax::exec::internal::kernel::GetUsedPointsFunctor<MaskPortalType>(
            this->PointMask.PrepareForInPlace()),
          outGrid.GetCellConnections());
  }

  template<typename InGridType,typename OutGridType>
  void CompactTopology(const InGridType &inGrid,
                                 OutGridType& outGrid)
    {
    // Here we are assuming OutGridType is an UnstructuredGrid so that we
    // can set point and connectivity information.

    //extract the point coordinates that we need for the new topology
    dax::cont::internal::StreamCompact(inGrid.GetPointCoordinates(),
                                       this->PointMask,
                                       outGrid.GetPointCoordinates(),
                                       DeviceAdapterTag());

    typedef typename OutGridType::CellConnectionsType CellConnectionsType;
    typedef typename OutGridType::PointCoordinatesType PointCoordinatesType;

    //compact the topology array to reference the extracted
    //coordinates ids
    {
    // Make usedPointIds become a sorted array of used point indices.
    // If entry i in usedPointIndices is j, then point index i in the
    // output corresponds to point index j in the input.
    IdArrayHandleType usedPointIndices;
    dax::cont::internal::Copy(outGrid.GetCellConnections(),
                              usedPointIndices,
                              DeviceAdapterTag());
    dax::cont::internal::Sort(usedPointIndices, DeviceAdapterTag());
    dax::cont::internal::Unique(usedPointIndices, DeviceAdapterTag());
    // Modify the connections of outGrid to point to compacted points.
    dax::cont::internal::LowerBounds(usedPointIndices,
                                     outGrid.GetCellConnections(),
                                     DeviceAdapterTag());
    }
    }
};



} //cont
} //dax


#endif // __dax_cont_ScheduleGenerateTopology_h
