# You can put your build options here
-include config.mk

# libjsmx.a uses zlib for CompressionStream / DecompressionStream.
LDLIBS += -lz

all: libjsmn.a libjsmx.a simple_example jsondump test test_jsstr test_jsmethod test_jsurl test_utf8 test_unicode test_collation test_jsnum test_jscrypto test_jsval test_codegen test_compliance

LIBJSMX_OBJS = jsmn.o jsnum.o jscrypto.o jsval.o jsval_url.o jsmethod.o jsregex.o jsurl.o jsstr.o unicode.o

libjsmn.a: jsmn.o
	$(AR) rc $@ jsmn.o

libjsmx.a: $(LIBJSMX_OBJS)
	$(AR) rc $@ $(LIBJSMX_OBJS)

jsstr.o: jsstr.c jsstr.h unicode.h unicode_db.h

unicode.o: unicode.c unicode.h unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h

jsmn.o: jsmn.c jsmn.h

jsnum.o: jsnum.c jsnum.h

jscrypto.o: jscrypto.c jscrypto.h jsmx_config.h

jsval.o: jsval.c jsval.h jscrypto.h jsnum.h jsmethod.h jsmn.h jsurl.h utf8.h jsmx_config.h

jsmethod.o: jsmethod.c jsmethod.h jsnum.h jsstr.h unicode.h jsregex.h jsmx_config.h

jsregex.o: jsregex.c jsregex.h jsmx_config.h

jsurl.o: jsurl.c jsurl.h jsval.h jsstr.h unicode.h utf8.h

libjsmndom.a: jsmndom.o

jsmndom.o: jsmn.c jsmn.h
	$(CC) -DJSMN_EMITTER=1 $(CFLAGS) -c jsmn.c -o $@

test: test_default test_strict test_links test_strict_links test_emitter
test_default: test/tests.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o test/$@
	./test/$@
test_strict: test/tests.c
	$(CC) -DJSMN_STRICT=1 $(CFLAGS) $(LDFLAGS) $< -o test/$@
	./test/$@
test_links: test/tests.c
	$(CC) -DJSMN_PARENT_LINKS=1 $(CFLAGS) $(LDFLAGS) $< -o test/$@
	./test/$@
test_strict_links: test/tests.c
	$(CC) -DJSMN_STRICT=1 -DJSMN_PARENT_LINKS=1 $(CFLAGS) $(LDFLAGS) $< -o test/$@
	./test/$@
test_emitter: test/tests.c
	$(CC) -g3 -DJSMN_EMITTER=1 $(CFLAGS) $(LDFLAGS) $< -o test/$@
	./test/$@

jsmn_test.o: jsmn_test.c libjsmn.a

simple_example: example/simple.o libjsmn.a
	$(CC) $(LDFLAGS) $^ -o $@

jsondump: example/jsondump.o libjsmn.a
	$(CC) $(LDFLAGS) $^ -o $@

test_jsstr: test_jsstr.c jsstr.c unicode.c unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

test_jsmethod: test_jsmethod.c jsnum.c jsmethod.c jsregex.c jsstr.c unicode.c jsnum.h unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	./$@

test_jsurl: test_jsurl.c jsnum.c jsval.c jsmethod.c jsregex.c jsmn.c jsurl.c jsstr.c unicode.c jsnum.h jsval.h jsmethod.h jsurl.h jsstr.h unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	./$@

test_jsnum: test_jsnum.c jsnum.c jsnum.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	./$@

test_jscrypto: test_jscrypto.c jscrypto.c jscrypto.h utf8.h jsmx_config.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	./$@

test_jsregex: test_jsregex.c jsregex.c jsregex.h jsmx_config.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	./$@

test_jsval: test_jsval.c jsnum.c jscrypto.c jsval.c jsmethod.c jsregex.c jsmn.c jsurl.c jsstr.c unicode.c jsnum.h jscrypto.h jsval.h jsmethod.h jsurl.h unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	./$@

