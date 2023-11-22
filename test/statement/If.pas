program IfStmt;

var
    a, b: integer;

begin 
    a := 10;
    b := 12;
    if a > b then
        a := 2*b
    else if a < b then
        a := b
    else if a = b then 
        a := 0;
end.

