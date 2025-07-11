//===-- RISCVScheduleXSf.td - Scheduling Definitions XSf ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the scheduling information for SiFive extensions.
//
//===----------------------------------------------------------------------===//

multiclass LMULSchedWritesVCIX<string id>{
defm "" : LMULSchedWrites<"WriteVC_" # id>;
defm "" : LMULSchedWrites<"WriteVC_V_" # id>;
}

defm "" : LMULSchedWritesVCIX<"I">;
defm "" : LMULSchedWritesVCIX<"X">;
defm "" : LMULSchedWritesVCIX<"IV">;
defm "" : LMULSchedWritesVCIX<"VV">;
defm "" : LMULSchedWritesVCIX<"XV">;
defm "" : LMULSchedWritesVCIX<"IVV">;
defm "" : LMULSchedWritesVCIX<"IVW">;
defm "" : LMULSchedWritesVCIX<"VVV">;
defm "" : LMULSchedWritesVCIX<"VVW">;
defm "" : LMULSchedWritesVCIX<"XVV">;
defm "" : LMULSchedWritesVCIX<"XVW">;
foreach f = ["FPR16", "FPR32", "FPR64"] in {
  defm "" : LMULSchedWritesVCIX<f # "V">;
  defm "" : LMULSchedWritesVCIX<f # "VV">;
  defm "" : LMULSchedWritesVCIX<f # "VW">;
}

multiclass LMULWriteResVCIX<string id, list<ProcResourceKind> resources>{
defm : LMULWriteRes<"WriteVC_" # id, resources>;
defm : LMULWriteRes<"WriteVC_V_" # id, resources>;
}

multiclass UnsupportedSchedXsfvcp {
let Unsupported = true in {
defm : LMULWriteResVCIX<"I", []>;
defm : LMULWriteResVCIX<"X", []>;
defm : LMULWriteResVCIX<"IV", []>;
defm : LMULWriteResVCIX<"VV", []>;
defm : LMULWriteResVCIX<"XV", []>;
defm : LMULWriteResVCIX<"IVV", []>;
defm : LMULWriteResVCIX<"IVW", []>;
defm : LMULWriteResVCIX<"VVV", []>;
defm : LMULWriteResVCIX<"VVW", []>;
defm : LMULWriteResVCIX<"XVV", []>;
defm : LMULWriteResVCIX<"XVW", []>;
foreach f = ["FPR16", "FPR32", "FPR64"] in {
  defm : LMULWriteResVCIX<f # "V", []>;
  defm : LMULWriteResVCIX<f # "VV", []>;
  defm : LMULWriteResVCIX<f # "VW", []>;
}
}
}

defm "" : LMULSchedWritesImpl<"WriteSF_VFNRClipV", !listremove(SchedMxListW, ["M4"])>;
defm "" : LMULSchedReadsImpl<"ReadSF_VFNRClipV",   !listremove(SchedMxListW, ["M4"])>;
defm "" : LMULSchedReadsImpl<"ReadSF_VFNRClipF",   !listremove(SchedMxListW, ["M4"])>;

multiclass UnsupportedSchedXSfvfnrclipxfqf {
let Unsupported = true in {
defm : LMULWriteRes<"WriteSF_VFNRClipV", []>;
defm : LMULReadAdvance<"ReadSF_VFNRClipV", 0>;
defm : LMULReadAdvance<"ReadSF_VFNRClipF", 0>;
} // Unsupported = true
}

defm "" : LMULSchedWritesImpl<"WriteSF_VQMACC_DOD", ["M1", "M2", "M4", "M8"]>;
defm "" : LMULSchedReadsImpl<"ReadSF_VQMACC_DOD",   ["M1", "M2", "M4", "M8"]>;

multiclass UnsupportedSchedXSfvqmaccdod {
let Unsupported = true in {
defm : LMULWriteRes<"WriteSF_VQMACC_DOD", []>;
defm : LMULReadAdvance<"ReadSF_VQMACC_DOD", 0>;
} // Unsupported = true
}

defm "" : LMULSchedWritesImpl<"WriteSF_VQMACC_QOQ", ["MF2", "M1", "M2", "M4"]>;
defm "" : LMULSchedReadsImpl<"ReadSF_VQMACC_QOQ",   ["MF2", "M1", "M2", "M4"]>;

multiclass UnsupportedSchedXSfvqmaccqoq {
let Unsupported = true in {
defm : LMULWriteRes<"WriteSF_VQMACC_QOQ", []>;
defm : LMULReadAdvance<"ReadSF_VQMACC_QOQ", 0>;
} // Unsupported = true
}

defm "" : LMULSchedWritesImpl<"WriteSF_VFWMACC_QQQ", SchedMxListFW>;
defm "" : LMULSchedReadsImpl<"ReadSF_VFWMACC_QQQ",   SchedMxListFW>;

multiclass UnsupportedSchedXSfvfwmaccqqq {
let Unsupported = true in {
defm : LMULWriteRes<"WriteSF_VFWMACC_QQQ", []>;
defm : LMULReadAdvance<"ReadSF_VFWMACC_QQQ", 0>;
} // Unsupported = true
}
