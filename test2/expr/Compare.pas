program cmp;


function main: Integer;
var errCode, a, b: Integer;
begin 
    a := 0; 
    b := 1;

    errCode += 1;
    if a = b then exit(errCode);
    errCode += 1;
    if not (a <> b) then exit(errCode);
    errCode += 1;
    if not (a < b) then exit(errCode); 
    errCode += 1;
    if a > b then exit(errCode);
    errCode += 1;
    if not (a <= b) then exit(errCode);
    errCode += 1;
    if a >= b then exit(errCode);

    a := b;
    errCode += 1;
    if not (a = b) then exit(errCode);
    errCode += 1;
    if a <> b then exit(errCode);
    errCode += 1;
    if not (a >= b) then exit(errCode);
    errCode += 1;
    if not (a <= b) then exit(errCode);
    exit(0);
end;



var test: Integer;
begin 

    case test of 
        1: writeln('cmp: =');
        2: writeln('cmp: <>');
        3: writeln('cmp: <');
        4: writeln('cmp: >');
        5: writeln('cmp: <=');
        6: writeln('cmp: >=');
        7: writeln('cmp: = (should be equal)');
        8: writeln('cmp: <> (should be equal)');
        7: writeln('cmp: <= (should be equal)');
        8: writeln('cmp: >= (should be equal)');
    else 
        writeln('cmp: passed');
    end;
end.
    

