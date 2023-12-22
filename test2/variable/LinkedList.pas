program Linked;

type List = record
    Data: Integer;
    Next: ^List;
end;
type PList = ^List;



var _Head: List;
var Head: PList = nil;

function ListInit(Data, a: Integer): PList;
begin 
    Head := @_Head;
    _Head.Data := Data;
    exit(Head);
end;


var SomeList: PList;
var Deref: List;
begin 
    SomeList := ListInit(2, 1);
    Deref := SomeList^;
    _Head := Deref;
    writeln(Deref.Data);
end.

