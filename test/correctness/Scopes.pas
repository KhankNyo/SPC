program Scope;




procedure caller(a, b, c, d: integer); 
begin 
    writeln('caller: ', a, b, c, d);
end;

procedure caller2(a, b: integer);
begin
    writeln('caller 2: ', a, b);
end;


procedure main;
begin 
    caller(1, 2, 3, 4);
    caller2(1, 2);
end;

begin main end.
