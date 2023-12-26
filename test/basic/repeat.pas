program RepeatUntil;



var 
    a: integer;
begin 
    repeat 
        writeln('should only be 1 of this');
    until 1 = 1;

    writeln('ok, next');
    repeat 
        writeln('a: ', a);
        a := a + 1;
    until a = 10;
end.



