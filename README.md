# STM32_RF_HUB

[:page_with_curl:](https://marks100org.github.io/STM32_test_bed/) [![Pages](https://img.shields.io/badge/GitHub_Pages-blue)](https://marks100org.github.io/STM32_test_bed/)

## Memory usage

High-level ROM/RAM usage per image, written by the memory stats step of each build (memory_analyser_stm32). ROM is flash; all images share the same 20 KB of RAM.

<!-- MEMORY:APP:START -->
### APP

_Built 2026-11-04 from `APP.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 37,900 B | 54,260 B | 41.1% |
| RAM | 10,416 B | 10,032 B | 50.9% |

<p>
  <img src=".github/badges/memory_APP_rom.svg" alt="APP ROM (Flash): 41.1% used" width="320">
  <img src=".github/badges/memory_APP_ram.svg" alt="APP RAM: 50.9% used" width="320">
</p>
<!-- MEMORY:APP:END -->

<!-- MEMORY:FBL:START -->
### FBL

_Built 2026-11-04 from `FBL.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 23,508 B | 44 B | 99.8% |
| RAM | 5,928 B | 14,520 B | 29.0% |

<p>
  <img src=".github/badges/memory_FBL_rom.svg" alt="FBL ROM (Flash): 99.8% used" width="320">
  <img src=".github/badges/memory_FBL_ram.svg" alt="FBL RAM: 29.0% used" width="320">
</p>
<!-- MEMORY:FBL:END -->

<!-- MEMORY:BM:START -->
### BM

_Built 2026-11-04 from `BM.map`_

| Region | Used | Free | Used % |
|---|---:|---:|---:|
| ROM (Flash) | 5,388 B | 3,828 B | 58.5% |
| RAM | 1,088 B | 19,360 B | 5.3% |

<p>
  <img src=".github/badges/memory_BM_rom.svg" alt="BM ROM (Flash): 58.5% used" width="320">
  <img src=".github/badges/memory_BM_ram.svg" alt="BM RAM: 5.3% used" width="320">
</p>
<!-- MEMORY:BM:END -->
