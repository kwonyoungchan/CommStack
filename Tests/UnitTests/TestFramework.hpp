/**
 * @file TestFramework.hpp
 * @brief 외부 의존성 없는 경량 단위 테스트 프레임워크
 * @details 외부 라이브러리 없이 표준 C++17만으로 동작합니다.
 */
#pragma once

// ── include는 반드시 파일 최상단, 함수 밖에 위치해야 함 ─────────
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <functional>

// ── 색상 코드 (ANSI) ───────────────────────────────────────────
#define CLR_GREEN  "\033[32m"
#define CLR_RED    "\033[31m"
#define CLR_YELLOW "\033[33m"
#define CLR_RESET  "\033[0m"

// ── 전역 통계 ──────────────────────────────────────────────────
static int g_totalTests  = 0;
static int g_passedTests = 0;
static int g_failedTests = 0;

// ── 테스트 케이스 등록 구조체 ──────────────────────────────────
struct TestCase {
    std::string suiteName;
    std::string testName;
    std::function<void()> fn;
};
static std::vector<TestCase> g_testCases;

struct TestRegistrar {
    TestRegistrar(const char* suite, const char* name, std::function<void()> fn) {
        g_testCases.push_back({ suite, name, fn });
    }
};

// ── 단언 매크로 ────────────────────────────────────────────────
#define EXPECT_TRUE(expr) \
    do { \
        ++g_totalTests; \
        if (!(expr)) { \
            ++g_failedTests; \
            std::cout << CLR_RED "  [FAIL] " CLR_RESET \
                      << __FILE__ << ":" << __LINE__ \
                      << "  EXPECT_TRUE(" #expr ")\n"; \
        } else { ++g_passedTests; } \
    } while(0)

#define EXPECT_FALSE(expr)  EXPECT_TRUE(!(expr))
#define EXPECT_EQ(a, b)     EXPECT_TRUE((a) == (b))
#define EXPECT_NE(a, b)     EXPECT_TRUE((a) != (b))
#define EXPECT_GT(a, b)     EXPECT_TRUE((a) >  (b))
#define EXPECT_GE(a, b)     EXPECT_TRUE((a) >= (b))

#define EXPECT_THROW(expr, exc_type) \
    do { \
        ++g_totalTests; \
        bool caught = false; \
        try { expr; } catch (const exc_type&) { caught = true; } \
        if (!caught) { \
            ++g_failedTests; \
            std::cout << CLR_RED "  [FAIL] " CLR_RESET \
                      << __FILE__ << ":" << __LINE__ \
                      << "  EXPECT_THROW(" #expr ", " #exc_type ")\n"; \
        } else { ++g_passedTests; } \
    } while(0)

// ── 테스트 등록 매크로 ─────────────────────────────────────────
#define TEST(suite, name) \
    static void suite##_##name(); \
    static TestRegistrar reg_##suite##_##name(#suite, #name, suite##_##name); \
    static void suite##_##name()

// ── 테스트 실행기 ──────────────────────────────────────────────
inline int RunAllTests() {
#ifdef _WIN32
    // Windows 콘솔 ANSI 색상 활성화
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::string currentSuite;
    for (auto& tc : g_testCases) {
        if (tc.suiteName != currentSuite) {
            currentSuite = tc.suiteName;
            std::cout << "\n" CLR_YELLOW "[ " << currentSuite << " ]" CLR_RESET "\n";
        }
        std::cout << "  " << tc.testName << " ... ";
        int failBefore = g_failedTests;
        tc.fn();
        if (g_failedTests == failBefore)
            std::cout << CLR_GREEN "PASS" CLR_RESET "\n";
        else
            std::cout << "\n";
    }

    std::cout << "\n──────────────────────────────────────\n";
    std::cout << "Results: "
              << CLR_GREEN << g_passedTests << " passed" CLR_RESET ", "
              << (g_failedTests > 0 ? CLR_RED : "") << g_failedTests << " failed" CLR_RESET
              << " / " << g_totalTests << " assertions\n";

    return g_failedTests > 0 ? 1 : 0;
}
