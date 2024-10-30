:: Change the "1.3.211.0" to your Vulkan version.
:: pass parameter debug, D, release, R 

@echo off
:: Output help info for user...
if %1 == ? (echo Compile Parameters: _DEBUG or _RELEASE)

:: Compiles shaders for debug...
if %1 == _DEBUG (
	echo Compiling SPIR-V shaders via GLSL-C...
	if not exist "%CD%\x64\DEBUG\Shaders\" (mkdir "%CD%\x64\DEBUG\Shaders\")
	echo _DEBUG From: 		"%CD%\Shaders\"
	echo _DEBUG To: 		"%CD%\x64\DEBUG\Shaders\"
	
	for %%f in (%CD%\Shaders\*.frag %CD%\Shaders\*.vert %CD%\Shaders\*.comp) do (
		if %%~xf == .frag (echo 	Vertex Stage:		%%~nf.spv)
		if %%~xf == .vert (echo 	Fragment Stage:		%%~nf.spv)
		if %%~xf == .comp (echo 	Compute Stage:		%%~nf.spv)
		"%VULKAN%\Bin\glslc.exe" %%f -o "%CD%\x64\DEBUG\Shaders\%%~nf.spv"
	)
)

:: Compiles shaders for release...
if %1 == _RELEASE (
	echo Compiling SPIR-V shaders via GLSL-C...
	if not exist "%CD%\x64\RELEASE\Shaders\" (mkdir "%CD%\x64\RELEASE\Shaders\")
	echo _RELEASE From: 	"%CD%\Shaders\"
	echo _RELEASE To: 		"%CD%\x64\RELEASE\Shaders\"
	
	for %%f in (%CD%\Shaders\*.frag %CD%\Shaders\*.vert %CD%\Shaders\*.comp) do (
		if %%~xf == .frag (echo 	Vertex Stage:		%%~nf.spv)
		if %%~xf == .vert (echo 	Fragment Stage:		%%~nf.spv)
		if %%~xf == .comp (echo 	Compute Stage:		%%~nf.spv)
		"%VULKAN%\Bin\glslc.exe" %%f -o "%CD%\x64\RELEASE\Shaders\%%~nf.spv"
	)
)
echo.
pause