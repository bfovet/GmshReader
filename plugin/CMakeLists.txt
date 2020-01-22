
paraview_add_plugin(GmshReader
  VERSION "1.0"
  MODULES vtkGmshReader
  MODULE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/vtkGmshReader/vtk.module"
  SERVER_MANAGER_XML GmshReader.xml
  )
