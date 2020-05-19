$ErrorActionPreference = "Continue"

if (${env:ARTIFACT} -eq 1) {
    $env:RUN="artifact"
    $env:TESTS="OFF"
}
else {
    $env:RUN="test"
    $env:TESTS="ON"
}

$env:BOOST_ROOT = ${env:BOOST_ROOT_1_69_0}
mkdir build
Push-Location build

& ..\ci\actions\windows\configure.bat
if (${LastExitCode} -ne 0) {
    throw "Failed to configure or build"
}
