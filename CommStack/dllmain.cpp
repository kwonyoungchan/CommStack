/**
 * @file dllmain.cpp
 * @brief CommStack.dll의 Windows 진입점(Entry Point) 구현부
 * @details 프로세스 및 스레드의 DLL 연결/해제 이벤트를 처리하며, 로더 락(Loader Lock) 방지를 위해 최소한의 로직만 포함합니다.
 */
#include "pch.h"
#include <windows.h>

 /**
  * @brief DLL의 메인 진입점 함수
  * @param hModule DLL 모듈의 핸들 (기준 주소)
  * @param ul_reason_for_call DLL이 호출된 이유 (프로세스/스레드 생성 및 종료)
  * @param lpReserved 예약된 매개변수 (동적 로드 여부 등에 따라 다름)
  * @return BOOL 초기화 성공 여부 (TRUE: 성공, FALSE: 실패)
  */
BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    // C++의 메모리 풀이나 소켓 초기화 등은 여기에서 수행하지 않습니다.
    // (이유: 운영체제의 Loader Lock으로 인해 데드락이 발생할 위험이 있습니다.)
    // 모든 초기화는 ICommunicationChannel의 Open() 메서드에서 명시적으로 수행됩니다.

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // 프로세스 주소 공간에 DLL이 매핑될 때 1회 호출됨
        // 스레드별 초기화가 필요 없다면 DisableThreadLibraryCalls를 호출하여 성능을 향상시킬 수 있습니다.
        DisableThreadLibraryCalls(hModule);
        break;

    case DLL_THREAD_ATTACH:
        // 현재 프로세스 내에서 새로운 스레드가 생성될 때 호출됨
        break;

    case DLL_THREAD_DETACH:
        // 현재 프로세스 내에서 스레드가 정상 종료될 때 호출됨
        break;

    case DLL_PROCESS_DETACH:
        // DLL이 프로세스 주소 공간에서 해제될 때 1회 호출됨
        break;
    }

    return TRUE;
}