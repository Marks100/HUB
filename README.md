# STM32_RF_HUB

[:page_with_curl:](https://marks100org.github.io/STM32_test_bed/) [![Pages](https://img.shields.io/badge/GitHub_Pages-blue)](https://marks100org.github.io/STM32_test_bed/)

<!-- MEMORY_REPORT:START -->
## Memory usage

Generated from the linker `.map` files by `Tool_cfg/MAP_REPORT/map_report.py`. All images use the same 20 KB of RAM; heap and stack are the linker reservations.

| Project | Flash used | Flash % | RAM used | RAM % | Map built |
|---|---|---|---|---|---|
| APP | 73,668 / 92,160 B | 79.9% | 16,296 / 20,480 B | 79.6% | 2026-10-18 |
| BM | 4,792 / 9,216 B | 52.0% | 1,088 / 20,480 B | 5.3% | 2026-10-16 |
| FBL | 23,204 / 23,552 B | 98.5% | 6,684 / 20,480 B | 32.6% | 2026-10-18 |

<details><summary>APP breakdown</summary>

```mermaid
pie showData title APP flash (bytes)
    "Header and vectors" : 524
    ".text" : 64664
    ".rodata" : 8036
    ".data init" : 444
    "Free" : 18492
```

```mermaid
pie showData title APP RAM (bytes)
    "Shared RAM" : 32
    ".data" : 444
    ".bss" : 14284
    "Heap (reserved)" : 512
    "Stack (reserved)" : 1024
    "Free" : 4184
```

</details>

<details><summary>BM breakdown</summary>

```mermaid
pie showData title BM flash (bytes)
    "Header and vectors" : 268
    ".text" : 4036
    ".rodata" : 468
    ".data init" : 20
    "Free" : 4424
```

```mermaid
pie showData title BM RAM (bytes)
    "Shared RAM" : 32
    ".data" : 20
    ".bss" : 12
    "Stack (reserved)" : 1024
    "Free" : 19392
```

</details>

<details><summary>FBL breakdown</summary>

```mermaid
pie showData title FBL flash (bytes)
    "Header and vectors" : 524
    ".text" : 21388
    ".rodata" : 1044
    ".data init" : 248
    "Free" : 348
```

```mermaid
pie showData title FBL RAM (bytes)
    "Shared RAM" : 32
    ".data" : 248
    ".bss" : 5380
    "Stack (reserved)" : 1024
    "Free" : 13796
```

</details>
<!-- MEMORY_REPORT:END -->
