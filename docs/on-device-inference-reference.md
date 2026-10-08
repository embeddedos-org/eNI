# On-device inference: the reference-design evidence pack

**Status:** reference (2026-10-08). This document collects the
independent existence proofs behind eNI's on-device neural claims. When
eNI says "commodity MCUs run neural models in-domain," these are the
receipts -- and the safety architecture one of them demonstrates is the
pattern eNI adopts for stimulation outputs.

## 1. Oido: commodity MCUs beat laptop baselines in-domain

**Oido** (October 2026) is open-source full-sentence speech recognition
on a **$5 ESP32-S3**: int8, WER 3.7%/8.2% -- *fewer errors than Whisper
tiny running on a laptop* -- and 2.3-2.6x better than Espressif's own
MultiNet7 on the same chip.

- https://dev.to/danivs10/oido-open-source-speech-recognition-on-a-5-chip-3n9d

The lesson for eNI: in-domain models on commodity silicon beat
datacenter baselines. eNI's neural-signal models should be evaluated the
same way -- against the task, on the target MCU, not against a
cloud-model leaderboard.

## 2. esp32-gpio-llm: the safety architecture is the lesson

A **312K-parameter transformer** turns English into GPIO actions entirely
offline (1.2MB flash, 302KB PSRAM). The model is not the interesting
part -- the *safety architecture* is:

- The model **never touches hardware directly**.
- Firmware holds a **GPIO allowlist** and validates every parameter
  (pin numbers, intervals) before acting.

That is Track 1's **capability-scoped inference in miniature**: the
untrusted model proposes, the trusted firmware disposes, and the
allowlist is the capability boundary.

- https://www.opensourceforu.com/2026/09/esp32-s3-runs-open-source-gpio-language-model/

**eNI adoption:** neural-signal models that drive stimulation outputs
get the same treatment. The model proposes a stimulation pattern; eNI
firmware validates it against a per-channel allowlist (amplitude,
pulse width, frequency bounds) before any electrode is driven. The
allowlist -- not the model -- is the safety case.

## 3. Bare-metal LLM on NXP FRDM-MCXN947

A 289M-parameter LLM runs bare-metal on NXP's FRDM-MCXN947, and a
diffusion model runs at 64x64 grayscale in 3.36MB on the STM32N657
(Ethos-U55). These are second and third existence proofs that the
tinyML tier is a parts list, not a roadmap -- and the FRDM-MCXN947 is
eNI's second reference target after the Alif B1.

## 4. The BitNet data point

Seven $4 ESP32-S3 chips run a 0.4B LLM in an SPI daisy-chain ($28
total) with 1.58-bit ternary weights (September 2026). For eNI's
model-serving story this sets the floor: if a general LLM fits in this
envelope, eNI's narrower neural models fit with room to spare.

- https://byteiota.com/esp32-bitnet-llm-cluster/

## How to cite this pack

- "In-domain MCU inference" claims -> Oido (section 1).
- "Safe neural actuation" claims -> esp32-gpio-llm allowlist pattern (section 2).
- "Fits on commodity silicon" claims -> FRDM-MCXN947 / STM32N657 / BitNet (sections 3-4).
