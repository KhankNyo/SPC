
var 
    a, b, c: Integer;
    d, e: Integer;
begin
    a := 1;
    b := 1;
    d := 2;
    e := a * b + d;
    c := e + d;
    a := c;
    Exit(a);
end


