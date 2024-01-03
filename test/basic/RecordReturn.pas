program RecordReturn;


type SomeStruct = record
    A, B: integer;
    C: int32;
end;

function ret: SomeStruct;
var Return: SomeStruct;
begin
    Return.A := 1;
    Return.B := 2;
    Return.C := 3;
    Exit(Return);
end;

procedure Fail(FailCode: integer);
var nilptr: ^integer = nil;
begin
    writeln('failed: ', FailCode);
    nilptr^ := 0;
end;

procedure main;
var Receiver: SomeStruct;
begin
    Receiver := ret();
    ret();

    if Receiver.A <> 1 then Fail(1);
    if Receiver.B <> 2 then Fail(2);
    if Receiver.C <> 3 then Fail(3);
end;

begin main end.

