program ErrorTest;

{ missing parameter type name }
function MissingParamType(x, y): integer;
begin exit(x + y); end;

{ type does not exist }
function UndefinedType(x, y: ThisIsAFakeType): integer;
begin exit(x + y); end;

{ missing return type }
function MissingReturnType(x, y: integer);
begin exit(x + y); end;

function WeirdParams(x, y: integer, a, b: int32): integer;
begin exit(x + y + a + b); end;

{ no name function }
function : integer;
begin end;


{ procedure with return type }
procedure ProcWithReturn: integer;
begin end;


begin
end.

