/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGmshReader.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGmshReader
 * @brief   Reader for visualization of high-order polynomial solutions
 *          under the GMSH format (version 4 and up).
  */

#ifndef vtkGmshReader_h
#define vtkGmshReader_h

#include "vtkGmshReaderModule.h"

#include <vtkUnstructuredGridAlgorithm.h>
#include <vtkCellType.h>

class vtkGmshReader : public vtkUnstructuredGridAlgorithm
{
public:
  vtkTypeMacro(vtkGmshReader, vtkUnstructuredGridAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Construct object.
   */
  static vtkGmshReader* New();

  /**
   * Set and get the Gmsh file name.
   */
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

  /**
   * Static method to know if a file can be read
   * based on its filename.
   */
  static bool CanReadFile(const char* filename);

protected:
  vtkGmshReader();
  ~vtkGmshReader() override;

  int RequestInformation(vtkInformation* request,
			 vtkInformationVector** inputVector,
			 vtkInformationVector* outputVector) override;
  int RequestData(vtkInformation* request,
		  vtkInformationVector** inputVector,
		  vtkInformationVector* outputVector) override;

private:
  char* FileName;

  VTKCellType GetVTKCellType(int mshElementType);
  int GetNumberOfVerticesForElementType(int mshElementType);
  
  vtkGmshReader(const vtkGmshReader&) = delete;
  void operator=(const vtkGmshReader&) = delete;
};

#endif
