# STM32_RF_HUB

[:page_with_curl:](https://marks100org.github.io/STM32_test_bed/) [![Pages](https://img.shields.io/badge/GitHub_Pages-blue)](https://marks100org.github.io/STM32_test_bed/)

## Memory usage

High-level ROM/RAM usage per image, written by the memory stats step of each build (memory_analyser_stm32). ROM is flash; all images share the same 20 KB of RAM.

<!-- MEMORY:APP:START -->
### APP

_Built 2026-11-23 from `APP.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 40,772 B | 50,364 B | 44.7% |
| RAM | 8,348 B | 12,100 B | 40.8% |

<p>
  <img src=".github/badges/memory_APP_rom.svg" alt="APP ROM (Flash): 44.7% used" width="320">
  <img src=".github/badges/memory_APP_ram.svg" alt="APP RAM: 40.8% used" width="320">
</p>
<!-- MEMORY:APP:END -->

<!-- MEMORY:FBL:START -->
### FBL

_Built 2026-11-26 from `FBL.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 21,188 B | 3,388 B | 86.2% |
| RAM | 6,192 B | 14,256 B | 30.3% |

<p>
  <img src=".github/badges/memory_FBL_rom.svg" alt="FBL ROM (Flash): 86.2% used" width="320">
  <img src=".github/badges/memory_FBL_ram.svg" alt="FBL RAM: 30.3% used" width="320">
</p>
<!-- MEMORY:FBL:END -->

<!-- MEMORY:BM:START -->
### BM

_Built 2026-11-26 from `BM.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 5,176 B | 3,016 B | 63.2% |
| RAM | 1,088 B | 19,360 B | 5.3% |

<p>
  <img src=".github/badges/memory_BM_rom.svg" alt="BM ROM (Flash): 63.2% used" width="320">
  <img src=".github/badges/memory_BM_ram.svg" alt="BM RAM: 5.3% used" width="320">
</p>
<!-- MEMORY:BM:END -->
