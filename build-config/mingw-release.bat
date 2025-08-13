@echo off
setlocal enabledelayedexpansion

set BaseDir=..

set Compiler=g++
set CompilerFlag=-Wall -Wextra -pedantic -Os -march=nehalem
set IncludeFlag=-I.\%BaseDir%\include\
set LinkingFlag=-lsodium -lws2_32 -lmf -lmfplat -lmfreadwrite -lmfuuid -lshlwapi -lole32 -loleaut32 -lrpcrt4 -lgdi32 -lgdiplus
set ImGuiFlag=-I.\%BaseDir%\imgui-win32-dx9\include\ -L.\%BaseDir%\imgui-win32-dx9\lib\
set ImGuiLinkingFlag=-limgui-win32-dx9  -ld3d9 -ldwmapi -luser32 -lwinmm
set ReleaseFlag=-s -static -static-libgcc -static-libstdc++

if not exist "%BaseDir%\bin" (
    mkdir "%BaseDir%\bin"
)

echo Compiling engine
if not exist "%BaseDir%\bin\engine" (
    mkdir "%BaseDir%\bin\engine"
)
for %%f in (%BaseDir%\src\engine\*.cpp) do (
    echo %%f
    %Compiler% %CompilerFlag% %IncludeFlag% -c %%f -o "%BaseDir%\bin\engine\%%~nf.o"
)

echo Compiling component
if not exist "%BaseDir%\bin\component" (
    mkdir "%BaseDir%\bin\component"
)
for %%f in (%BaseDir%\src\component\*.cpp) do (
    echo %%f
    %Compiler% %CompilerFlag% %IncludeFlag% %ImGuiFlag% -c %%f -o "%BaseDir%\bin\component\%%~nf.o"
)

echo Compiling client
%Compiler% %CompilerFlag% %IncludeFlag% %ImGuiFlag% -c "%BaseDir%\src\client.cpp" -o "%BaseDir%\bin\client.o"

echo Compiling server
%Compiler% %CompilerFlag% %IncludeFlag% %ImGuiFlag% -c "%BaseDir%\src\server.cpp" -o "%BaseDir%\bin\server.o"

set ObjectFiles=
for %%f in ("%BaseDir%\bin\engine\*.o" "%BaseDir%\bin\component\*.o") do (
    set ObjectFiles=!ObjectFiles! "%%f"
)

echo Linking client
%Compiler% -mwindows -municode %ReleaseFlag% %CompilerFlag% %ObjectFiles% "%BaseDir%\bin\client.o"  -o "%BaseDir%\bin\client.exe" %IncludeFlag% %ImGuiFlag% %ImGuiLinkingFlag% %LinkingFlag%

echo Linking server
%Compiler% -mwindows -municode %ReleaseFlag% %CompilerFlag% %ObjectFiles% "%BaseDir%\bin\server.o" -o "%BaseDir%\bin\server.exe" %IncludeFlag% %ImGuiFlag% %ImGuiLinkingFlag% %LinkingFlag%

endlocal