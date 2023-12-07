program t;


procedure die; begin die; end;



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
    die;
end;


begin main end.
