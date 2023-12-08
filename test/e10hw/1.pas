program p1;
{
    E10 hw 3 problem 1:
        int a, b, c;
        a = 5;
        b = 2;
        c = a * b;

        a = c*2;
        b = a/4;
        c = a - b;
        if (c - b < a)
            a = 5;
            b = c;
        else
            a = 6;
            b = 0;
}

var 
    // R0 = a, R1 = b, R2 = c
    a, b, c: integer;
begin
    a := 5;
    b := 2;
    c := a * b;

    a := c * 2;
    b := a / 4;
    c := a - b;

    if a - b < a then 
    begin 
        a := 5;
        b := c;
    end
    else 
    begin
        a := 6;
        b := 0;
    end;
    writeln('A: ', a, '; B: ', b, '; C: ', c);
end.
