# Microsoft Developer Studio Project File - Name="TSFileSource" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=TSFileSource - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "TSFileSource.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "TSFileSource.mak" CFG="TSFileSource - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "TSFileSource - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "TSFileSource - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "TSFileSource - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TSFILESOURCE_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Gi /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TSFILESOURCE_EXPORTS" /FR /FD /c
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
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib strmbase.lib WINMM.LIB /nologo /dll /machine:I386 /out:"bin/TSFileSource.ax"
# SUBTRACT LINK32 /pdb:none /nodefaultlib

!ELSEIF  "$(CFG)" == "TSFileSource - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TSFILESOURCE_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TSFILESOURCE_EXPORTS" /FR /FD /GZ /c
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
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib strmbasd.lib WINMM.LIB /nologo /dll /debug /machine:I386 /out:"bin/TSFileSource.ax" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "TSFileSource - Win32 Release"
# Name "TSFileSource - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\PropPage.rc
# End Source File
# Begin Source File

SOURCE=.\src\RegStore.cpp
# End Source File
# Begin Source File

SOURCE=.\src\SettingsStore.cpp
# End Source File
# Begin Source File

SOURCE=.\src\setup.cpp
# End Source File
# Begin Source File

SOURCE=.\src\StreamInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\src\StreamParser.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSource.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\src\DvbFormats.h
# End Source File
# Begin Source File

SOURCE=.\src\RegStore.h
# End Source File
# Begin Source File

SOURCE=.\src\resource.h
# End Source File
# Begin Source File

SOURCE=.\src\SettingsStore.h
# End Source File
# Begin Source File

SOURCE=.\src\StreamInfo.h
# End Source File
# Begin Source File

SOURCE=.\src\StreamParser.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\TSFileSource.reg
# End Source File
# End Group
# Begin Group "TSFileSource"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\ITSFileSource.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSource.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSource.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSourceGuids.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSourcePin.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSourcePin.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSourceProp.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSourceProp.h
# End Source File
# End Group
# Begin Group "TSFileSink"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\ITSFileSink.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSink.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSink.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSinkFilter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSinkFilter.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSinkGuids.h
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSinkPin.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSFileSinkPin.h
# End Source File
# End Group
# Begin Group "File Handling"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\FileReader.cpp
# End Source File
# Begin Source File

SOURCE=.\src\FileReader.h
# End Source File
# Begin Source File

SOURCE=.\src\FileWriter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\FileWriter.h
# End Source File
# Begin Source File

SOURCE=.\src\MultiFileReader.cpp
# End Source File
# Begin Source File

SOURCE=.\src\MultiFileReader.h
# End Source File
# Begin Source File

SOURCE=.\src\MultiFileWriter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\MultiFileWriter.h
# End Source File
# Begin Source File

SOURCE=.\src\TSBuffer.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TSBuffer.h
# End Source File
# End Group
# Begin Group "Filter Handling"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\Demux.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Demux.h
# End Source File
# Begin Source File

SOURCE=.\src\MediaFormats.h
# End Source File
# Begin Source File

SOURCE=.\src\PidInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\src\PidInfo.h
# End Source File
# Begin Source File

SOURCE=.\src\PidParser.cpp
# End Source File
# Begin Source File

SOURCE=.\src\PidParser.h
# End Source File
# Begin Source File

SOURCE=.\src\TunerEvent.cpp
# End Source File
# Begin Source File

SOURCE=.\src\TunerEvent.h
# End Source File
# End Group
# End Target
# End Project
