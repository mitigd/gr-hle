# Ranked seam candidates: agi

Format: **MZ** (16-bit). Functions: 210 from built-in function discovery.

Evidence is combined with identification confidence and call-site hotness into a
**replacement value**. Five high-value seams can beat fifty arbitrary ones; the shortlist
below is cross-category, not just the top of one hardware table.

**The target is function replacement, not device emulation.** A port hit only locates the
seam. Implement its intent natively (host audio, frames/palette, host clock, host input);
do not build a Sound Blaster/AdLib/VGA/PIT/keyboard-controller model.


## Replacement shortlist (best value across areas)

| # | priority | confidence | function | addr | area | value | why |
| --: | :-- | --: | :-- | :-- | :-- | --: | :-- |
| 1 | Very high | 85% | FUN_00006840 | `00006840` | input | 37.0 | direct evidence; 2 direct callers |
| 2 | Very high | 82% | FUN_00004489 | `00004489` | timing | 32.3 | direct evidence; 1 direct caller |
| 3 | Very high | 80% | FUN_00002dac | `00002dac` | video | 31.7 | direct evidence; 6 direct callers |
| 4 | Very high | 80% | FUN_00008507 | `00008507` | timing | 28.6 | direct evidence; 2 direct callers |
| 5 | Very high | 85% | FUN_00004489 | `00004489` | video | 26.4 | direct evidence; 1 direct caller |
| 6 | Very high | 85% | FUN_000087b3 | `000087b3` | timing | 26.4 | direct evidence; 1 direct caller |
| 7 | Very high | 73% | FUN_00002eb9 | `00002eb9` | video | 18.7 | direct evidence; 6 direct callers |
| 8 | Very high | 73% | FUN_00002eef | `00002eef` | video | 18.0 | direct evidence; 5 direct callers |
| 9 | High | 78% | FUN_00005f02 | `00005f02` | input | 15.7 | direct evidence; 1 direct caller |
| 10 | Medium | 40% | FUN_0000434c | `0000434c` | timing | 9.1 | propagated from callers; 5 direct callers |
| 11 | Low | 39% | FUN_000065be | `000065be` | input | 5.8 | propagated from callers; 1 direct caller |
| 12 | Medium | 48% | FUN_00008507 | `00008507` | sound | 3.7 | direct evidence; 2 direct callers |

## video (14 candidates, showing 14)

| rank | priority | confidence | function | addr | size | value | evidence |
| --: | :-- | --: | :-- | :-- | --: | --: | :-- |
| 1 | Very high | 80% | FUN_00002dac | `00002dac` | 269 | 31.7 | calls FUN_00002dac; int 10h x8 |
| 2 | Very high | 85% | FUN_00004489 | `00004489` | 161 | 26.4 | int 10h x1; port 3BAh x2 (MDA/VGA range) |
| 3 | Very high | 73% | FUN_00002eb9 | `00002eb9` | 21 | 18.7 | int 10h x1 |
| 4 | Very high | 73% | FUN_00002eef | `00002eef` | 35 | 18.0 | int 10h x1 |
| 5 | High | 73% | FUN_00002ece | `00002ece` | 33 | 17.1 | int 10h x1 |
| 6 | High | 78% | FUN_00002f4a | `00002f4a` | 61 | 15.7 | int 10h x3 |
| 7 | High | 75% | FUN_00002f87 | `00002f87` | 42 | 14.2 | int 10h x2 |
| 8 | High | 73% | FUN_00005eed | `00005eed` | 21 | 12.7 | int 10h x1 |
| 9 | Low | 36% | FUN_0000211e | `0000211e` | 388 | 5.1 | calls FUN_00002eb9; calls FUN_00002ece; calls FUN_00002eef |
| 10 | Low | 38% | FUN_0000937a | `0000937a` | 270 | 5.1 | calls FUN_00002dac; calls FUN_00002ece; calls FUN_00002eef |
| 11 | Low | 36% | FUN_0000390f | `0000390f` | 138 | 4.4 | calls FUN_00002eb9; calls FUN_00002ece; calls FUN_00002eef |
| 12 | Low | 32% | FUN_000092e7 | `000092e7` | 147 | 3.0 | calls FUN_00002eb9; calls FUN_00002eef |
| 13 | Low | 32% | FUN_00009824 | `00009824` | 412 | 2.6 | calls FUN_00002ece; calls FUN_00002eef |
| 14 | Low | 31% | FUN_00004416 | `00004416` | 51 | 1.0 | calls FUN_00004489 |

