# klaus: the external 6502 functional test, wrapped as an image.
#
# Nothing here is compiled.  The test is downloaded (or taken from $KLAUS_BIN),
# its blank reset vector is replaced with its own entry point ($0400), and the
# result becomes the same flat image every other app produces.  Success is the
# test spinning at $3469, so this app uses CHECK=pc instead of CHECK=halt.

APP_NAME     := klaus
APP_KIND     := external
APP_SRCS     :=
APP_CHECK    := pc:0x3469
APP_MAX_INST := 200000000
APP_VECTOR   := 0x0400
APP_URL      := https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/bin_files/6502_functional_test.bin
APP_ASSET     = $(if $(KLAUS_BIN),$(KLAUS_BIN),$(BUILD)/6502_functional_test.bin)
