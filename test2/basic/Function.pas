program Fn;


type FnPtr = function: integer;

function MyFunction: integer;
begin 
    exit(1);
end;

function MyOtherFunction: FnPtr;
begin 
    exit(@MyFunction);
end;


procedure MyProcedure(Argument: integer);
begin 
    writeln(Argument);
end;

var a: integer;
begin 
    a := 1 + MyOtherFunction()();
    MyProcedure(a);
    if 2 <> a 
    then writeln('failed')
    else writeln('passed');
end.

