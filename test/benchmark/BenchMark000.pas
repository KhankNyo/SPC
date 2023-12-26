program Benchmark000;

var 
    i, top: uint32;
begin
    top := 256 * 256 * 256 * 8;
    i := 0;
    while i < top do 
        i := i + 1;
end.
