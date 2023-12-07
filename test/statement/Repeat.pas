program RepeatUntilStmt;


var i, top: integer;
begin 
    top := 10;
    i := top;
    repeat 
        i := i - 1;
    until i = 0;

    top := top;
end.