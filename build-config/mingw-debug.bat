@echo off
setlocal enabledelayedexpansion

set BaseDir=..

set Compiler=g++
set CompilerFlag=-Wall -Wextra -pedantic -g -march=native
set IncludeFlag=-I.\%BaseDir%\include\
set LinkingFlag=-lsodium -lws2_32 -lmf -lmfplat -lmfreadwrite -lmfuuid -lshlwapi -lole32 -loleaut32 -lrpcrt4 -lgdi32 -lgdiplus
set ImGuiFlag=-I.\%BaseDir%\imgui-win32-dx9\include\ -L.\%BaseDir%\imgui-win32-dx9\lib\
set ImGuiLinkingFlag=-limgui-win32-dx9  -ld3d9 -ldwmapi -luser32 -lwinmm

set EngineFlag=-Wall -Wextra -pedantic -Os -march=native 

if not exist "%BaseDir%\bin" (
    mkdir "%BaseDir%\bin"
)

@REM echo Compiling engine
@REM if not exist "%BaseDir%\bin\engine" (
@REM     mkdir "%BaseDir%\bin\engine"
@REM )
@REM for %%f in (%BaseDir%\src\engine\*.cpp) do (
@REM     echo %%f
@REM     %Compiler% %CompilerFlag% %IncludeFlag% -c %%f -o "%BaseDir%\bin\engine\%%~nf.o"
@REM     @REM %Compiler% %EngineFlag% %IncludeFlag% -c %%f -o "%BaseDir%\bin\engine\%%~nf.o"
@REM )

@REM echo Compiling component
@REM if not exist "%BaseDir%\bin\component" (
@REM     mkdir "%BaseDir%\bin\component"
@REM )
@REM for %%f in (%BaseDir%\src\component\*.cpp) do (
@REM     echo %%f
@REM     %Compiler% %CompilerFlag% %IncludeFlag% %ImGuiFlag% -c %%f -o "%BaseDir%\bin\component\%%~nf.o"
@REM )

@REM echo Compiling client
@REM %Compiler% %CompilerFlag% %IncludeFlag% %ImGuiFlag% -c "%BaseDir%\src\client.cpp" -o "%BaseDir%\bin\client.o"

echo Compiling server
%Compiler% %CompilerFlag% %IncludeFlag% %ImGuiFlag% -c "%BaseDir%\src\server.cpp" -o "%BaseDir%\bin\server.o"

set ObjectFiles=
for %%f in ("%BaseDir%\bin\engine\*.o" "%BaseDir%\bin\component\*.o") do (
    set ObjectFiles=!ObjectFiles! "%%f"
)

@REM echo Linking client
@REM %Compiler% -mwindows -municode %CompilerFlag% %ObjectFiles% "%BaseDir%\bin\client.o"  -o "%BaseDir%\bin\client.exe" %IncludeFlag% %ImGuiFlag% %ImGuiLinkingFlag% %LinkingFlag%

echo Linking server
%Compiler% -mwindows -municode %CompilerFlag% %ObjectFiles% "%BaseDir%\bin\server.o" -o "%BaseDir%\bin\server.exe" %IncludeFlag% %ImGuiFlag% %ImGuiLinkingFlag% %LinkingFlag%

endlocal