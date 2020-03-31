$ErrorActionPreference = "Continue"

if (${env:ARTIFACT} -eq 1) {
    $env:RUN="artifact"
    $env:TESTS="OFF"
}
else {
    $env:RUN="test"
    $env:TESTS="ON"
}

mkdir build
Push-Location build

& ..\ci\actions\windows\configure.bat
if (${LastExitCode} -ne 0) {
    throw "Failed to configure or build"
}
