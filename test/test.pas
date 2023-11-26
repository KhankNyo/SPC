program Test;


var 
    Global, SomeVar: integer;

function fun: integer;
begin
    Global := Global + 1;
    exit(Global);
end;

procedure die; begin die end;


begin 
    Global := 0;
    { Global = 1 }
    SomeVar := fun;
    { 1 * 2 + 3 * 4 = }
    SomeVar := SomeVar * fun + fun * fun;
    if SomeVar <> 1 * 2 + 3 * 4 then begin
        SomeVar := SomeVar;
        die
    end;
end.
