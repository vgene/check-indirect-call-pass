
# Check Indirect Call Pass
Check all existing indirect calls and report them as four categories
1. Calls covered by static devirt in CAF(i.e., with compare and call chain before the indirect call block)
2. Calls covered by pgo (same as 1 but with modified version pgo-icall-prom pass)
3. Calls not devirtualized but not exercised by profiling
4. Other calls (might due to external function calls that are not declared)
