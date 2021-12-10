CC = clang
CFLAGS = -std=c17  -Wall -Wextra -march=native -g3 -Og -fno-omit-frame-pointer
LDFLAGS = -lrt -lpthreads

all: csprod csconsume

# We will add these eventually:
# -Walloca -Wcast-qual -Wconversion -Wformat=2 -Wformat-security -Wnull-dereference -Wstack-protector -Wvla -Warray-bounds -Warray-bounds-pointer-arithmetic -Wassign-enum -Wbad-function-cast -Wconditional-uninitialized -Wconversion -Wfloat-equal -Wformat-type-confusion -Widiomatic-parentheses -Wimplicit-fallthrough -Wloop-analysis -Wpointer-arith -Wshift-sign-overflow -Wshorten-64-to-32 -Wswitch-enum -Wtautological-constant-in-range-compare -Wunreachable-code-aggressive -Wthread-safety -Wthread-safety-beta -Wcomma
# -D_FORTIFY_SOURCE=2

csprod: csprod.c cpcommon.c squeue.c
	$(CC) $(CFLAGS) $^ -o $@

csconsume: csconsume.c
	$(CC) $(CFLAGS) $^ -o $@


.PHONY: clean
clean:
	rm -f $(obj) csprod
	rm -f $(obj) csconsume
	rm -rf csconsume.dSYM
	rm -rf csprod.dSYM
