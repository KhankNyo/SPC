program FunctionPointer;


type fnptr = function(a, b: integer): integer;

function f(a, b: integer): integer;
begin
    exit(a + b);
end;

procedure main;
var fp: fnptr;
begin 
    fp := @f;
    writeln(fp(1, 2));
end;

begin main end.

