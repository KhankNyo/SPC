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

# Supported functions:
- write, writeln (no file parameter)
- sizeof


# TODO:
### Critical:
- 1-character long string literals are automatically treated as char, fix this
- add goto, label, and with statement
- add array, set, and union (record case) types
- reconsider string as 'array[0..255] of char' (after adding array)
- file, text type

### Features:
- for in loop
- const, var, and constref parameters
- x86, x64, ARM backend
- different calling conventions


