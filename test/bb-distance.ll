; RUN: echo 'test.c:3' > %t
; RUN: %opt_printer -passes='print-aflgo-basic-block-distance' -targets=%t -disable-output 2>&1 %s | %FileCheck %s

; CHECK: function_name,basic_block_name,distance

; ModuleID = 'test.c'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-redhat-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @callee(i32 noundef %0) #0 !dbg !8 {
; CHECK-DAG: callee,%1,1.00
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  call void @llvm.dbg.declare(metadata ptr %3, metadata !13, metadata !DIExpression()), !dbg !14
  %4 = load i32, ptr %3, align 4, !dbg !15
  %5 = icmp sgt i32 %4, 4, !dbg !17
  br i1 %5, label %6, label %11, !dbg !18

; CHECK-DAG: callee,%6,0.00
6:                                                ; preds = %1
  %7 = load i32, ptr %3, align 4, !dbg !19
  %8 = icmp sgt i32 %7, 7, !dbg !22
  br i1 %8, label %9, label %10, !dbg !23

9:                                                ; preds = %6
  store i32 1, ptr %2, align 4, !dbg !24
  br label %16, !dbg !24

10:                                               ; preds = %6
  store i32 2, ptr %2, align 4, !dbg !26
  br label %16, !dbg !26

11:                                               ; preds = %1
  %12 = load i32, ptr %3, align 4, !dbg !28
  %13 = icmp slt i32 %12, 2, !dbg !31
  br i1 %13, label %14, label %15, !dbg !32

14:                                               ; preds = %11
  store i32 3, ptr %2, align 4, !dbg !33
  br label %16, !dbg !33

15:                                               ; preds = %11
  store i32 4, ptr %2, align 4, !dbg !35
  br label %16, !dbg !35

16:                                               ; preds = %15, %14, %10, %9
  %17 = load i32, ptr %2, align 4, !dbg !37
  ret i32 %17, !dbg !37
}

; Function Attrs: nocallback nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @caller(i32 noundef %0) #0 !dbg !38 {
; CHECK-DAG: caller,%1,12.00
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  call void @llvm.dbg.declare(metadata ptr %3, metadata !39, metadata !DIExpression()), !dbg !40
  %4 = load i32, ptr %3, align 4, !dbg !41
  %5 = icmp sgt i32 %4, 3, !dbg !43
  br i1 %5, label %6, label %12, !dbg !44

; CHECK-DAG: caller,%6,11.00
6:                                                ; preds = %1
  %7 = load i32, ptr %3, align 4, !dbg !45
  %8 = icmp sgt i32 %7, 5, !dbg !48
  br i1 %8, label %9, label %11, !dbg !49

; CHECK-DAG: caller,%9,10.00
9:                                                ; preds = %6
  %10 = call i32 @callee(i32 noundef 7), !dbg !50
  store i32 %10, ptr %2, align 4, !dbg !52
  br label %13, !dbg !52

11:                                               ; preds = %6
  store i32 3, ptr %2, align 4, !dbg !53
  br label %13, !dbg !53

12:                                               ; preds = %1
  store i32 2, ptr %2, align 4, !dbg !55
  br label %13, !dbg !55

13:                                               ; preds = %12, %11, %9
  %14 = load i32, ptr %2, align 4, !dbg !57
  ret i32 %14, !dbg !57
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind readnone speculatable willreturn }

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
!8 = distinct !DISubprogram(name: "callee", scope: !1, file: !1, line: 1, type: !9, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !12)
!9 = !DISubroutineType(types: !10)
!10 = !{!11, !11}
!11 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !{}
!13 = !DILocalVariable(name: "n", arg: 1, scope: !8, file: !1, line: 1, type: !11)
!14 = !DILocation(line: 1, column: 16, scope: !8)
!15 = !DILocation(line: 2, column: 7, scope: !16)
!16 = distinct !DILexicalBlock(scope: !8, file: !1, line: 2, column: 7)
!17 = !DILocation(line: 2, column: 9, scope: !16)
!18 = !DILocation(line: 2, column: 7, scope: !8)
!19 = !DILocation(line: 3, column: 9, scope: !20)
!20 = distinct !DILexicalBlock(scope: !21, file: !1, line: 3, column: 9)
!21 = distinct !DILexicalBlock(scope: !16, file: !1, line: 2, column: 14)
!22 = !DILocation(line: 3, column: 11, scope: !20)
!23 = !DILocation(line: 3, column: 9, scope: !21)
!24 = !DILocation(line: 4, column: 7, scope: !25)
!25 = distinct !DILexicalBlock(scope: !20, file: !1, line: 3, column: 16)
!26 = !DILocation(line: 6, column: 7, scope: !27)
!27 = distinct !DILexicalBlock(scope: !20, file: !1, line: 5, column: 12)
!28 = !DILocation(line: 9, column: 9, scope: !29)
!29 = distinct !DILexicalBlock(scope: !30, file: !1, line: 9, column: 9)
!30 = distinct !DILexicalBlock(scope: !16, file: !1, line: 8, column: 10)
!31 = !DILocation(line: 9, column: 11, scope: !29)
!32 = !DILocation(line: 9, column: 9, scope: !30)
!33 = !DILocation(line: 10, column: 7, scope: !34)
!34 = distinct !DILexicalBlock(scope: !29, file: !1, line: 9, column: 16)
!35 = !DILocation(line: 12, column: 7, scope: !36)
!36 = distinct !DILexicalBlock(scope: !29, file: !1, line: 11, column: 12)
!37 = !DILocation(line: 15, column: 1, scope: !8)
!38 = distinct !DISubprogram(name: "caller", scope: !1, file: !1, line: 17, type: !9, scopeLine: 17, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !12)
!39 = !DILocalVariable(name: "n", arg: 1, scope: !38, file: !1, line: 17, type: !11)
!40 = !DILocation(line: 17, column: 16, scope: !38)
!41 = !DILocation(line: 18, column: 7, scope: !42)
!42 = distinct !DILexicalBlock(scope: !38, file: !1, line: 18, column: 7)
!43 = !DILocation(line: 18, column: 9, scope: !42)
!44 = !DILocation(line: 18, column: 7, scope: !38)
!45 = !DILocation(line: 19, column: 9, scope: !46)
!46 = distinct !DILexicalBlock(scope: !47, file: !1, line: 19, column: 9)
!47 = distinct !DILexicalBlock(scope: !42, file: !1, line: 18, column: 14)
!48 = !DILocation(line: 19, column: 11, scope: !46)
!49 = !DILocation(line: 19, column: 9, scope: !47)
!50 = !DILocation(line: 20, column: 12, scope: !51)
!51 = distinct !DILexicalBlock(scope: !46, file: !1, line: 19, column: 16)
!52 = !DILocation(line: 20, column: 5, scope: !51)
!53 = !DILocation(line: 22, column: 7, scope: !54)
!54 = distinct !DILexicalBlock(scope: !46, file: !1, line: 21, column: 12)
!55 = !DILocation(line: 25, column: 5, scope: !56)
!56 = distinct !DILexicalBlock(scope: !42, file: !1, line: 24, column: 10)
!57 = !DILocation(line: 27, column: 1, scope: !38)
