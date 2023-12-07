
program NestedFunction;

procedure Die; 
begin 
    Die { stack overflow }
end;

function Main: integer;
    function Main1: integer;
        function Main2: integer;
        var a: integer;
        begin 
            a := 1; 
            exit(a + 1);
        end;
    var a: integer;
    begin 
        a := Main2; 
        exit(a + 1) 
    end;
var a: integer;
begin 
    exit(Main1)
end;


begin 
    { Pascal's readability > Python }
    if Main <> 3 then Die;
end.

