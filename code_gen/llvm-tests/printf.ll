; ModuleID = 'printf.bc'
source_filename = "my_module"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

@format = private unnamed_addr constant [18 x i8] c"Hello World, %d!\0A\00"

define void @main() {
entry:
  %printf = call i32 (...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @format, i32 0, i32 0), i8 1)
  ret void
}

declare i32 @printf(...)
