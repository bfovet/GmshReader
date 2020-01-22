set(classes
  vtkGmshReader
)

vtk_module_add_module(vtkGmshReader
  FORCE_STATIC
  CLASSES ${classes}
  )
