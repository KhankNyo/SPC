program p2;
{
    E10 problem 2:

    int a = 1;
    int count = 0;

    while (count < 4):
        if (count < 2):
            a += 4;
        else
            a += count;
        count += 1;
}


var
    // R0 = a, R1 = count
    a, count: integer;
begin
    a := 1;
    count := 0;
    while count < 4 do 
    begin
        if count < 2 then 
            a := a + 4
        else 
            a := a + count;

        count := count + 1;
    end;
    writeln('a: ', a, '; count: ', count);
end.

