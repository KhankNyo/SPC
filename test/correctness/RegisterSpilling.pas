program spill;



procedure main;
var A, B: integer;
    Tmp: integer = 1;
begin
    A := 23;
    B := Tmp + (Tmp + (Tmp + (Tmp + (Tmp + 
        (Tmp + (Tmp + (Tmp + (Tmp + (Tmp + 
        (Tmp + (Tmp + (Tmp + (Tmp + (Tmp + 
        (Tmp + (Tmp + (Tmp + (Tmp + (Tmp + 
        (Tmp + (Tmp + (Tmp))))))))))))))))))))));

    if B = A 
    then writeln('passed')
    else writeln('failed');
end;

begin main end.
