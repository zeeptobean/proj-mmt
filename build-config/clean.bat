@echo off
setlocal

set BaseDir=..

if exist "%BaseDir%\bin" (
    del /s /q "%BaseDir%\bin\*"
)

endlocal