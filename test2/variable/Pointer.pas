program BasicPointer;



procedure main;
var a: integer = 10;
    b: integer = 11;
    p: ^integer = @a;
begin 
    a += p^ + b;

    writeln(a);
    writeln(b);
    writeln(p^);
end;

begin main end.

