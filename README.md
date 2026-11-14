# STM32_RF_HUB

[:page_with_curl:](https://marks100org.github.io/STM32_test_bed/) [![Pages](https://img.shields.io/badge/GitHub_Pages-blue)](https://marks100org.github.io/STM32_test_bed/)

## Memory usage

High-level ROM/RAM usage per image, written by the memory stats step of each build (memory_analyser_stm32). ROM is flash; all images share the same 20 KB of RAM.

<!-- MEMORY:APP:START -->
### APP

_Built 2026-11-14 from `APP.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 39,780 B | 51,356 B | 43.6% |
| RAM | 10,804 B | 9,644 B | 52.8% |

<p>
  <img src=".github/badges/memory_APP_rom.svg" alt="APP ROM (Flash): 43.6% used" width="320">
  <img src=".github/badges/memory_APP_ram.svg" alt="APP RAM: 52.8% used" width="320">
</p>
<!-- MEMORY:APP:END -->

<!-- MEMORY:FBL:START -->
### FBL

_Built 2026-11-14 from `FBL.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 20,856 B | 3,720 B | 84.9% |
| RAM | 6,176 B | 14,272 B | 30.2% |

<p>
  <img src=".github/badges/memory_FBL_rom.svg" alt="FBL ROM (Flash): 84.9% used" width="320">
  <img src=".github/badges/memory_FBL_ram.svg" alt="FBL RAM: 30.2% used" width="320">
</p>
<!-- MEMORY:FBL:END -->

<!-- MEMORY:BM:START -->
### BM

_Built 2026-11-14 from `BM.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 5,388 B | 2,804 B | 65.8% |
| RAM | 1,088 B | 19,360 B | 5.3% |

<p>
  <img src=".github/badges/memory_BM_rom.svg" alt="BM ROM (Flash): 65.8% used" width="320">
  <img src=".github/badges/memory_BM_ram.svg" alt="BM RAM: 5.3% used" width="320">
</p>
<!-- MEMORY:BM:END -->
