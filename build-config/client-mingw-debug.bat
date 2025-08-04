@echo off
setlocal

set BaseDir=..

set Compiler=g++
set CompilerFlag=-Wall -Wextra -pedantic -g -march=native
set IncludeFlag=-I.\%BaseDir%\include\
set ImGuiIncludeFlag=-I.\%BaseDir%\imgui-win32-dx9\include\
set ImGuiLibraryFlag=-I.\%BaseDir%\imgui-win32-dx9\lib\

if not exist "%BaseDir%\bin" (
    mkdir "%BaseDir%\bin"
)

if not exist "%BaseDir%\bin\client" (
    mkdir "%BaseDir%\bin\client"
)

if not exist "%BaseDir%\bin\client\obj" (
    mkdir "%BaseDir%\bin\client\obj"
)

for %%f in (%BaseDir%\src\engine\*.cpp) do (
    echo Compiling %%f
    %Compiler% %CompilerFlag% %IncludeFlag% -c %%f -o "%BaseDir%\bin\client\obj%%~nf.o"
)