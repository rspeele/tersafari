@ECHO OFF

set TESS_BIN=bin

REM IF /I "%PROCESSOR_ARCHITECTURE%" == "amd64" (
REM     set TESS_BIN=bin64
REM )
REM IF /I "%PROCESSOR_ARCHITEW6432%" == "amd64" (
REM     set TESS_BIN=bin64
REM )

start %TESS_BIN%\tesseract.exe "-q$HOME\My Games\Tesseract" -ktesseract -ksauerbraten\packages -glog.txt %*
