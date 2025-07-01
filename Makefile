# You can put your build options here
-include config.mk

all: libjsmn.a simple_example jsondump test test_jsstr test_mnurl test_utf8 test_unicode test_collation

libjsmn.a: jsmn.o
	$(AR) rc $@ $^

jsmn.o: jsmn.c jsmn.h

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

test_jsstr: test_jsstr.c jsstr.c unicode.c unicode_db.h unicode_collation.h unicode_special_casing.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

test_mnurl: test_mnurl.c mnurl.c jsstr.c unicode.c unicode_db.h unicode_collation.h unicode_special_casing.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

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

test_unicode: test_unicode.c unicode.c unicode_case_data.h unicode_db.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

test_collation: test_collation.c unicode_collation.h
	$(CC) -g $(CFLAGS) $(LDFLAGS) $^ -o $@
	./$@

clean:
	rm -f jsmn.o jsmndom.o jsmn_test.o example/simple.o
	rm -f libjsmn.a libjsmndom.a
	rm -f simple_example
	rm -f jsondump
	rm -rf test_jsstr test_mnurl test_utf8

.PHONY: all clean test test_collation

