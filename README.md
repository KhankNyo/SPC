# Shitty Pascal Compiler
- A toy Pascal Compiler I wrote for fun

# Build:
### Windows:
- Prerequisite: C99 compiler, or cl
- Build each translation unit individually:
    -     .\build.bat cl
- Or unity build to speed up compile time:
    -     .\build.bat cl unity
### Linux:
- Prerequisite: C99 compiler, preferably gcc
- Build each translation unit individually:
    -     ./build.sh gcc
- Or unity build to speed up compile time:
    -     ./build.sh gcc unity

# Usage:
- Windows: `Pascal InputFile.pas OutputFile.exe`
- Other OSes: `./Pascal InputFile.pas OutputFile`

# Supported keywords:
- None yet

# Supported functions:
- None yet

# TODO:
- range, set and array, file type, and rewrtie readln and writeln
- char type, and rewrite string aswell
- const subroutine parameters
- passing by reference
- calling functions that return a record without using the return value


