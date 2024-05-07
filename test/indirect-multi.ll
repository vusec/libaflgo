; RUN: %opt_printer -passes='print-aflgo-function-distance' -extend-cg -disable-output 2>&1 %s | %FileCheck %s

; CHECK: function_name,distance

; ModuleID = 'indirect.c'
source_filename = "indirect.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@__const.ind_call.callbacks = private unnamed_addr constant [2 x i32 ()*] [i32 ()* @target1, i32 ()* @intermediate2], align 16

; CHECK-DAG: target1,0.00
; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @target1() #0 {
  call void @__aflgo_trace_bb_target(i32 0)
  ret i32 1, !annotation !6
}

; CHECK-DAG: target2,0.00
; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @target2() #0 {
  call void @__aflgo_trace_bb_target(i32 0)
  ret i32 2, !annotation !6
}

; CHECK-DAG: intermediate2,1.00
; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @intermediate2() #0 {
  %1 = call i32 @target2()
  ret i32 %1
}

; CHECK-DAG: indirect_caller,1.33
; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @indirect_caller(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca [2 x i32 ()*], align 16
  store i32 %0, i32* %2, align 4
  %4 = bitcast [2 x i32 ()*]* %3 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %4, i8* align 16 bitcast ([2 x i32 ()*]* @__const.ind_call.callbacks to i8*), i64 16, i1 false)
  %5 = load i32, i32* %2, align 4
  %6 = sext i32 %5 to i64
  %7 = getelementptr inbounds [2 x i32 ()*], [2 x i32 ()*]* %3, i64 0, i64 %6
  %8 = load i32 ()*, i32 ()** %7, align 8
  %9 = call i32 %8()
  ret i32 %9
}

declare void @__aflgo_trace_bb_target(i32)

; Function Attrs: argmemonly nocallback nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nocallback nofree nounwind willreturn }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 15.0.7"}
!6 = !{!"libaflgo.target"}
