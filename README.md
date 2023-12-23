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
- revamp 'enter' instruction to take the number of argument register to push on stack
-   NOTE: to do that, CompileArgumentList and CompileParameterList will treat param locations differently
- rewrite PascalFile
- ignoring record return value
- rewrite tests

### Features:
- const, var, and constref parameters
- char, array type, and reconsider string
- file, text type
- x86, x64, ARM backend
- different calling conventions


