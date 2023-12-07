


function f(x, y: int32): int32;
begin
    Exit(x + y);
end;


procedure proc;
var
    a: int32;
begin
    a := f(1 + 2, 3);
end;


begin
    proc;
end.

