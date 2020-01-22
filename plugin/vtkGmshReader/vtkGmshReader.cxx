/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGmshReader.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkGmshReader.h"

#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPointData.h>
#include <vtkCellType.h>

#include <string>
#include <vector>
#include <fstream>
#include <limits>

vtkStandardNewMacro(vtkGmshReader);

//----------------------------------------------------------------------------
vtkGmshReader::vtkGmshReader()
{
  this->FileName = nullptr;
  this->SetNumberOfInputPorts(0);
}

//----------------------------------------------------------------------------
vtkGmshReader::~vtkGmshReader()
{
  this->SetFileName(nullptr);
}

//----------------------------------------------------------------------------
int vtkGmshReader::RequestData(vtkInformation*,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  vtkUnstructuredGrid* output =
    vtkUnstructuredGrid::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  std::ifstream MshFile(this->FileName);

  // Nodes
  for (std::string line; std::getline(MshFile, line);) {
    if (line == "$Nodes") {
      break;
    }
  }

  int NumberOfEntityBlocks, NumberOfNodes, MinNodeTag, MaxNodeTag;
  MshFile >> NumberOfEntityBlocks >> NumberOfNodes >> MinNodeTag >> MaxNodeTag;

  std::size_t MaxNodeId = 0;
  std::size_t MinNodeId = std::numeric_limits<std::size_t>::max();
  
  vtkNew<vtkPoints> vertices;
  vertices->SetDataTypeToDouble();
  
  for (std::size_t i = 0; i < NumberOfEntityBlocks; ++i) {
    int EntityDim, EntityTag, Parametric, NumberOfNodesInBlock;
    MshFile >> EntityDim >> EntityTag >> Parametric >> NumberOfNodesInBlock;

    std::size_t NumberOfCoords = 3;
    if (Parametric) {
      NumberOfCoords += EntityDim;
    }
    
    std::vector<std::size_t> tags;
    for (std::size_t j = 0; j < NumberOfNodesInBlock; ++j) {
      std::size_t tag;
      MshFile >> tag;
      tags.push_back(tag);
    }

    for (std::size_t j = 0; j < NumberOfNodesInBlock; ++j) {
      std::size_t NodeTag = tags[j];

      double x, y, z;
      if (NumberOfCoords == 5) {
	double u, v;
	MshFile >> x >> y >> z >> u >> v; 
      } else if (NumberOfCoords == 4) {
	double u;
	MshFile >> x >> y >> z >> u;
      } else {
	MshFile >> x >> y >> z;
      }

      MinNodeId = std::min(MinNodeId, NodeTag);
      MaxNodeId = std::max(MaxNodeId, NodeTag);

      vertices->InsertPoint(NodeTag-1, x, y, z);
    }
  }

  // Consistency check
  if (MinNodeTag != MinNodeId || MaxNodeTag != MaxNodeId) {
    vtkWarningMacro("Min/Max node tags reported in section header are wrong: "
		    << "(" << MinNodeTag << "/" << MaxNodeTag << ") != "
		    << "(" << MinNodeId << "/" << MaxNodeId << ")");
  }

  output->SetPoints(vertices);

  // Cells
  for (std::string line; std::getline(MshFile, line);) {
    if (line == "$Elements") {
      break;
    }
  }

  int NumberOfElements, MinElementTag, MaxElementTag;
  MshFile >> NumberOfEntityBlocks >> NumberOfElements >> MinElementTag >> MaxElementTag;

  std::size_t MaxElementId = 0;
  std::size_t MinElementId = std::numeric_limits<std::size_t>::max();

  output->Allocate(NumberOfElements);
  vtkNew<vtkIdList> ids;
  
  for (std::size_t i = 0; i < NumberOfEntityBlocks; ++i) {
    int EntityDim, EntityTag, ElementType, NumberOfElementsInBlock;
    MshFile >> EntityDim >> EntityTag >> ElementType >> NumberOfElementsInBlock;

    const int NumberOfVerticesPerElement =
      this->GetNumberOfVerticesForElementType(ElementType);

    for (std::size_t j = 0; j < NumberOfElementsInBlock; ++j) {
      std::size_t ElementTag;
      MshFile >> ElementTag;

      ids->SetNumberOfIds(NumberOfVerticesPerElement);
      
      for (int k = 0; k < NumberOfVerticesPerElement; ++k) {
	std::size_t VertexTag;
	MshFile >> VertexTag;
	ids->SetId(k, VertexTag-1);
      }

      MinElementId = std::min(MinElementId, ElementTag);
      MaxElementId = std::min(MaxElementId, ElementTag);

      output->InsertNextCell(this->GetVTKCellType(ElementType), ids);
    }
  }

  return 1;
}

//----------------------------------------------------------------------------
int vtkGmshReader::RequestInformation(vtkInformation*, vtkInformationVector**,
				      vtkInformationVector* outputVector)
{
  if (!this->FileName) {
    vtkErrorMacro("FileName has to be specified.");
    return 0;
  }

  // $MeshFormat section.
  double FormatVersionNumber;
  int FileType;  // 0 for ASCII, 1 for binary.
  int DataSize;  // sizeof(size_t).

  std::ifstream MshFile(this->FileName);
  std::string line;
  MshFile >> line;
  if (line != "$MeshFormat") {
    vtkErrorMacro("Expected $MeshFormat in first line.");
    return 0;
  }

  MshFile >> FormatVersionNumber >> FileType >> DataSize;

  // TODO: implement 2.0 and 3.0 formats.
  if (FormatVersionNumber < 4.0) {
    vtkErrorMacro("Reader can only read MSH file format version 4.0 and up.");
    return 0;
  }

  // TODO: read binary files too.
  if (FileType != 0) {
    vtkErrorMacro("Reader can only read ASCII formatted files");
    return 0;
  }

  MshFile >> line;
  if (line != "$EndMeshFormat") {
    vtkErrorMacro("Expected $EndMeshFormat.");
    return 0;
  }

  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  return 1;
}

