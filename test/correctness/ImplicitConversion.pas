program ImplConv;



{ DeadCode in r0 }
procedure Die(DeadCode: Integer); begin die(DeadCode) end;


procedure main;
var i8: int8;
    i16: int16;
    i32: int32;
    i64: int64;
    u8: uint8;
    u16: uint16;
    u32: uint32;
    u64: uint64;
begin 
    i8 := -1;
    i16 := i8;
    i32 := i8;
    i64 := i8;
    if i8 <> i16 then Die(1);
    if i8 <> i32 then Die(2);
    if i8 <> i64 then Die(3);
    
    u8 := i8;
    u16 := i8;
    u32 := i8;
    u64 := i8;
    // if u8 = i8 then Die(4); { FreePascal said this is false, so this should be an error }
    if u8 <> u16 then Die(5);
    if u8 <> u32 then Die(6);
    if u8 <> u64 then Die(7);
    if u8 <> $ff then Die(8);
end;


begin main end.

