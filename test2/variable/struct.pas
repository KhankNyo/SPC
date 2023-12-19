program test;


type Struct = record 
    a, b, c, d: integer;
end;


function StructRet: Struct;
var Foo: Struct;
begin 
    Foo.a := 1;
    Foo.b := 2;
    Foo.c := 3;
    Foo.d := 4;
    Exit(Foo);
end;

procedure PrintStruct(Name: String; s: Struct);
begin 
    writeln('struct: ', Name);
    writeln(' ', s.a);
    writeln(' ', s.b);
    writeln(' ', s.c);
    writeln(' ', s.d);
end;

procedure main;
var Foo: Struct;
begin 
    Foo := StructRet;
    PrintStruct('Foo', Foo);
end;


begin main end.

