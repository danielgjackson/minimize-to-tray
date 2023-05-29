@ECHO OFF
SETLOCAL EnableDelayedExpansion
PUSHD %~dp0

rem If launched from anything other than cmd.exe, will have "%WINDIR%\system32\cmd.exe" in the command line
ECHO.%CMDCMDLINE% | FINDSTR /C:"%COMSPEC% /c" >NUL
IF ERRORLEVEL 1 GOTO NONINTERACTIVE
rem Preserve this as it seems to be corrupted below otherwise?!
SET CMDLINE=%CMDCMDLINE%
rem If launched from anything other than cmd.exe, last character of command line will always be a double quote
IF NOT ^!CMDCMDLINE:~-1!==^" GOTO NONINTERACTIVE
rem If run from Explorer, last-but-one character of command line will be a space
IF NOT "!CMDLINE:~-2,1!"==" " GOTO NONINTERACTIVE
SET INTERACTIVE=1
:NONINTERACTIVE

SET FIND_CL=
FOR %%p IN (cl.exe) DO SET "FIND_CL=%%~$PATH:p"
IF DEFINED FIND_CL (
  ECHO Build tools already on path.
  GOTO BUILD
)

ECHO Build tools not on path, looking for 'vcvarsall.bat'...
SET ARCH=x86
SET VCVARSALL=
FOR %%f IN (70 71 80 90 100 110 120 130 140) DO IF EXIST "!VS%%fCOMNTOOLS!\..\..\VC\vcvarsall.bat" SET VCVARSALL=!VS%%fCOMNTOOLS!\..\..\VC\vcvarsall.bat
FOR /F "usebackq tokens=*" %%f IN (`DIR /B /ON "%ProgramFiles(x86)%\Microsoft Visual Studio\????"`) DO FOR %%g IN (BuildTools Community Professional Enterprise) DO IF EXIST "%ProgramFiles(x86)%\Microsoft Visual Studio\%%f\%%g\VC\Auxiliary\Build\vcvarsall.bat" SET "VCVARSALL=%ProgramFiles(x86)%\Microsoft Visual Studio\%%f\%%g\VC\Auxiliary\Build\vcvarsall.bat"
IF "%VCVARSALL%"=="" ECHO Cannot find C compiler environment for 'vcvarsall.bat'. & GOTO ERROR
ECHO Setting environment variables for C compiler... %VCVARSALL%
CALL "%VCVARSALL%" %ARCH%

:BUILD
SET NOLOGO=/nologo
ECHO Compiling...
cl %NOLOGO% -c /EHsc /DUNICODE /D_UNICODE /Tc"minimize-to-tray.c"
IF ERRORLEVEL 1 GOTO ERROR
ECHO Resources...
rc %NOLOGO% minimize-to-tray.rc
IF ERRORLEVEL 1 GOTO ERROR
ECHO Linking...
rem /manifest:embed  -- now external .manifest is included in .rc file
link %NOLOGO% /out:minimize-to-tray.exe User32.lib minimize-to-tray minimize-to-tray.res /subsystem:windows
IF ERRORLEVEL 1 GOTO ERROR
ECHO Done.
IF DEFINED INTERACTIVE COLOR 2F & PAUSE & COLOR
GOTO :EOF

:ERROR
ECHO ERROR: An error occured.
IF DEFINED INTERACTIVE COLOR 4F & PAUSE & COLOR
EXIT /B 1
GOTO END

:END
POPD
