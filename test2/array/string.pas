
program StringIndexing;

var s: string = 'Hello, world';
begin
    s[1] := 'a';
    if 'aello, world' = s 
    then writeln('passed')
    else writeln('failed');
end.
