# llvm-pass-sdc

InsertClonedInstPass

**Overview**

The InsertClonedInstPass is an LLVM pass that examines each basic block in a function. For each basic block, it looks at the instruction preceding its branch terminator. If this instruction is not of type void and is not in the set of excluded instructions (e.g., alloca, non-void call, load, store, compare-and-swap), it does the following:

Duplication: The pass creates a clone of this instruction and inserts it right after the original instruction.
Comparison: The pass then compares the result of the original instruction with its clone. It inserts a conditional branch based on the result of this comparison.
Error Handling: If the results of the original and cloned instructions differ (i.e., a mismatch is detected), control is transferred to a new basic block (elseBB). This block logs the error information, which includes:
The function name in which the error occurred.
The block number where the mismatch was detected.
The iteration index for the block (in case it's accessed multiple times within a loop)
The type of instruction that had a mismatch.
The values of both the original and duplicated instruction, if they are of certain types (e.g., i32, i64, double). For now, if the type isn't explicitly handled, only the mismatch is reported without the exact values.
This information is printed to the standard output using the printf function.
Continuation: If there's no mismatch detected, the execution continues as normal to the subsequent instructions.


**Usage**

clang++ -shared -o InsertClonedInstPass.so -fPIC InsertClonedInstPass.cpp `llvm-config --cxxflags`

opt -load ./InsertClonedInstPass.so -insert-cloned-inst -S < test_llvm.ll > result.ll

llvm-as result.ll

clang result.bc -o result




