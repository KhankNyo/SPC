program Spill;


function one: integer; begin exit(1); end;

var a: integer;
begin
    a :=   (one + (one + (one + (one
         + (one + (one + (one + (one
         + (one + (one + (one + (one
         + (one + (one + (one + (one
         + (one + (one + (one + (one))))))))))))))))))));
    if a = 4*5
    then writeln('passed')
    else writeln('failed:', a);
end.
