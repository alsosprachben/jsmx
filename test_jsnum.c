#include <assert.h>
#include <math.h>
#include <string.h>

#include "jsnum.h"

static void assert_text(double number, const char *expected)
{
	char buf[64];
	size_t len = 0;

	assert(jsnum_format(number, buf, sizeof(buf), &len) == 0);
	assert(len == strlen(expected));
	assert(strcmp(buf, expected) == 0);
}

static void assert_parse_json_fail(const char *text)
{
	double number = 0.0;

	assert(jsnum_parse_json((const uint8_t *)text, strlen(text), &number) < 0);
}

static void test_parse_json(void)
{
	double number = 0.0;

	assert(jsnum_parse_json((const uint8_t *)"1", 1, &number) == 0);
	assert(number == 1.0);
	assert(jsnum_parse_json((const uint8_t *)"-12.5", 5, &number) == 0);
	assert(number == -12.5);
	assert(jsnum_parse_json((const uint8_t *)"1e2", 3, &number) == 0);
	assert(number == 100.0);

	assert_parse_json_fail("+1");
	assert_parse_json_fail("01");
	assert_parse_json_fail(".5");
	assert_parse_json_fail("1.");
	assert_parse_json_fail("Infinity");
}

static void test_parse_string(void)
{
	double number = 0.0;

	assert(jsnum_parse_string((const uint8_t *)"", 0, &number) == 0);
	assert(number == 0.0);
	assert(jsnum_parse_string((const uint8_t *)"   ", 3, &number) == 0);
	assert(number == 0.0);
	assert(jsnum_parse_string((const uint8_t *)"  +1.5 ", 7, &number) == 0);
	assert(number == 1.5);
	assert(jsnum_parse_string((const uint8_t *)".5", 2, &number) == 0);
	assert(number == 0.5);
	assert(jsnum_parse_string((const uint8_t *)"1.", 2, &number) == 0);
	assert(number == 1.0);
	assert(jsnum_parse_string((const uint8_t *)"Infinity", 8, &number) == 0);
	assert(number == INFINITY);
	assert(jsnum_parse_string((const uint8_t *)"+Infinity", 9, &number) == 0);
	assert(number == INFINITY);
	assert(jsnum_parse_string((const uint8_t *)"-Infinity", 9, &number) == 0);
	assert(number == -INFINITY);
	assert(jsnum_parse_string((const uint8_t *)"INFINITY", 8, &number) == 0);
	assert(isnan(number));
	assert(jsnum_parse_string((const uint8_t *)"0x10", 4, &number) == 0);
	assert(isnan(number));
}

static void test_format(void)
{
	assert_text(NAN, "NaN");
	assert_text(INFINITY, "Infinity");
	assert_text(-INFINITY, "-Infinity");
	assert_text(0.0, "0");
	assert_text(-0.0, "-0");
	assert_text(1.0, "1");
	assert_text(-1.0, "-1");
	assert_text(3.5, "3.5");
	assert_text(0.1, "0.10000000000000001");
	assert_text(1000000.0, "1000000");
	assert_text(1e20, "1e+20");
}

static void test_remainder(void)
{
	double value;

	value = jsnum_remainder(1.0, 1.0);
	assert(value == 0.0);
	assert(signbit(value) == 0);

	value = jsnum_remainder(-1.0, 1.0);
	assert(value == 0.0);
	assert(signbit(value) != 0);

	value = jsnum_remainder(5.5, 2.0);
	assert(value == 1.5);

	value = jsnum_remainder(1.0, 0.0);
	assert(isnan(value));

	value = jsnum_remainder(INFINITY, 1.0);
	assert(isnan(value));

	value = jsnum_remainder(1.0, INFINITY);
	assert(value == 1.0);
}

int main(void)
{
	test_parse_json();
	test_parse_string();
	test_format();
	test_remainder();
	return 0;
}
