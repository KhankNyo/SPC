program BasicRecord;


type Basic = record
    a, b: integer;
    c: ^Basic;
end;

procedure main;
var a: Basic;
begin
    a.a := 1;
    a.b := 2;
    a.c := @a;
end;

begin main end.

