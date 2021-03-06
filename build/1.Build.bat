@echo off

REM Set the working folder to the folder this batch file is in just to be sure
%~d0
cd %~dp0

time /T

if not exist "SetEnvVars.bat" (
	copy "SetEnvVars.bat.template" "SetEnvVars.bat"

	call :exec ".\bin\fregex.exe" "r|VS9PATH=.*$|VS9PATH=$r|HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\9.0\InstallDir|" -i "SetEnvVars.bat" -o "SetEnvVars.bat"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

	call :exec ".\bin\fregex.exe" "r|TSVNPATH=.*$|TSVNPATH=$r|HKEY_LOCAL_MACHINE\SOFTWARE\TortoiseSVN\Directory|" -i "SetEnvVars.bat" -o "SetEnvVars.bat"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
)

call SetEnvVars.bat

if not exist "%TSVNPATH%" (
	echo Error: TSVNPATH does not exist: "%TSVNPATH%"
	echo Please edit your SetEnvVars.bat file to enter the correct location for the TortoiseSVN installation folder
	goto ConfigIsWrong
)

if not exist "%VS9PATH%" (
	echo Error: VS9PATH does not exist: "%VS9PATH%"
	echo Please edit your SetEnvVars.bat file to enter the correct location for your Visual Studio 2008 installation
	goto ConfigIsWrong
)

if exist devenv.log del devenv.log

if not exist .\Temp mkdir .\Temp

:Update-VersionNumbers
echo ######################## Update-VersionNumbers ########################

	REM Use the rev of the repo for the version
	call :exec "%TSVNPATH%\bin\subwcrev.exe" ".." "SetVersion.bat.template" "SetVersion.bat"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
	call SetVersion.bat
	echo Version is %VERSION%
	del SetVersion.bat

	REM back up files that hold versions so they can be restored after the build
	REM  (this is just so that these files don't keep showing up in the svn commit dialog)
	call :exec copy /Y "..\src\PropPage.rc" ".\Temp\PropPage.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

	REM Replace the version numbers
	call :exec ".\bin\fregex.exe" "s/FILEVERSION .*$/FILEVERSION %VERSION_A%,%VERSION_B%,%VERSION_C%,%VERSION_D%/" "s/FileVersion\".*$/FileVersion\", \"%VERSION%\"/" -i "..\src\PropPage.rc" -o "..\src\PropPage.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
	call :exec ".\bin\fregex.exe" "s/PRODUCTVERSION .*$/PRODUCTVERSION %VERSION_A%,%VERSION_B%,%VERSION_C%,%VERSION_D%/" "s/ProductVersion\".*$/ProductVersion\", \"%VERSION%\"/" -i "..\src\PropPage.rc" -o "..\src\PropPage.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
    
:End-VersionNumbers

:Build-TSFileSource
echo ######################## Build-TSFileSource ########################

	call :exec "%VS9PATH%\devenv.exe" "..\TSFileSource.sln" /rebuild "Release|Win32" /out devenv.log
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

:CreateBuildOutput

	if not exist "..\builds" mkdir "..\builds"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
    
	if not exist "..\builds\TSFileSource-%VERSION%" mkdir "..\builds\TSFileSource-%VERSION%"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

	copy /Y "..\bin\Release\*.ax" "..\builds\TSFileSource-%VERSION%"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
	copy /Y "..\bin\Release\*.bat" "..\builds\TSFileSource-%VERSION%"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
	copy /Y "..\bin\Release\*.reg" "..\builds\TSFileSource-%VERSION%"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
	copy /Y "..\bin\Release\*.txt" "..\builds\TSFileSource-%VERSION%"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

:CreateSourceOutput
	if exist "..\builds\TSFileSource-%VERSION%src" rmdir /S /Q "..\builds\TSFileSource-%VERSION%src"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

	call :export ".." ".\Temp" "..\builds\TSFileSource-%VERSION%src"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
    
	REM restore resource files to original (unaltered version number)
	copy /Y ".\Temp\PropPage.rc" "..\src\PropPage.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

    
:end
echo Build Successful
time /T
exit /B 0	

:ConfigIsWrong
	echo -------------------
	echo The settings file will now be opened in notepad.
	echo Please confirm that each of the path variables is pointing to the correct location.
	echo -------------------
	pause
	start notepad "SetEnvVars.bat"
	exit /B 1

:exec
pushd %CD%
if "%~x1" == ".bat" (
	echo call %*
	call %*
) else (
	echo %*
	%*
)
popd

set RETURNCODE=%ERRORLEVEL%
if not %RETURNCODE%==0 echo Exited with code %RETURNCODE%
echo _ %RETURNCODE%

exit /B %RETURNCODE%

:export
if exist "%~f2\%~n1" rmdir /S /Q "%~f2\%~n1"
if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

call :exec "%TSVNPATH%\bin\TortoiseProc.exe" /command:dropexport /path:"%~f1" /droptarget:"%~f2"
if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

move /Y "%~f2\%~n1" "%~f3"
exit /B %ERRORLEVEL%
