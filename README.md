# 26-01-W5-ScanADC01

## 프로젝트 개요
- **MCU**: STM32F411RETx (NUCLEO-F411RE)
- **SYSCLK**: 84 MHz (HSI + PLL)
- **기능**: ADC1 Scan 모드 (3채널) + TIM3 트리거 (100Hz) + USART2 시리얼 출력

---

## ADC 모듈 설정 과정 (상세)

이 프로젝트는 **TIM3의 TRGO(Update 이벤트)** 로 ADC1 정규 변환을 시작하고, 한 번의 트리거마다
`CH0 -> CH1 -> CH4` 순서로 3채널을 스캔합니다.

### 1) 기본 설계 의도
- 샘플링 주기: 100 Hz (10 ms)
- 채널 수: 3개 (ADC1 IN0, IN1, IN4)
- 데이터 수집 방식: 인터럽트 기반 (`HAL_ADC_Start_IT` + `HAL_ADC_ConvCpltCallback`)
- 버퍼링: 채널별 링버퍼(500샘플)

### 2) ADC1 전역 설정 (`MX_ADC1_Init`)
아래 값으로 ADC 동작 모드를 고정합니다.

| 항목 | 설정값 | 이유 |
|------|--------|------|
| Clock Prescaler | `ADC_CLOCK_SYNC_PCLK_DIV4` | ADC 클럭을 안정 범위로 낮춰 샘플링 여유 확보 |
| Resolution | `ADC_RESOLUTION_12B` | 0~4095(12-bit) 정밀도 사용 |
| ScanConvMode | `ENABLE` | 1회 트리거에 다채널 순차 변환 |
| ContinuousConvMode | `DISABLE` | 타이머 트리거 기반 주기 샘플링을 위해 연속 모드 비활성 |
| ExternalTrigConvEdge | `ADC_EXTERNALTRIGCONVEDGE_RISING` | TRGO 상승엣지에서 변환 시작 |
| ExternalTrigConv | `ADC_EXTERNALTRIGCONV_T3_TRGO` | TIM3 TRGO를 변환 시작 신호로 사용 |
| NbrOfConversion | `3` | 채널 3개 스캔 |
| EOCSelection | `ADC_EOC_SINGLE_CONV` | 채널 1개 변환 완료마다 인터럽트 발생 |
| DMAContinuousRequests | `DISABLE` | 본 예제는 DMA 대신 인터럽트로 저장 |

### 3) 정규 채널 시퀀스 설정
`HAL_ADC_ConfigChannel()`을 3회 호출해 순서를 만듭니다.

| 순서(Rank) | 채널 | 샘플링 시간 |
|------------|------|-------------|
| 1 | `ADC_CHANNEL_0` | `ADC_SAMPLETIME_112CYCLES` |
| 2 | `ADC_CHANNEL_1` | `ADC_SAMPLETIME_112CYCLES` |
| 3 | `ADC_CHANNEL_4` | `ADC_SAMPLETIME_112CYCLES` |

> 핵심: Rank 순서가 곧 콜백에서 들어오는 데이터 순서입니다.

### 4) 트리거 타이머(TIM3) 설정
`MX_TIM3_Init()`에서 TRGO를 Update 이벤트로 설정합니다.

- Prescaler: `8400 - 1`
- Period: `99`
- MasterOutputTrigger: `TIM_TRGO_UPDATE`

SYSCLK/APB 설정 기준으로 타이머 업데이트 주기를 100 Hz로 맞춰, ADC가 10 ms마다 1회 스캔을 시작합니다.

### 5) 런타임 시작 순서 (`main`)
초기화 후 시작 순서는 반드시 아래와 같습니다.

1. `HAL_TIM_Base_Start(&htim3)`
2. `HAL_ADC_Start_IT(&hadc1)`

이 순서로 TIM3가 주기 트리거를 만들고, ADC는 트리거를 기다리다가 인터럽트로 결과를 전달합니다.

### 6) 변환 완료 콜백 처리 로직
`HAL_ADC_ConvCpltCallback()`에서 채널별 데이터를 링버퍼로 저장합니다.

- `EOC_SINGLE_CONV`이므로 한 채널 변환마다 콜백 1회 호출
- `adc_ch_idx`를 0,1,2로 증가시키며 `adc_buf[ch][idx]` 저장
- 3채널 저장이 끝나면(`adc_ch_idx >= 3`):
  - `adc_buf_idx` 증가 (500 도달 시 0으로 순환)
  - `adc_conv_count` 1 증가 ("3채널 1세트 완료" 카운트)

즉, `adc_conv_count`는 "샘플 포인트 개수"이고, 각 샘플 포인트에는 CH0/CH1/CH4가 한 세트로 들어갑니다.

### 7) 설정 검증 체크포인트
- 시리얼 로그에서 `new` 값이 초당 약 100인지 확인
- `CH0/CH1/CH4` 값이 모두 갱신되는지 확인
- `idx`가 0~499 범위에서 순환하는지 확인

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
| 15:00 | ADC 모듈 설정 과정 (상세) 섹션 추가 — CubeMX/HAL 초기화 코드 기준 단계별 설명 | `README.md` |
