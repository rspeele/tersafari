@ECHO OFF

set TSF_BIN=bin

IF /I "%PROCESSOR_ARCHITECTURE%" == "amd64" (
    set TSF_BIN=bin64
)
IF /I "%PROCESSOR_ARCHITEW6432%" == "amd64" (
    set TSF_BIN=bin64
)
IF NOT EXIST "%TSF_BIN%" (
   IF /I "%TSF_BIN%" == "bin64" (
       set TSF_BIN=bin
   ) else (
       set TSF_BIN=bin64
   )
)

start %TSF_BIN%\tersafari.exe "-q$HOME\My Games\Tersafari" -ktesseract -ktsfmod -glog.txt %*
