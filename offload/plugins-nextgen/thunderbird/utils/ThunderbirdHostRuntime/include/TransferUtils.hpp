// functions to bulk data transfer using DTE rather than mailbox slots
// host can do this when a mnemory allocation has happened in a batch that completed so it is obvious device activity in the address range isn't supposed to be ongoing
// must check against a resource map which mirrors the malloc and free history on the machine,.