## sound (2 candidates, showing 2)

| rank | priority | confidence | function | addr | size | value | evidence |
| --: | :-- | --: | :-- | :-- | --: | --: | :-- |
| 1 | Medium | 48% | FUN_00008507 | `00008507` | 82 | 3.7 | port 61h speaker gate + PIT channel 2 |
| 2 | Low (helper) | 48% | FUN_00004329 | `00004329` | 35 | 1.6 | port 61h speaker gate + PIT channel 2 |

## timing (11 candidates, showing 11)

| rank | priority | confidence | function | addr | size | value | evidence |
| --: | :-- | --: | :-- | :-- | --: | --: | :-- |
| 1 | Very high | 82% | FUN_00004489 | `00004489` | 161 | 32.3 | calls FUN_00004329; calls leaf helper FUN_00004329; int 15h x1; int 1Ah x2; leaf helper caller |
| 2 | Very high | 80% | FUN_00008507 | `00008507` | 82 | 28.6 | port 42h PIT ch2 x2 (PIT); port 43h PIT cmd x1 (PIT) |
| 3 | Very high | 85% | FUN_000087b3 | `000087b3` | 242 | 26.4 | port 40h PIT ch0 x2 (PIT); port 43h PIT cmd x1 (PIT) |
| 4 | Low (helper) | 80% | FUN_00004329 | `00004329` | 35 | 12.3 | port 42h PIT ch2 x2 (PIT); port 43h PIT cmd x1 (PIT) |
| 5 | Medium | 40% | FUN_0000434c | `0000434c` | 53 | 9.1 | calls FUN_00004329; calls leaf helper FUN_00004329; leaf helper caller |
| 6 | Medium | 40% | FUN_00004381 | `00004381` | 149 | 8.2 | calls FUN_00004329; calls leaf helper FUN_00004329; leaf helper caller |
| 7 | Low | 40% | FUN_0000452a | `0000452a` | 367 | 7.5 | calls FUN_00004329; calls leaf helper FUN_00004329; leaf helper caller |
| 8 | Low | 40% | FUN_00003472 | `00003472` | 80 | 6.5 | calls FUN_00004329; calls leaf helper FUN_00004329; leaf helper caller |
| 9 | Low | 31% | FUN_000084d5 | `000084d5` | 50 | 2.4 | calls FUN_00008507 |
| 10 | Low | 31% | FUN_0000860b | `0000860b` | 37 | 2.1 | calls FUN_000087b3 |
| 11 | Low | 31% | FUN_0000848c | `0000848c` | 55 | 1.0 | calls FUN_00008507 |

## input (5 candidates, showing 5)

| rank | priority | confidence | function | addr | size | value | evidence |
| --: | :-- | --: | :-- | :-- | --: | --: | :-- |
| 1 | Very high | 85% | FUN_00006840 | `00006840` | 103 | 37.0 | cli/sti; int 16h x2; port 201h joystick x2 (joystick) |
| 2 | High | 78% | FUN_00005f02 | `00005f02` | 77 | 15.7 | int 16h x3 |
| 3 | Low | 39% | FUN_000065be | `000065be` | 182 | 5.8 | calls FUN_00006840; calls FUN_000068a7; calls leaf helper FUN_000068a7; leaf helper caller |
| 4 | Low (helper) | 62% | FUN_000068a7 | `000068a7` | 27 | 3.2 | port 201h joystick x1 (joystick) |
| 5 | Low | 32% | FUN_00006674 | `00006674` | 103 | 2.6 | calls FUN_00006840 |
