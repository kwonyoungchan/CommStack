/**
 * @file main.cpp
 * @brief CommStack 단위 테스트 진입점
 * @details 모든 테스트를 실행하고, 실패가 하나라도 있으면 exit code 1을 반환합니다.
 *          CI에서 exit code로 성공/실패를 판단합니다.
 */
#include "TestFramework.hpp"

int main() {
    return RunAllTests();
}
