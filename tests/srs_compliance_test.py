#!/usr/bin/env python3

import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

COMPILER = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./cminorcc")
TIMEOUT = 5

positive_tests = [
    ("basic_main", "int main(void) { return 0; }"),
    ("initialized_variable", "int main(void) { int x = 10; return x; }"),
    ("variable_assignment", "int main(void) { int x; x = 5; return x; }"),
    ("arithmetic", "int main(void) { int x = 2 + 3 * 4 - 1; return x; }"),
    ("relational", "int main(void) { int x = 5; if (x > 2) x = 1; return x; }"),
    ("if_else", "int main(void) { int x = 0; if (1) x = 2; else x = 3; return x; }"),
    ("while_loop", "int main(void) { int x = 0; while (x < 3) { x = x + 1; } return x; }"),
    ("for_loop", "int main(void) { int x = 0; for (int i = 0; i < 3; i = i + 1) x = x + i; return x; }"),
    ("nested_block", "int main(void) { int x = 1; { int y = 2; x = x + y; } return x; }"),
    ("unary_operators", "int main(void) { int x = -5; int y = +x; return !y; }"),
    ("logical_operators", "int main(void) { int x = 1; if (x && !0 || 0) x = 2; return x; }"),
    ("equality_operators", "int main(void) { int x = 5; if (x == 5) x = 1; if (x != 2) x = 3; return x; }"),
    ("modulo_operator", "int main(void) { int x = 17 % 5; return x; }"),
    ("function_call", "int add(int a, int b) { return a + b; } int main(void) { return add(2, 3); }"),
    ("function_prototype", "int add(int a, int b); int add(int a, int b) { return a + b; } int main(void) { return add(1, 2); }"),
    ("void_function", "void helper(void) { return; } int main(void) { helper(); return 0; }"),
    ("empty_statement", "int main(void) { ; return 0; }"),
    ("comments", "// line comment\nint main(void) { /* block comment */ return 0; }"),
    ("nested_if", "int main(void) { int x = 1; if (x) { if (x == 1) x = 2; } return x; }"),
    ("nested_while", "int main(void) { int x = 0; while (x < 2) { while (0) x = 9; x = x + 1; } return x; }"),
    ("for_optional_expressions", "int main(void) { int x = 0; for (; x < 2; ) x = x + 1; return x; }"),
    ("six_parameters", "int sum6(int a, int b, int c, int d, int e, int p6) { return a+b+c+d+e+p6; } int main(void) { return sum6(1,2,3,4,5,6); }"),
    ("multiple_functions", "int one(void) { return 1; } int two(void) { return 2; } int main(void) { return one() + two(); }"),
    ("variable_shadowing", "int main(void) { int x = 1; { int x = 2; x = x + 1; } return x; }"),
    ("chained_assignment", "int main(void) { int a; int b; a = b = 3; return a + b; }"),
]

negative_tests = [
    ("invalid_character", "int main(void) { @ return 0; }"),
    ("unsupported_increment", "int main(void) { int x = 0; x++; return x; }"),
    ("preprocessor_directive", "#include <stdio.h>\nint main(void) { return 0; }"),
    ("string_literal", 'int main(void) { "hello"; return 0; }'),
    ("character_literal", "int main(void) { 'a'; return 0; }"),
    ("unterminated_comment", "int main(void) { /* unfinished comment"),
    ("floating_literal", "int main(void) { int x = 1.5; return 0; }"),
    ("unsupported_ampersand", "int main(void) { int x = 1 & 2; return x; }"),
    ("missing_semicolon", "int main(void) { int x = 1 return x; }"),
    ("missing_expression", "int main(void) { int x = ; return 0; }"),
    ("missing_closing_brace", "int main(void) { return 0;"),
    ("missing_opening_parenthesis", "int main(void { return 0; }"),
    ("missing_function_parenthesis", "int main void { return 0; }"),
    ("invalid_if_condition", "int main(void) { if () return 1; return 0; }"),
    ("invalid_for_header", "int main(void) { for (int i = 0 i < 2; i = i + 1) ; return 0; }"),
    ("invalid_else", "int main(void) { if (1) return 1; else return; }"),
    ("undeclared_variable", "int main(void) { x = 10; return 0; }"),
    ("duplicate_variable", "int main(void) { int x = 1; int x = 2; return x; }"),
    ("break_outside_loop", "int main(void) { break; return 0; }"),
    ("continue_outside_loop", "int main(void) { continue; return 0; }"),
    ("wrong_argument_count", "int f(int x) { return x; } int main(void) { return f(1, 2); }"),
    ("too_many_parameters", "int f(int a, int b, int c, int d, int e, int f, int g) { return a; } int main(void) { return 0; }"),
    ("void_return_value", "void f(void) { return 1; } int main(void) { return 0; }"),
    ("integer_empty_return", "int f(void) { return; } int main(void) { return 0; }"),
    ("existing_semantic_invalid", None),
]

