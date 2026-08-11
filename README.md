# Radix-2 Online Multiplier

A modular C++ hardware-level simulation of a Radix-2 Online Multiplier. This project implements Most-Significant Digit First (MSDF) digit-serial arithmetic, based on the architectures described in Chapter 9 of *Digital Arithmetic* by Milos D. Ercegovac and Tomas Lang.

## Features

* **Convert-and-Append (CA) Registers:** Hardware-accurate On-the-Fly Conversion (OTFC) of redundant digits without carry-propagate addition.
* **Carry-Save Architecture:** Utilizes layered `[4:2]` compressors (Full Adders) to prevent carry propagation delay.
* **SELM & M-Blocks:** Implements exact Boolean switching expressions for digit selection ($\{-1, 0, 1\}$) and XOR-based subtraction with wired left-shifts.
* **Cycle-Accurate Tracing:** Generates terminal outputs that perfectly mirror the hardware bus states and standard textbook execution traces.
* **Template-Based Design:** Allows compile-time configuration of the multiplier's bit-width for strict hardware accuracy.


## Output

The simulation will output a cycle-by-cycle trace of the hardware registers ($X$, $Y$, $v[j]$, $w[j+1]$, and $p_{out}$). At the end of the execution, it compares the accumulated redundant decimal output against the exact mathematical product of the input vectors to verify the truncation error bound.

## References

* Ercegovac, M. D., & Lang, T. (2003). *Digital Arithmetic*. Morgan Kaufmann. (Specifically Chapter 9: Digit-Serial Arithmetic).
<img width="1269" height="979" alt="Screenshot from 2026-08-11 22-40-46" src="https://github.com/user-attachments/assets/4e1a401e-a55b-419c-aefe-873b2df2ad77" />
