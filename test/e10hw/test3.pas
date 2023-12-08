program t;

procedure main;
var x, y: integer;
begin 
    x := 0;
    y := 1;
    while x < 9 do 
    begin 
        y += x;
        x += 4;
    end;
    writeln('(x, y) = (', x, ', ', y, ')');
end;


begin main end.
