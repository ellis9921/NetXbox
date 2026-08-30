@echo off
setlocal enabledelayedexpansion
echo =============================================
echo   NetXbox Browser - Xbox 360 Build Script
echo =============================================
echo.

set "XEDK=C:\Program Files (x86)\Microsoft Xbox 360 SDK"
set "XDKCL=%XEDK%\bin\win32\cl.exe"
set "XDKLINK=%XEDK%\bin\win32\link.exe"
set "IMAGEXEX=%XEDK%\bin\win32\imagexex.exe"
set "ROOT=%~dp0"
set "OUTDIR=%ROOT%build_xbox\Release"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "INCLUDE=%XEDK%\include\xbox;%XEDK%\include\xbox\sys;%XEDK%\include\win32"

set "FLAGS=/TP /D_XBOX /DPLATFORM_XBOX360 /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /MT /O2 /GS- /GF /GR- /fp:fast /Zi /W3 /nologo /EHsc"
set "INC=/I"%ROOT%include\xbox_compat" /I"%ROOT%include" /I"%ROOT%thirdparty""

echo [1/3] Compiling...
echo.

set FAILED=0

echo   Compiling main...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\main.c" /Fo"%OUTDIR%\main.obj" 2>nul
if errorlevel 1 ( echo   FAILED: main & set FAILED=1 )

echo   Compiling app...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\app.c" /Fo"%OUTDIR%\app.obj" 2>nul
if errorlevel 1 ( echo   FAILED: app & set FAILED=1 )

echo   Compiling types...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\core\types.c" /Fo"%OUTDIR%\types.obj" 2>nul
if errorlevel 1 ( echo   FAILED: types & set FAILED=1 )

echo   Compiling translate...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\core\translate.c" /Fo"%OUTDIR%\translate.obj" 2>nul
if errorlevel 1 ( echo   FAILED: translate & set FAILED=1 )

echo   Compiling logo_data...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\core\logo_data.c" /Fo"%OUTDIR%\logo_data.obj" 2>nul
if errorlevel 1 ( echo   FAILED: logo_data & set FAILED=1 )

echo   Compiling html...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\html\html.c" /Fo"%OUTDIR%\html.obj" 2>nul
if errorlevel 1 ( echo   FAILED: html & set FAILED=1 )

echo   Compiling http...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\network\http.c" /Fo"%OUTDIR%\http.obj" 2>nul
if errorlevel 1 ( echo   FAILED: http & set FAILED=1 )

echo   Compiling renderer...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\renderer\renderer.c" /Fo"%OUTDIR%\renderer.obj" 2>nul
if errorlevel 1 ( echo   FAILED: renderer & set FAILED=1 )

echo   Compiling browser...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\browser\browser.c" /Fo"%OUTDIR%\browser.obj" 2>nul
if errorlevel 1 ( echo   FAILED: browser & set FAILED=1 )

echo   Compiling ui...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\ui\ui.c" /Fo"%OUTDIR%\ui.obj" 2>nul
if errorlevel 1 ( echo   FAILED: ui & set FAILED=1 )

echo   Compiling framebuffer...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\graphics\framebuffer.c" /Fo"%OUTDIR%\framebuffer.obj" 2>nul
if errorlevel 1 ( echo   FAILED: framebuffer & set FAILED=1 )

echo   Compiling font...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\graphics\font.c" /Fo"%OUTDIR%\font.obj" 2>nul
if errorlevel 1 ( echo   FAILED: font & set FAILED=1 )

echo   Compiling stb_truetype_impl...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\graphics\stb_truetype_impl.c" /Fo"%OUTDIR%\stb_truetype_impl.obj" 2>nul
if errorlevel 1 ( echo   FAILED: stb_truetype_impl & set FAILED=1 )

echo   Compiling image...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\graphics\image.c" /Fo"%OUTDIR%\image.obj" 2>nul
if errorlevel 1 ( echo   FAILED: image & set FAILED=1 )

echo   Compiling adblock...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\adblock\adblock.c" /Fo"%OUTDIR%\adblock.obj" 2>nul
if errorlevel 1 ( echo   FAILED: adblock & set FAILED=1 )

echo   Compiling platform_xbox360...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\platform\xbox360\platform_xbox360.c" /Fo"%OUTDIR%\platform_xbox360.obj" 2>nul
if errorlevel 1 ( echo   FAILED: platform_xbox360 & set FAILED=1 )

echo   Compiling xui_ui...
"%XDKCL%" /c %FLAGS% %INC% "%ROOT%src\ui\xui_ui.cpp" /Fo"%OUTDIR%\xui_ui.obj" 2>nul
if errorlevel 1 ( echo   FAILED: xui_ui & set FAILED=1 )

if %FAILED% neq 0 (
    echo.
    echo ERROR: Compilation failed!
    pause
    exit /b 1
)

echo.
echo [2/3] Linking...
echo.

