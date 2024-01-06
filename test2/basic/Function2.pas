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
    writeln(Loc);
    Loc := FnLocal(1, 2);
    writeln(Loc);
end;

begin main end.


