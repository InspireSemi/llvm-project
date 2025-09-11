/**
 * @file unit_test_framework.h
 * @brief Unit test framework declarations using common test framework
 */

#pragma once

#include "test_common.h"
#include "unit_test_declarations.h"

/**
 * @brief Run all unit tests
 * @return 0 if all tests passed, 1 if any tests failed (standard exit codes)
 */
int run_all_unit_tests(void);
