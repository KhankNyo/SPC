program Funct;



function FnParam(a, b, c, d, e: integer): integer;
begin exit(a + b + c + d + e); end;

function FnLocal(a, b: integer): integer;
var LocA, LocB: integer;
begin 
    LocA := a;
    LocB := b;
    exit(LocA * Locb * a * b);
end;


procedure main;
var Loc: integer;
begin
    Loc := FnParam(1, 2, 3, 4, 5);
    if Loc <> 1+2+3+4+5 then writeln('failed stack arg')
    else writeln('passed');

    Loc := FnLocal(1, 2);
    if 1*2*1*2 <> Loc 
    then writeln('failed fn local')
    else writeln('passed');
end;

begin main end.


