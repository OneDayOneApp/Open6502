# hello: the M1 smoke test.  Prints a line through _putc and halts with 0.

APP_NAME     := hello
APP_KIND     := compiled
APP_SRCS     := hello.s
APP_CHECK    := halt
APP_MAX_INST := 2000000