//----------------------------------------------------------------------------
VTKCellType vtkGmshReader::GetVTKCellType(int mshElementType)
{
  VTKCellType CellType = VTK_EMPTY_CELL;
  
  switch(mshElementType) {
  case 1:
    CellType = VTK_LINE;
    break;
  case 2:
    CellType = VTK_TRIANGLE;
    break;
  case 3:
    CellType = VTK_QUAD;
    break;
  case 4:
    CellType = VTK_TETRA;
    break;
  case 5:
    CellType = VTK_HEXAHEDRON;
    break;
  case 6:
    CellType = VTK_WEDGE;
    break;
  case 7:
    CellType = VTK_PYRAMID;
    break;
  case 8:
    CellType = VTK_QUADRATIC_EDGE;
    break;
  case 9:
    CellType = VTK_QUADRATIC_TRIANGLE;
    break;
  case 10:
    CellType = VTK_BIQUADRATIC_QUAD;
    break;
  case 11:
    CellType = VTK_QUADRATIC_TETRA;
    break;
  case 12:
    CellType = VTK_TRIQUADRATIC_HEXAHEDRON;
    break;
  case 13:
    CellType = VTK_BIQUADRATIC_QUADRATIC_WEDGE;
    break;
  case 14:
    CellType = VTK_PYRAMID;
    break;
  case 15:
    CellType = VTK_VERTEX;
    break;
  case 16:
    CellType = VTK_QUADRATIC_QUAD;
    break;
  case 17:
    CellType = VTK_QUADRATIC_HEXAHEDRON;
    break;
  case 18:
    CellType = VTK_QUADRATIC_WEDGE;
    break;
  case 19:
    CellType = VTK_QUADRATIC_PYRAMID;
    break;
  case 20:
    CellType = VTK_TRIANGLE;
    break;
  case 21:
    CellType = VTK_TRIANGLE;
    break;
  case 22:
    CellType = VTK_TRIANGLE;
    break;
  case 23:
    CellType = VTK_TRIANGLE;
    break;
  case 24:
    CellType = VTK_TRIANGLE;
    break;
  case 25:
    CellType = VTK_TRIANGLE;
    break;
  case 26:
    CellType = VTK_POLY_LINE;
    break;
  case 27:
    CellType = VTK_POLY_LINE;
    break;
  case 28:
    CellType = VTK_POLY_LINE;
    break;
  case 29:
    CellType = VTK_TETRA;
    break;
  case 30:
    CellType = VTK_TETRA;
    break;
  case 31:
    CellType = VTK_TETRA;
    break;
  case 92:
    CellType = VTK_HEXAHEDRON;
    break;
  case 93:
    CellType = VTK_HEXAHEDRON;
    break;
  default:
    vtkErrorMacro("Cannot convert unknown element type " << mshElementType);
    break;
  }

  return CellType;
}

//----------------------------------------------------------------------------
int vtkGmshReader::GetNumberOfVerticesForElementType(int mshElementType)
{
  int NumberOfVertices = 0;

  switch(mshElementType) {
  case 1:
    NumberOfVertices = 2;
    break;
  case 2:
    NumberOfVertices = 3;
    break;
  case 3:
    NumberOfVertices = 4;
    break;
  case 4:
    NumberOfVertices = 4;
    break;
  case 5:
    NumberOfVertices = 8;
    break;
  case 6:
    NumberOfVertices = 6;
    break;
  case 7:
    NumberOfVertices = 5;
    break;
  case 8:
    NumberOfVertices = 3;
    break;
  case 9:
    NumberOfVertices = 6;
    break;
  case 10:
    NumberOfVertices = 9;
    break;
  case 11:
    NumberOfVertices = 10;
    break;
  case 12:
    NumberOfVertices = 27;
    break;
  case 13:
    NumberOfVertices = 18;
    break;
  case 14:
    NumberOfVertices = 14;
    break;
  case 15:
    NumberOfVertices = 1;
    break;
  case 16:
    NumberOfVertices = 8;
    break;
  case 17:
    NumberOfVertices = 20;
    break;
  case 18:
    NumberOfVertices = 15;
    break;
  case 19:
    NumberOfVertices = 13;
    break;
  case 20:
    NumberOfVertices = 9;
    break;
  case 21:
    NumberOfVertices = 10;
    break;
  case 22:
    NumberOfVertices = 12;
    break;
  case 23:
    NumberOfVertices = 15;
    break;
  case 24:
    NumberOfVertices = 15;
    break;
  case 25:
    NumberOfVertices = 21;
    break;
  case 26:
    NumberOfVertices = 4;
    break;
  case 27:
    NumberOfVertices = 5;
    break;
  case 28:
    NumberOfVertices = 6;
    break;
  case 29:
    NumberOfVertices = 20;
    break;
  case 30:
    NumberOfVertices = 35;
    break;
  case 31:
    NumberOfVertices = 56;
    break;
  case 92:
    NumberOfVertices = 64;
    break;
  case 93:
    NumberOfVertices = 125;
    break;
  default:
    vtkErrorMacro("Unknown element type " << mshElementType);
    break;
  }

  return NumberOfVertices;
}

//----------------------------------------------------------------------------
bool vtkGmshReader::CanReadFile(const char* filename)
{
  // TODO: check validity of the $MeshFormat section here.
  return true;
}

//----------------------------------------------------------------------------
void vtkGmshReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
