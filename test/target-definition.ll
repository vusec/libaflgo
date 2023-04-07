; RUN: echo 'test.c:1' > %t
; RUN: %opt_printer -passes='print-aflgo-target-definition' -targets=%t -disable-output 2>&1 %s | %FileCheck %s

; CHECK: function_name,target_count

; ModuleID = 'test.c'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-redhat-linux-gnu"

; CHECK: callee,1
; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @callee() #0 !dbg !8 {
  ret void, !dbg !12
}

; CHECK-NOT: caller,
; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @caller() #0 !dbg !13 {
  call void @callee(), !dbg !14
  ret void, !dbg !15
}

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
!8 = distinct !DISubprogram(name: "callee", scope: !1, file: !1, line: 1, type: !9, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !11)
!9 = !DISubroutineType(types: !10)
!10 = !{null}
!11 = !{}
!12 = !DILocation(line: 1, column: 20, scope: !8)
!13 = distinct !DISubprogram(name: "caller", scope: !1, file: !1, line: 2, type: !9, scopeLine: 2, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !11)
!14 = !DILocation(line: 2, column: 21, scope: !13)
!15 = !DILocation(line: 2, column: 31, scope: !13)
