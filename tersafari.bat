@ECHO OFF

set TSF_BIN=bin

REM IF /I "%PROCESSOR_ARCHITECTURE%" == "amd64" (
REM     set TSF_BIN=bin64
REM )
REM IF /I "%PROCESSOR_ARCHITEW6432%" == "amd64" (
REM     set TSF_BIN=bin64
REM )

start %TSF_BIN%\tersafari.exe "-q$HOME\My Games\Tersafari" -ktesseract -ktsfmod -glog.txt %*
