program Chained;

type 
r1 = record
    a, b: integer;
end;
r2 = record
    fa, fb: real;
end;

function mk1(a, b: integer): r1;
var ret: r1;
begin
    ret.a := a;
    ret.b := b;
    exit(ret);
end;

function mk2(uno: r1): r2;
var ret: r2;
begin
    ret.fa := uno.a;
    ret.fb := uno.b;
    exit(ret);
end;

procedure Fail(FailCode: integer);
begin
    writeln('failed: ', FailCode);
end;

procedure main;
var uno: r1;
    dos: r2;
begin
    uno := mk1(1, 2);
    if (uno.a <> 1) or (uno.b <> 2) 
    then Fail(1);

    dos := mk2(uno);
    if (dos.fa <> uno.a) or (dos.fb <> uno.b) 
    then Fail(2);

    dos := mk2(mk1(3, 4));
    if (dos.fa <> 3) or (dos.fb <> 4) 
    then Fail(3);
end;

begin main end.

