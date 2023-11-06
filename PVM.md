
# 1/Notation:
 - <code>[b..a]</code>: bits from index a to index b of an unsigned number

# 2/PVM Opcode Format:
 - <code>&nbsp;U32 Opcode     [31..28][27..22][21..16][15..10][ 9..4 ][ 3..0 ]</code>
 - <code>&nbsp;Resv           [ 0000 ][                 TODO                 ]</code>
 - <code>&nbsp;DataIns        [ 0001 ][ Ins  ][  Rd  ][  S0  ][  S1  ][ 0000 ]</code>
 - <code>&nbsp;BranchIf       [ 0010 ][  Cc  ][  Ra  ][  Rb  ][     Imm10    ]</code>
 - <code>&nbsp;ImmRd          [ 0011 ][ Ins  ][  Rd  ][         Imm16        ]</code>

