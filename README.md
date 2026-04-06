# 26-01-W5-ScanADC01

## 프로젝트 개요
- **MCU**: STM32F411RETx (NUCLEO-F411RE)
- **SYSCLK**: 84 MHz (HSI + PLL)
- **기능**: ADC1 Scan 모드 (3채널) + TIM3 트리거 (100Hz) + USART2 시리얼 출력

---

## printf → USART2 리다이렉트 상세 설명

### 1. 개요

임베디드 환경에서는 표준 `printf()`의 출력 대상(stdout)이 정의되어 있지 않다.  
이를 USART2로 연결하면, PC의 시리얼 터미널(PuTTY, Tera Term 등)에서 디버그 메시지를 확인할 수 있다.

### 2. 동작 원리 (호출 흐름)

```
printf("Hello\n")
  → _write()            ← syscalls.c (Newlib 시스템 콜)
    → __io_putchar()    ← Serial_printf.h (사용자 구현)
      → HAL_UART_Transmit()  ← USART2 하드웨어로 1바이트 전송
```

| 계층 | 파일 | 역할 |
|------|------|------|
| C 표준 라이브러리 | (내장) | `printf()` 호출 시 포맷 문자열을 처리하고, 각 문자를 `_write()`로 전달 |
| Newlib syscalls | `Core/Src/syscalls.c` | `_write()` 함수가 문자열의 각 바이트마다 `__io_putchar()`를 호출 |
| 사용자 구현 | `Core/Inc/Serial_printf.h` | `__io_putchar()`에서 `HAL_UART_Transmit()`으로 USART2에 1바이트 전송 |

### 3. 핵심 코드 (`Serial_printf.h`)

```c
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  // '\n'이 들어오면 '\r'을 먼저 전송 (줄바꿈 자동 변환)
  if (ch == '\n')
    HAL_UART_Transmit(&huart2, (uint8_t*) "\r", 1, 0xFFFF);
  HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF);
  return ch;
}
```

#### 컴파일러별 분기

| 컴파일러 | 매크로 | 리다이렉트 함수 |
|----------|--------|-----------------|
| **GCC (ARM)** | `__GNUC__` 정의됨 | `__io_putchar(int ch)` — Newlib의 `_write()`가 호출 |
| **ARMCC / IAR** | `__GNUC__` 미정의 | `fputc(int ch, FILE *f)` — 표준 라이브러리가 직접 호출 |

STM32CubeIDE는 **GCC (arm-none-eabi-gcc)** 를 사용하므로 `__io_putchar()`가 선택된다.

#### `\n` → `\r\n` 자동 변환

시리얼 터미널은 **`\r\n` (CR+LF)** 을 수신해야 정상적으로 줄바꿈을 표시한다.

| printf 입력 | 실제 UART 전송 | 터미널 동작 |
|-------------|---------------|-------------|
| `\n` | `\r` + `\n` | ✅ 줄바꿈 정상 |
| `\r\n` | `\r` + `\r` + `\n` | ✅ 줄바꿈 정상 (`\r` 중복은 무해) |

따라서 코드에서는 `printf("...\n")`만 사용하면 된다.

### 4. 연결 구조

| 항목 | 설정 |
|------|------|
| **UART 인스턴스** | USART2 |
| **Baud Rate** | 115200 bps |
| **Data / Stop / Parity** | 8N1 |
| **TX 핀** | PA2 |
| **RX 핀** | PA3 |
| **PC 연결** | Nucleo 보드의 ST-LINK가 USB-UART 브릿지 역할 (별도 USB-TTL 불필요) |

### 5. 사용 방법

1. `main.c`에서 헤더 포함:
   ```c
   #include "Serial_printf.h"
   ```
2. 코드 어디서든 `printf()` 사용:
   ```c
   printf("ADC CH0 = %d\n", adc_value[0]);
   ```
3. PC에서 시리얼 터미널을 **115200 / 8N1**로 열면 출력 확인 가능

---

## 변경 이력

### 2026-04-06

| 시간 | 내용 | 파일 |
|-------|------|------|
| 14:00 | `Serial_printf.h` 생성 — `printf()`를 USART2로 리다이렉트, `\n` → `\r\n` 자동 변환 포함 | `Core/Inc/Serial_printf.h` |
| 14:00 | `main.c`에 `#include "Serial_printf.h"` 추가 | `Core/Src/main.c` |
| 15:00 | ADC 3채널 Scan + TIM3 100Hz 트리거 + 500샘플 링버퍼 저장 코드 추가 | `Core/Src/main.c` |
| 15:00 | `HAL_ADC_ConvCpltCallback()` 구현 — 채널별 EOC 인터럽트로 버퍼에 저장 | `Core/Src/main.c` |
| 15:00 | 1초 주기 printf 출력 — 신규 변환 수, 누적 수, 최근 3채널 ADC 값 표시 | `Core/Src/main.c` |