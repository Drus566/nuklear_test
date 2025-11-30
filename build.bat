@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl /D_CRT_SECURE_NO_DEPRECATE /nologo /W3 /O2 /MD /fp:fast /Gm- src\main.c user32.lib dxguid.lib d3d11.lib /I"include"  /Fe:build\sdl3_app.exe /link /incremental:no
@REM cl /D_CRT_SECURE_NO_DEPRECATE /nologo /W3 /O2 /fp:fast /Gm- /Fedemo.exe main.c user32.lib dxguid.lib d3d11.lib /link /incremental:no

if %errorlevel% == 0 (
    echo Success compile! 
    del *.obj *.pdb *.ilk 2>nul
) else (
    echo Error compile!
    pause
)

if not exist "build\SDL3.dll" (
    copy "libs\sdl3\x64\SDL3.dll" "build\" >nul
)

endlocal