program t;



procedure main;
var i, control, count: integer;
begin 
    i := 1;
    control := 1;
    count := 0;
    while control = 1 do
    begin 
        i := i + 1;
        if i > 2 then
            control := 2;
        if i = 2 then
            control := 3;
        count += 1;
    end;
    writeln('i = ', i, 
            '; control = ', control,
            '; count = ', count
    );
end;

begin main end.
