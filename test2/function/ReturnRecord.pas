program ReturnRecord;


type Basic = record
    a, b: integer;
    p: ^Basic;
end;

function BasicInit(a, b: integer): Basic;
var ReturnValue: Basic;
begin
    ReturnValue.a := a;
    ReturnValue.b := b;
    ReturnValue.p := nil;
    exit(ReturnValue);
end;

procedure main;
var Receiver: Basic;
begin
    Receiver := BasicInit(1, 2);
    BasicInit(3, 4);
    BasicInit(5, 6);

    if (Receiver.a <> 1) or (Receiver.b <> 2) 
    then writeln('failed')
    else writeln('passed');
end;

begin main end.

