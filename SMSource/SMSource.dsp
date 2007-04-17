# Microsoft Developer Studio Project File - Name="SMSource" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=SMSource - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SMSource.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SMSource.mak" CFG="SMSource - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SMSource - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SMSource - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SMSource - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj\Release"
# PROP Intermediate_Dir "obj\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SMSource_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Gi /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SMSource_EXPORTS" /FR /FD /c
# SUBTRACT CPP /WX /YX /Yc /Yu
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 strmbase.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib WINMM.LIB ws2_32.lib shlwapi.lib version.lib /nologo /dll /machine:I386 /out:"bin/SMSource.ax"
# SUBTRACT LINK32 /pdb:none /incremental:yes /nodefaultlib

!ELSEIF  "$(CFG)" == "SMSource - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj\Debug"
# PROP Intermediate_Dir "obj\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SMSource_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SMSource_EXPORTS" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib strmbasd.lib WINMM.LIB ws2_32.lib shlwapi.lib version.lib /nologo /dll /debug /machine:I386 /out:"bin/SMSource.ax" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "SMSource - Win32 Release"
# Name "SMSource - Win32 Debug"
# Begin Group "SM Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\FileReader.cpp
# End Source File
# Begin Source File

SOURCE=.\src\setup.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SharedMemory.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SMSource.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SMSourcePin.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SMSourceProp.cpp
# End Source File
# End Group
# Begin Group "SM Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\FileReader.h
# End Source File
# Begin Source File

SOURCE=.\src\Global.h
# End Source File
# Begin Source File

SOURCE=.\src\ISMSource.h
# End Source File
# Begin Source File

SOURCE=.\src\resource.h
# End Source File
# Begin Source File

SOURCE=.\src\SharedMemory.h
# End Source File
# Begin Source File

SOURCE=.\src\SMSource.def
# End Source File
# Begin Source File

SOURCE=.\src\SMSource.h
# End Source File
# Begin Source File

SOURCE=.\src\SMSourceGuids.h
# End Source File
# Begin Source File

SOURCE=.\src\SMSourcePin.h
# End Source File
# Begin Source File

SOURCE=.\src\SMSourceProp.h
# End Source File
# End Group
# Begin Group "Resources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\PropPage.rc
# End Source File
# End Group
# Begin Group "SM Sink Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\RegStore.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SMSink.cpp
# End Source File
# End Group
# Begin Group "SM Sink Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\RegStore.h
# End Source File
# Begin Source File

SOURCE=.\src\SMSink.h
# End Source File
# Begin Source File

SOURCE=.\src\SMSinkGuids.h
# End Source File
# End Group
# End Target
# End Project
