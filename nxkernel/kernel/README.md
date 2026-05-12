# Nyxis Kernel Components

## English

This directory contains the core kernel components for the Nyxis operating system.

### Paging (`paging/`)

The paging subsystem provides virtual memory management through x86 paging.

- `paging_init()`: Initializes the paging system with a basic identity map.
- `paging_map_page()`: Maps a physical page to a virtual address.
- `paging_unmap_page()`: Unmaps a virtual page.
- `paging_enable()`: Enables paging by setting CR0 and CR3.
- `paging_disable()`: Disables paging.

All functions return `Nstatus` for error handling.

### Process (`process/`)

The process subsystem handles process creation and context switching.

- `process_init()`: Initializes the process system with an idle process.
- `process_create()`: Creates a new process with given entry point and stack.
- `process_switch()`: Switches context to a different process.
- `process_terminate()`: Terminates a process by PID.

All functions return `Nstatus` for error handling.

## 한국어

이 디렉토리에는 Nyxis 운영체제의 핵심 커널 컴포넌트가 포함되어 있습니다.

### 페이징 (`paging/`)

페이징 서브시스템은 x86 페이징을 통해 가상 메모리 관리를 제공합니다.

- `paging_init()`: 기본 아이덴티티 맵으로 페이징 시스템을 초기화합니다.
- `paging_map_page()`: 물리 페이지를 가상 주소에 매핑합니다.
- `paging_unmap_page()`: 가상 페이지를 언매핑합니다.
- `paging_enable()`: CR0과 CR3을 설정하여 페이징을 활성화합니다.
- `paging_disable()`: 페이징을 비활성화합니다.

모든 함수는 오류 처리를 위해 `Nstatus`를 반환합니다.

### 프로세스 (`process/`)

프로세스 서브시스템은 프로세스 생성과 컨텍스트 스위칭을 처리합니다.

- `process_init()`: 유휴 프로세스로 프로세스 시스템을 초기화합니다.
- `process_create()`: 주어진 진입점과 스택으로 새 프로세스를 생성합니다.
- `process_switch()`: 다른 프로세스로 컨텍스트를 전환합니다.
- `process_terminate()`: PID로 프로세스를 종료합니다.

모든 함수는 오류 처리를 위해 `Nstatus`를 반환합니다.