program Subroutine;



{ 
    should be a compile error
    procedure ProcWithoutParam: int32;
}
procedure ProcWithoutParam;
begin end;
procedure ProcWithParam(a, b: integer);
begin a := a + b end;
function FuncWithoutParam(): integer;
begin Exit(5) end;
function FuncWithParam(a: integer; b: integer): integer;
begin Exit(a + b) end;


var 
    ret: integer;
begin 
    ProcWithoutParam;
    {
        should be a compile error
        ProcWithParam(1);
    }
    ProcWithParam(1, 2);

    FuncWithoutParam;
    FuncWithParam(1, 2);

    ret := FuncWithoutParam;
    ret := FuncWithParam(1, 2)
end.
