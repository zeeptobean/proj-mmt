@echo off
setlocal

set BaseDir=..

set Compiler=g++
set CompilerFlag=-Wall -Wextra -pedantic -g -march=native
set IncludeFlag=-I.\%BaseDir%\include\
set LinkingFlag=-lsodium -lws2_32 -lmf -lmfplat -lmfreadwrite -lmfuuid -lshlwapi -lole32 -loleaut32 -lrpcrt4 -lgdi32 -lgdiplus
set ImGuiFlag=-I.\%BaseDir%\imgui-sfml\include\ -L.\%BaseDir%\imgui-sfml\lib\
set ImGuiLinkingFlag=-limgui-sfml -lopengl32 -lglu32 -lsfml-system -lsfml-window -lsfml-audio -lsfml-main -lsfml-graphics

set EngineFlag=-Wall -Wextra -pedantic -Os -march=native 

if not exist "%BaseDir%\bin" (
    mkdir "%BaseDir%\bin"
)

echo Compiling engine
if not exist "%BaseDir%\bin\engine" (
    mkdir "%BaseDir%\bin\engine"
)
for %%f in (%BaseDir%\src\engine\*.cpp) do (
    echo %%f
    %Compiler% %EngineFlag% %IncludeFlag% -c %%f -o "%BaseDir%\bin\engine\%%~nf.o"
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

echo Linking client
%Compiler% -mwindows -municode %CompilerFlag% "%BaseDir%\bin\engine\*.o" "%BaseDir%\bin\component\*.o" "%BaseDir%\bin\client.o" -o "%BaseDir%\bin\client.exe" %IncludeFlag% %ImGuiFlag% %ImGuiLinkingFlag% %LinkingFlag%

echo Compiling server
%Compiler% %CompilerFlag% %IncludeFlag% %ImGuiFlag% -c "%BaseDir%\src\server.cpp" -o "%BaseDir%\bin\server.o"

echo Linking server
%Compiler% %CompilerFlag% "%BaseDir%\bin\engine\*.o" "%BaseDir%\bin\component\*.o" "%BaseDir%\bin\server.o" -o "%BaseDir%\bin\server.exe" %IncludeFlag% %ImGuiFlag% %ImGuiLinkingFlag% %LinkingFlag%

endlocal