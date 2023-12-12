program ForLoop;


var n: integer;
function stop: integer;
begin 
    n += 1;
    exit(n);
end;


procedure Main; 
begin
    n := 10;
    for n := 0 to stop() do 
    begin
        writeln(n);
    end;

    if n <> 11 
    then writeln('Failed: n = ', n)
    else writeln('Passed: n = ', n);
end;


begin Main end.

