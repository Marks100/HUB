# STM32_RF_HUB

[:page_with_curl:](https://marks100org.github.io/STM32_test_bed/) [![Pages](https://img.shields.io/badge/GitHub_Pages-blue)](https://marks100org.github.io/STM32_test_bed/)

## Memory usage

High-level ROM/RAM usage per image, written by the memory stats step of each build (memory_analyser_stm32). ROM is flash; all images share the same 20 KB of RAM.

<!-- MEMORY:APP:START -->
### APP

_Built 2026-11-04 from `APP.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 37,640 B | 54,520 B | 40.8% |
| RAM | 10,396 B | 10,052 B | 50.8% |

<p>
  <img src=".github/badges/memory_APP_rom.svg" alt="APP ROM (Flash): 40.8% used" width="320">
  <img src=".github/badges/memory_APP_ram.svg" alt="APP RAM: 50.8% used" width="320">
</p>
<!-- MEMORY:APP:END -->

<!-- MEMORY:FBL:START -->
### FBL

_Built 2026-11-04 from `FBL.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 23,260 B | 292 B | 98.8% |
| RAM | 5,908 B | 14,540 B | 28.9% |

<p>
  <img src=".github/badges/memory_FBL_rom.svg" alt="FBL ROM (Flash): 98.8% used" width="320">
  <img src=".github/badges/memory_FBL_ram.svg" alt="FBL RAM: 28.9% used" width="320">
</p>
<!-- MEMORY:FBL:END -->

<!-- MEMORY:BM:START -->
### BM

_Built 2026-11-04 from `BM.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 4,792 B | 4,424 B | 52.0% |
| RAM | 1,068 B | 19,380 B | 5.2% |

<p>
  <img src=".github/badges/memory_BM_rom.svg" alt="BM ROM (Flash): 52.0% used" width="320">
  <img src=".github/badges/memory_BM_ram.svg" alt="BM RAM: 5.2% used" width="320">
</p>
<!-- MEMORY:BM:END -->