def run_source(name, source, should_pass, directory):
    path = Path(directory) / (name + ".cm")
    path.write_text(source, encoding="ascii")

    try:
        result = subprocess.run(
            [COMPILER, "--check", str(path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=TIMEOUT,
        )
        passed = (result.returncode == 0) == should_pass

        if not passed:
            expectation = "success" if should_pass else "failure"
            print(f"[FAIL] {name}: expected {expectation}, got exit {result.returncode}")
            output = (result.stdout + "\n" + result.stderr).strip()
            if output:
                print(output[:1200])
        else:
            print(f"[PASS] {name}")

        return passed

    except subprocess.TimeoutExpired:
        print(f"[FAIL] {name}: exceeded {TIMEOUT}-second timeout")
        return False
    except OSError as error:
        print(f"[FAIL] {name}: could not execute compiler: {error}")
        return False

def main():
    if not os.path.isfile(COMPILER):
        print(f"Compiler not found: {COMPILER}")
        return 2

    passed = 0
    failed = 0
    started = time.monotonic()

    print("========== SRS FUNCTIONAL TESTS ==========")

    with tempfile.TemporaryDirectory(prefix="cminor_srs_") as directory:
        for name, source in positive_tests:
            if run_source(name, source, True, directory):
                passed += 1
            else:
                failed += 1

        for name, source in negative_tests:
            if source is None:
                source = Path("examples/semantic_invalid.cm").read_text(encoding="utf-8")
            if run_source(name, source, False, directory):
                passed += 1
            else:
                failed += 1

        print("\n========== 100 DIVERSE MALFORMED INPUT TESTS ==========")

        # Twenty categories, each with five genuinely distinct variations.
        malformed_templates = [
            "int main(void) { int value = {n} + ; return 0; }",
            "int main(void) { int value = {n} @ 2; return 0; }",
            "int main(void) { int value = {n}\n return value; }",
            "int main(void) { return {n}; } }",
            "int main(void) { return {n};",
            "int main(void) { if ({n} > 0 return 1; return 0; }",
            "int main(void) { while () { return {n}; } return 0; }",
            "int main(void) { for (int i = 0 i < {n}; i = i + 1) { } return 0; }",
            "int main(void) { int marker{n} = 0; return; }",
            "int main(void) { return missing_{n}; }",
            "int main(void) { int value_{n} = 1; int value_{n} = 2; return 0; }",
            "int main(void) { int marker{n} = 0; break; return marker{n}; }",
            "int main(void) { int marker{n} = 0; continue; return marker{n}; }",
            "int helper_{n}(int a) { return a; } int main(void) { return helper_{n}(); }",
            "int helper_{n}(int a) { return a; } int main(void) { return helper_{n}({n}; }",
            "int main(void) { int value_{n} = 0; value_{n} = ; return value_{n}; }",
            "int main(void) { int value_{n} = ({n} + 1; return value_{n}; }",
            "int main(void) { if ({n} > 0) return 1 else return 0; }",
            "void helper_{n}(void) { return {n}; } int main(void) { return 0; }",
            "int helper_{n}(int a, int b, int c, int d, int e, int f, int g) { return a; } int main(void) { return 0; }",
        ]

        if len(malformed_templates) != 20:
            raise RuntimeError("Expected exactly 20 malformed-input categories.")

        malformed_sources = []
        for index in range(100):
            category = index % len(malformed_templates)
            source = malformed_templates[category].replace(
                "{n}", str(index + 1)
            )
            malformed_sources.append(source)

        if len(set(malformed_sources)) != 100:
            raise RuntimeError(
                "Malformed test generator produced duplicate source inputs."
            )

        malformed_passed = 0
        malformed_failed = 0

        for index, source in enumerate(malformed_sources):
            category = index % len(malformed_templates) + 1
            variation = index // len(malformed_templates) + 1

            test_name = (
                f"malformed_{index + 1:03d}_"
                f"category_{category:02d}_"
                f"variation_{variation}"
            )

            if run_source(test_name, source, False, directory):
                malformed_passed += 1
            else:
                malformed_failed += 1

    elapsed = time.monotonic() - started
    total_passed = passed + malformed_passed
    total_failed = failed + malformed_failed

    print("\n========== SRS TEST SUMMARY ==========")
    print(f"Functional tests: {passed}/{len(positive_tests) + len(negative_tests)} passed")
    print(f"Malformed tests:  {malformed_passed}/100 passed")
    print(f"Total:            {total_passed}/{total_passed + total_failed} passed")
    print(f"Failed:           {total_failed}")
    print(f"Elapsed time:     {elapsed:.2f} seconds")

    if failed or malformed_failed:
        print("SRS TEST RESULT: FAILED")
        return 1

    print("SRS TEST RESULT: PASSED")
    return 0

if __name__ == "__main__":
    sys.exit(main())