"%XDKLINK%" /SUBSYSTEM:XBOX /NODEFAULTLIB /INCREMENTAL:NO /NOLOGO /MAP:"%OUTDIR%\netxbox.map" /OUT:"%OUTDIR%\netxbox.exe" ^
    "%OUTDIR%\main.obj" "%OUTDIR%\app.obj" "%OUTDIR%\types.obj" "%OUTDIR%\translate.obj" "%OUTDIR%\logo_data.obj" "%OUTDIR%\html.obj" ^
    "%OUTDIR%\http.obj" "%OUTDIR%\renderer.obj" "%OUTDIR%\browser.obj" "%OUTDIR%\ui.obj" ^
    "%OUTDIR%\framebuffer.obj" "%OUTDIR%\font.obj" "%OUTDIR%\stb_truetype_impl.obj" "%OUTDIR%\image.obj" ^
    "%OUTDIR%\adblock.obj" "%OUTDIR%\platform_xbox360.obj" "%OUTDIR%\xui_ui.obj" ^
    "%XEDK%\lib\xbox\d3d9.lib" "%XEDK%\lib\xbox\d3dx9.lib" "%XEDK%\lib\xbox\xonline.lib" "%XEDK%\lib\xbox\xinput2.lib" ^
    "%XEDK%\lib\xbox\xnet.lib" "%XEDK%\lib\xbox\xboxkrnl.lib" "%XEDK%\lib\xbox\xapilib.lib" ^
    "%XEDK%\lib\xbox\libcMT.lib" "%XEDK%\lib\xbox\libcpMT.lib" "%XEDK%\lib\xbox\xgraphics.lib" ^
    "%XEDK%\lib\xbox\xuirun.lib" "%XEDK%\lib\xbox\xuirender.lib" "%XEDK%\lib\xbox\xmedia2.lib" ^
    "%XEDK%\lib\xbox\xmcore.lib" "%XEDK%\lib\xbox\xaudio2.lib" "%XEDK%\lib\xbox\xact3.lib" ^
    "%XEDK%\lib\xbox\x3daudio.lib" "%XEDK%\lib\xbox\xbdm.lib"

if errorlevel 1 (
    echo ERROR: Link failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Converting to XEX...
echo.

"%IMAGEXEX%" /IN:"%OUTDIR%\netxbox.exe" /OUT:"%OUTDIR%\netxbox.xex"

echo.
echo Building XUI media package...
set "XUI2BIN=%XEDK%\bin\win32\xui2bin.exe"
set "XUIPKG=%XEDK%\bin\win32\xuipkg.exe"
if not exist "%OUTDIR%\xui_build\xui" mkdir "%OUTDIR%\xui_build\xui"
"%XUI2BIN%" /NOLOGO "%ROOT%media\xui\netxbox_skin.xui" "%OUTDIR%\xui_build\xui\netxbox_skin.xur"
"%XUI2BIN%" /NOLOGO "%ROOT%media\xui\us_keyboard.xui" "%OUTDIR%\xui_build\xui\us_keyboard.xur"
"%XUI2BIN%" /NOLOGO "%ROOT%media\xui\toolbar.xui" "%OUTDIR%\xui_build\xui\toolbar.xur"
"%XUI2BIN%" /NOLOGO "%ROOT%media\xui\settings.xui" "%OUTDIR%\xui_build\xui\settings.xur"
"%XUI2BIN%" /NOLOGO "%ROOT%media\xui\home.xui" "%OUTDIR%\xui_build\xui\home.xur"
"%XUIPKG%" /NOLOGO /O "%OUTDIR%\xui_build\netxbox.xzp" "%OUTDIR%\xui_build\xui\netxbox_skin.xur" "%OUTDIR%\xui_build\xui\us_keyboard.xur" "%OUTDIR%\xui_build\xui\toolbar.xur" "%OUTDIR%\xui_build\xui\settings.xur" "%OUTDIR%\xui_build\xui\home.xur"

echo.
echo Staging media into %OUTDIR%\media\ ...
if not exist "%OUTDIR%\media" mkdir "%OUTDIR%\media"
if exist "%ROOT%media\xui\cursor.png" copy /Y "%ROOT%media\xui\cursor.png" "%OUTDIR%\media\cursor.png" >nul
if exist "%ROOT%media\xui\xarialuni.ttf" copy /Y "%ROOT%media\xui\xarialuni.ttf" "%OUTDIR%\media\xarialuni.ttf" >nul
if exist "%OUTDIR%\xui_build\netxbox.xzp" copy /Y "%OUTDIR%\xui_build\netxbox.xzp" "%OUTDIR%\media\netxbox.xzp" >nul

echo.
if exist "%OUTDIR%\netxbox.xex" (
    echo SUCCESS: %OUTDIR%\netxbox.xex
) else (
    echo SUCCESS: %OUTDIR%\netxbox.exe
)
echo.
echo Deploy: Copy netxbox.xex to USB, launch from XeXMenu on your Xbox 360
echo.

endlocal
exit /b 0
