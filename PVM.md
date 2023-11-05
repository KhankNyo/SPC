#1 Notation:
 - [b..a]: bits from index a to index b of an unsigned number

#2 PVM Opcode Format:
 - U32 Opcode:     [31..28][27..22][21..16][15..10][ 9..4 ][ 3..0 ]
 - Resv:           [ 0000 ][                 TODO                 ]
 - DataIns:        [ 0001 ][ Ins  ][  Rd  ][  S0  ][  S1  ][ 0000 ]
 - BranchIf:       [ 0010 ][  Cc  ][  Ra  ][  Rb  ][     Imm10    ]
 - ImmRd:          [ 0011 ][ Ins  ][  Rd  ][         Imm16        ]

#3 Instructions:

