program Stack;

function test: integer;
var a, b: integer;
    c: integer = 10;
begin 
    a := c; 
    b := 2*c;
    c += a + b;
    exit(c);
end;


begin writeln(test) end.

