/*************************************************************************\
* Copyright (c) 2026  Jerzy Jamroz
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "dbAccess.h"
#include "errlog.h"
#include "dbTest.h"
#include "epicsStdio.h"
#include "epicsMath.h"
#include "epicsStdlib.h"

// #ifdef __rtems__
// static long long raw_llround(double x)
// {
//     return (x >= 0.0)
//         ? (long long)floor(x + 0.5)
//         : -(long long)floor(-x + 0.5);
// }
// #define llround raw_llround
// #endif

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static double dbpr_double(const char* pv)
{
    DBADDR addr;
    long st = dbNameToAddr(pv, &addr);
    if (st) testAbort("dbNameToAddr(%s) failed (%ld)", pv, st);

    double val = 0.0;
    long nReq = 1;
    st = dbGetField(&addr, DBR_DOUBLE, &val, NULL, &nReq, NULL);
    if (st) testAbort("dbGetField(%s) failed (%ld)", pv, st);

    return val;
}

static void dbpf_double(const char* pv, double v)
{
    char str[64];
    epicsSnprintf(str, sizeof(str), "%.17g", v);

    long st = dbpf(pv, str);
    if (st) testAbort("dbpf(%s,%s) failed (%ld)", pv, str, st);
}

static void test_expected(const char* pv, double expected)
{
    double actual = dbpr_double(pv);

    long long  ai = llround(actual * 1e6);
    long long  ei = llround(expected * 1e6);

    testOk(ai == ei, "%s: actual=%.17g expected=%.17g", pv, actual, expected);
}

static void test_calc_expression(const char* expr, double a, double b, double expected)
{
    testDiag("CALC=%s A=%.17g B=%.17g VAL=%.17g", expr, a, b, expected);

    dbpf_double("srcA.VAL", a);
    dbpf_double("srcB.VAL", b);

    dbpf("calc.CALC", expr);
    dbpf("calcout.CALC", expr);

    test_expected("calc.VAL", expected);
    test_expected("calcout.VAL", expected);
}

static void test_arithmetic_operators()
{
    testDiag("===================");
    testDiag("Arithmetic Operators");
    testDiag("===================");
    testDiag("Addition (+)");
    test_calc_expression("A+B",  7, 5, 7+5);
    test_calc_expression("A+B",  7.2, 5.3, 7.2+5.3);
    testDiag("Subtraction (-)");
    test_calc_expression("A-B",  7, 5, 7-5);
    test_calc_expression("A-B",  7.5, 5.25, 7.5-5.25);
    testDiag("Multiplication (*)");
    test_calc_expression("A*B",  7, 5, 7*5);
    test_calc_expression("A*B",  7.3, 5.5, 7.3*5.5);
    testDiag("Division (/)");
    test_calc_expression("A/B",  7, 2, 7.0/2);
    test_calc_expression("A/B",  7.8, 2, 7.8/2);
    testDiag("Modulo (%%)");
    test_calc_expression("A%B",  7, 5,  7%5);
    test_calc_expression("A%B",  1.2, 10,  1);
    testDiag("Exponential (^)");
    test_calc_expression("A^B",  7, 5,  pow(7,5));
    test_calc_expression("A^B",  1.25, 3,  pow(1.25,3));
    testDiag("Exponential (**)");
    test_calc_expression("A**B",  7, 5,  pow(7,5));
    test_calc_expression("A**B",  1.25, 3,  pow(1.25,3));
}

static void test_algebraic_functions()
{
    testDiag("===================");
    testDiag("Algebraic Functions");
    testDiag("===================");
    testDiag("Absolute value (ABS)");
    test_calc_expression("ABS(A-B)",  5, 7, abs(5-7));
    test_calc_expression("ABS(A)",  -5, 0, abs(-5));
    test_calc_expression("ABS(A)",  5, 0, abs(5));
    testDiag("Exponential function (EXP)");
    test_calc_expression("EXP(A)",  2, 0, exp(2.0));
    test_calc_expression("EXP(A)",  5, 0, exp(5.0));
    testDiag("Floating point modulo (FMOD)");
    test_calc_expression("FMOD(A,B)",  1.2, 10, fmod(1.2,10.0));
    testDiag("Natural log (LN)");
    test_calc_expression("LN(A)",  1.2, 0, log(1.2));
    testDiag("Log base 10 (LOG)");
    test_calc_expression("LOG(A)",  1.2, 0, log10(1.2));
    testDiag("Natural log (LOGE)");
    test_calc_expression("LOGE(A)",  1.2, 0, log(1.2));
    testDiag("Minimum (MIN)");
    test_calc_expression("MIN(A)",  1.2, 0, 1.2);
    test_calc_expression("MIN(A,B)",  1.2, 0.5, fmin(1.2,0.5));
    testDiag("Maximum (MAX)");
    test_calc_expression("MAX(A)",  1.2, 0, 1.2);
    test_calc_expression("MAX(A,B)",  1.2, 0.5, fmax(1.2,0.5));
    testDiag("Square root (SQR)");
    test_calc_expression("SQR(A)",  1.2, 0, sqrt(1.2));
    test_calc_expression("SQR(A+B)",  20, 5, sqrt(25));
    testDiag("Square root (SQRT)");
    test_calc_expression("SQRT(A)",  1.2, 0, sqrt(1.2));
    test_calc_expression("SQRT(A+B)",  20, 5, sqrt(25));
}

MAIN(calcTest)
{
    testPlan(62);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("calcTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    test_arithmetic_operators();
    test_algebraic_functions();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
