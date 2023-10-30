; RUN: %opt_aflgo_linker -passes='instrument-linker-aflgo' -S %s | %FileCheck %s

; ModuleID = 'test.c'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-redhat-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @target1() #0 !dbg !8 {
; CHECK: @target1
; CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 0)
; CHECK-NEXT: call void @__aflgo_trace_bb_target(i32 0)
  call void @__aflgo_trace_bb_target(i32 0)
  ret void, !dbg !12
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @target2() #0 !dbg !13 {
; CHECK: @target2
; CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 0)
; CHECK-NEXT: call void @__aflgo_trace_bb_target(i32 1)
  call void @__aflgo_trace_bb_target(i32 0)
  ret void, !dbg !14
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @caller1() #0 !dbg !15 {
; CHECK: @caller1
; CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 10000)
  call void @target1(), !dbg !16
  ret void, !dbg !17
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @caller2() #0 !dbg !18 {
; CHECK: @caller2
; CHECK-NEXT: call void @__aflgo_trace_bb_distance(i64 10000)
  call void @caller1(), !dbg !19
  call void @target2(), !dbg !20
  ret void, !dbg !21
}

declare void @__aflgo_trace_bb_target(i32)

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6}
!llvm.ident = !{!7}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 15.0.7 (Fedora 15.0.7-2.fc37)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.c", directory: "/home/egeretto/Downloads/ir_test")
!2 = !{i32 7, !"Dwarf Version", i32 4}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"uwtable", i32 2}
!6 = !{i32 7, !"frame-pointer", i32 2}
!7 = !{!"clang version 15.0.7 (Fedora 15.0.7-2.fc37)"}
!8 = distinct !DISubprogram(name: "target1", scope: !1, file: !1, line: 1, type: !9, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !11)
!9 = !DISubroutineType(types: !10)
!10 = !{null}
!11 = !{}
!12 = !DILocation(line: 1, column: 21, scope: !8)
!13 = distinct !DISubprogram(name: "target2", scope: !1, file: !1, line: 2, type: !9, scopeLine: 2, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !11)
!14 = !DILocation(line: 2, column: 21, scope: !13)
!15 = distinct !DISubprogram(name: "caller1", scope: !1, file: !1, line: 3, type: !9, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !11)
!16 = !DILocation(line: 3, column: 22, scope: !15)
!17 = !DILocation(line: 3, column: 33, scope: !15)
!18 = distinct !DISubprogram(name: "caller2", scope: !1, file: !1, line: 4, type: !9, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !11)
!19 = !DILocation(line: 5, column: 3, scope: !18)
!20 = !DILocation(line: 6, column: 3, scope: !18)
!21 = !DILocation(line: 7, column: 1, scope: !18)
