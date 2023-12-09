program RecordTest;


type r = record 
    a, b: integer;
    p: ^integer;
end;

procedure Test;
var struct: r;
begin 
    struct.a := 10;
    struct.b := struct.a;
    struct.p := @a;

    writeln('values: ', struct.a,
            ', ', struct.b, 
            ', ', struct.p^
    );
end;



begin Test end.

