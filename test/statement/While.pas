program WhileStmt;

var i : integer;
begin 
    i := 0;
    while i < 10 do 
        i := i + 1;

    // viewing register 0, r0 = 10
    i := i;
end.
