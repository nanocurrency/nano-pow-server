@echo off
set exit_code=0

echo "build %RUN%"

cmake .. ^
    -Ax64 ^
    -G"Visual Studio 16 2019" ^
    -DNANO_POW_STANDALONE=ON ^
    -DNANO_POW_SERVER_TEST=%TESTS% ^
    -DBoost_NO_SYSTEM_PATHS=TRUE ^
    -DBoost_NO_BOOST_CMAKE=TRUE 

set exit_code=%errorlevel%
if %exit_code% neq 0 goto exit

goto %RUN%

:test
cmake --build . ^
  --target tests ^
  --config Debug ^
  -- /m:2
set exit_code=%errorlevel%
if %exit_code% neq 0 goto exit
call Debug\tests.exe
set exit_code=%errorlevel%
goto exit

:artifact
cmake --build . ^
  --target PACKAGE ^
  --config Release ^
  -- /m:2
set exit_code=%errorlevel%

:exit
exit /B %exit_code%