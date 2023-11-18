


function f(x, y: int32): int32;
begin
    Exit(x + y);
end;


var 
    a, b: int32;
begin
    b := 2;
    a := 10;
    a := f(a, b);
end.