test_codegen: test_codegen.c jsnum.c jscrypto.c jsval.c jsmethod.c jsregex.c jsmn.c jsurl.c jsstr.c unicode.c jsnum.h jscrypto.h jsval.h jsmethod.h jsurl.h jsstr.h unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	./$@

test_faas: test_faas.c example/wintertc_proxy_handler.c jsnum.c jscrypto.c jsval.c jsmethod.c jsregex.c jsmn.c jsurl.c jsstr.c unicode.c jsnum.h jscrypto.h jsval.h jsmethod.h jsurl.h unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h runtime_modules/shared/faas_bridge.h
	$(CC) -g -I. $(CFLAGS) $(LDFLAGS) test_faas.c example/wintertc_proxy_handler.c jsnum.c jscrypto.c jsval.c jsmethod.c jsregex.c jsmn.c jsurl.c jsstr.c unicode.c $(LDLIBS) -o $@
	./$@

test_compliance: scripts/run_compliance_manifest.py compliance/manifest.json compliance/generated/test_contract.h
	LDLIBS="$(LDLIBS)" python3 scripts/run_compliance_manifest.py

bench: scripts/run_bench_manifest.py bench/manifest.json bench/generated/bench_util.h
	python3 scripts/run_bench_manifest.py

test_utf8: utf8.c
	$(CC) -g -DUTF8_TEST $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

UnicodeData.txt:
	curl -o $@ https://www.unicode.org/Public/UNIDATA/UnicodeData.txt

unicode_case_data.h: UnicodeData.txt scripts/gen_unicode_case.py
	python3 scripts/gen_unicode_case.py UnicodeData.txt unicode_case_data.h

unicode_db.h: UnicodeData.txt scripts/gen_unicode_db.py
	python3 scripts/gen_unicode_db.py UnicodeData.txt unicode_db.h

allkeys.txt:
	curl -o $@ https://www.unicode.org/Public/UCA/latest/allkeys.txt

unicode_collation.h: allkeys.txt scripts/gen_unicode_collation.py
	python3 scripts/gen_unicode_collation.py allkeys.txt unicode_collation.h

SpecialCastings.txt:
	curl -o $@ https://www.unicode.org/Public/UNIDATA/SpecialCasing.txt

unicode_special_casing.h: SpecialCastings.txt scripts/gen_unicode_special_casing.py
	python3 scripts/gen_unicode_special_casing.py SpecialCastings.txt unicode_special_casing.h

CompositionExclusions.txt:
	curl -o $@ https://www.unicode.org/Public/UNIDATA/CompositionExclusions.txt

unicode_exclusions.h: CompositionExclusions.txt scripts/gen_unicode_exclusions.py
	python3 scripts/gen_unicode_exclusions.py CompositionExclusions.txt unicode_exclusions.h

DerivedNormalizationProps.txt:
	curl -o $@ https://www.unicode.org/Public/UNIDATA/DerivedNormalizationProps.txt

unicode_derived_normalization_props.h: DerivedNormalizationProps.txt scripts/gen_unicode_derived_normalization_props.py
	python3 scripts/gen_unicode_derived_normalization_props.py DerivedNormalizationProps.txt unicode_derived_normalization_props.h

test_unicode: test_unicode.c unicode.c unicode_case_data.h unicode_db.h unicode_collation.h unicode_special_casing.h unicode_exclusions.h unicode_derived_normalization_props.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

test_collation: test_collation.c unicode_collation.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

clean:
	rm -f jsmn.o jsnum.o jscrypto.o jsval.o jsmethod.o jsregex.o jsurl.o jsstr.o unicode.o jsmndom.o jsmn_test.o example/simple.o example/jsondump.o
	rm -f libjsmn.a libjsmx.a libjsmndom.a
	rm -f simple_example
	rm -f jsondump
	rm -f test_jsstr test_jsmethod test_jsnum test_jscrypto test_jsregex test_jsval test_codegen test_jsurl test_utf8 test_unicode test_collation
	rm -f test/test_default test/test_strict test/test_links test/test_strict_links test/test_emitter
	rm -f test_compliance_*

.PHONY: all clean test test_collation test_compliance bench
