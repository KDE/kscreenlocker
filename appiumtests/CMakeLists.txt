# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

if(NOT BUILD_TESTING OR NOT CMAKE_SYSTEM_NAME MATCHES "Linux")
    return()
endif()
# pkg_search_module(pam_wrapper REQUIRED)

if (pam_wrapper_FOUND)
    set(PAM_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../autotests/data")
    configure_file(fallbacklocktest.py.in ${CMAKE_BINARY_DIR}/fallbacklocktest.py USE_SOURCE_PERMISSIONS @ONLY )
    add_test(
        NAME calculatortest
        COMMAND selenium-webdriver-at-spi-run ${CMAKE_BINARY_DIR}/fallbacklocktest.py
    )
set_tests_properties(calculatortest PROPERTIES TIMEOUT 300)
endif()


