Compilation with Visual Studio 2013, preferable with QT plugin

**Prerequisites:**

* **TUM.3D Virtual Wind Tunnel** library including the GPU PerfLib, respectively the *GPUPerfAPICL-x64.dll* file
* **Qt 5.5** or higher, located at the environment variable *QTDIR* such that *<QTDIR>\include* and *<QTDIR>\bin* are valid
* OpenCL installation e.g. AMD APP
* A Device (CPU or GPU) supporting respective OpenCL platform
* A GPU supporting DirectX 11
* Windows SDK containing DirectX


The compilation configuration of the project expects the the TUM.3D Virtual Wind Tunnel solution to be located in a parallel folder *WindTunnelSource*, so it can check prior to the compilation if the dll, OpenCL or shader files of the library have changed. If that is the case, it automatically pulls the newer files into its respective folders so the WindSim binaries can use them.

If only the TUM.3D Virtual Wind Tunnel library binaries are available, place the following files in the respective folders manually:
   Shader files *Smoke.cso*, *Lines.cso* in *<cwd>/Fx* (For compilation: *<solutionDir>\WindSim\Fx*)
   OpenCL files *FluidSimulation.cl*, *Lines.cl*, *Multigrid.cl*, *Smoke.cl* in *<cwd>\Kernels* (For compilation: *<solutionDir\WindSim\Kernels*)
   *Tum3DVirtualWindTunnelLib.dll* in the same folder as the *WindSim.exe*

Make sure to use the correct *OpenCL.dll*, matching with the used OpenCL platform and device. E.g. you can not use a *OpenCL.dll* of the AMD APP with CUDA and a Nvidia GPU.
Make sure the *GPUPerfAPICL-x64.dll* file is available, e.g. located next to the executable.
